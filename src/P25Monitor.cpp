/**
 * @file P25Monitor.cpp
 * @brief P25 control channel monitor — ID-only (no audio output).
 */
#include "P25Monitor.hpp"

#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/receiver_options.hpp>
#include <proton/source_options.hpp>
#include <proton/sender.hpp>

#include <complex>
#include <vector>
#include <cmath>
#include <chrono>

using json = nlohmann::json;

namespace demod {

// ── AMQP handler for rf.detections subscription ───────────────────────────────

class P25Monitor::Handler : public proton::messaging_handler {
public:
    Handler(P25Monitor& mon) : mon_(mon) {}

    void on_container_start(proton::container& c) override {
        proton::connection_options copts;
        if (!mon_.cfg_.broker.username.empty())
            copts.user(mon_.cfg_.broker.username);
        if (!mon_.cfg_.broker.password.empty())
            copts.password(mon_.cfg_.broker.password);
        c.connect(mon_.cfg_.broker.url, copts);
    }

    void on_connection_open(proton::connection& conn) override {
        conn.open_receiver("rf.detections",
            proton::receiver_options().source(
                proton::source_options().address("rf.detections")));
        sender_ = conn.open_sender(mon_.p25cfg_.grant_topic);
        spdlog::info("[P25Monitor] connected → {} | grants → {}",
                     mon_.cfg_.broker.url, mon_.p25cfg_.grant_topic);
    }

    void on_message(proton::delivery& d, proton::message& m) override {
        try {
            mon_.process_detection(json::parse(m.body().get<std::string>()));
        } catch (...) {}
        d.accept();
    }

    void on_sendable(proton::sender&) override {}

    proton::sender sender_;
    P25Monitor& mon_;
};

// ── Construction / destruction ────────────────────────────────────────────────

P25Monitor::P25Monitor(const AppConfig& cfg, const P25Config& p25cfg,
                       IqFetcher& fetcher)
    : cfg_(cfg), p25cfg_(p25cfg), fetcher_(fetcher)
{
    c4fm_    = std::make_unique<p25::C4FmDemod>(p25cfg_.sample_rate_hz);
    decoder_ = std::make_unique<p25::P25ControlDecoder>(
        [this](const p25::ChannelGrant& g){ on_grant(g); },
        p25cfg_.tg_whitelist
    );
}

P25Monitor::~P25Monitor() { stop(); }

void P25Monitor::start() {
    auto* h = new Handler(*this);
    pub_container_ = std::make_unique<proton::container>(*h);
    pub_thread_ = std::thread([this]{ pub_container_->run(); });
    spdlog::info("[P25Monitor] started (control_freq={:.4f} MHz)",
                 p25cfg_.control_freq_hz / 1e6);
}

void P25Monitor::stop() {
    if (pub_container_) {
        pub_container_->stop();
        if (pub_thread_.joinable()) pub_thread_.join();
        pub_container_.reset();
    }
}

// ── Detection processing ───────────────────────────────────────────────────────

void P25Monitor::process_detection(const json& det) {
    if (det.value("msg_type", "") != "RF_DETECTION") return;

    double cf = det.value("center_freq_hz", 0.0);
    if (cf <= 0) return;

    // If control_freq_hz configured: only process near that frequency
    if (p25cfg_.control_freq_hz > 0) {
        double diff = std::abs(cf - p25cfg_.control_freq_hz);
        if (diff > 25000.0) return;  // >25 kHz away — not our control channel
    }

    // Check if modulation looks like P25 (FSK-like, ~12.5 kHz BW)
    double bw = det.value("bandwidth_hz", 0.0);
    if (bw > 0 && (bw < 8000.0 || bw > 20000.0)) return;

    spdlog::debug("[P25Monitor] candidate control channel at {:.4f} MHz", cf / 1e6);
    decode_control_channel(cf);
}

// ── Control channel decoding ──────────────────────────────────────────────────

void P25Monitor::decode_control_channel(double freq_hz) {
    // Fetch IQ from the controller
    auto sr_qty = au::hertz(p25cfg_.sample_rate_hz);
    auto bw_qty = au::hertz(12500.0);
    auto dur_qty = au::seconds(p25cfg_.capture_s);

    auto samples = fetcher_.collect(
        au::hertz(freq_hz),
        bw_qty,
        sr_qty,
        dur_qty,
        "p25-ctrl");

    if (samples.empty()) {
        spdlog::warn("[P25Monitor] IQ fetch returned empty");
        return;
    }

    // Demodulate C4FM → dibits
    auto dibits = c4fm_->process(samples);
    if (dibits.empty()) return;

    spdlog::debug("[P25Monitor] C4FM: {} symbols from {} IQ samples",
                  dibits.size(), samples.size());

    // Feed to TSBK decoder
    decoder_->feed(dibits);
}

// ── Grant handling ────────────────────────────────────────────────────────────

void P25Monitor::on_grant(const p25::ChannelGrant& grant) {
    if (grant.freq_hz <= 0) {
        spdlog::warn("[P25Monitor] grant TG={} — frequency unresolved "
                     "(IDEN_UP not yet received)", grant.talk_group);
        return;
    }

    spdlog::info("[P25Monitor] GRANT TG={} freq={:.4f}MHz enc={} emerg={}",
                 grant.talk_group, grant.freq_hz / 1e6,
                 grant.encrypted, grant.emergency);

    // Track grant
    {
        std::lock_guard lk(grant_mu_);
        active_grants_[grant.talk_group] = grant.freq_hz;
    }

    publish_grant(grant);
}

void P25Monitor::publish_grant(const p25::ChannelGrant& grant) {
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    json j;
    j["msg_type"]     = "P25_CHANNEL_GRANT";
    j["schema_version"] = "1.0";
    j["timestamp_ms"] = now_ms;
    j["talk_group"]   = grant.talk_group;
    j["source_id"]    = grant.source_id;
    j["freq_hz"]      = grant.freq_hz;
    j["channel_iden"] = grant.channel_iden;
    j["channel_num"]  = grant.channel_num;
    j["encrypted"]    = grant.encrypted;
    j["alg_id"]       = static_cast<int>(grant.alg_id);
    j["alg_name"]     = grant.alg_name();
    j["key_id"]       = grant.key_id;
    j["emergency"]    = grant.emergency;
    j["site"] = {
        {"wacn",    decoder_->site_info().wacn},
        {"sys_id",  decoder_->site_info().sys_id},
        {"rfss_id", decoder_->site_info().rfss_id},
        {"site_id", decoder_->site_info().site_id},
    };

    // Published to rf.p25.grants — future subscribers retune SDR to freq_hz
    spdlog::debug("[P25Monitor] publishing grant JSON");
    // Note: actual AMQP publish requires access to the proton sender in Handler.
    // For now, log the JSON — full publish wired via shared sender ref in future iteration.
    spdlog::info("[P25Monitor] {}", j.dump());
}

} // namespace demod
