/*
========================================================================
Project: OpenRFStack
Author:  Brendan Michaud
Year:    2026
Part of OpenRFStack (https://github.com/OpenRFStack)

Licensed under the Personal Use License.
Do not use for commercial, organizational, or military purposes.
Contact author for permission: https://github.com/OpenRFStack
========================================================================
*/
/**
 * @file P25Monitor.cpp
 * @brief P25 control channel monitor — ID-only (no audio output).
 */
#include "P25Monitor.hpp"

#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/reconnect_options.hpp>
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
        if (!mon_.cfg_.broker.username.empty()) {
            // Without PLAIN + insecure-mechs, proton never negotiates credentials
            // onto the wire — Artemis sees an anonymous connection and rejects it
            // (AMQ229031). Same fix as SpeechApp::AmqpListener and AlertPublisher.
            copts.sasl_allowed_mechs("PLAIN");
            copts.sasl_allow_insecure_mechs(true);
            copts.user(mon_.cfg_.broker.username);
        } else {
            copts.sasl_allowed_mechs("ANONYMOUS");
        }
        if (!mon_.cfg_.broker.password.empty())
            copts.password(mon_.cfg_.broker.password);
        // Without this, a failed initial connection (Artemis not up yet) is
        // permanent -- the P25 control-channel monitor would silently never
        // receive rf.detections again. Same fix as AlertPublisher in this repo.
        proton::reconnect_options ropts;
        ropts.delay(proton::duration(2000));
        ropts.max_delay(proton::duration(30000));
        ropts.max_attempts(0);
        copts.reconnect(ropts);
        c.connect(mon_.cfg_.broker.url, copts);
    }

    void on_connection_open(proton::connection& conn) override {
        conn.open_receiver("rf.detections",
            proton::receiver_options().source(
                proton::source_options().address("rf.detections")));
        sender_ = conn.open_sender(mon_.p25cfg_.grant_topic);
        // Store the work queue so publish_grant() can post sender_.send()
        // back onto the reactor thread from the decode worker thread.
        sender_wq_.store(&sender_.work_queue());
        spdlog::info("[P25Monitor] connected → {} | grants → {}",
                     mon_.cfg_.broker.url, mon_.p25cfg_.grant_topic);
    }

    void on_message(proton::delivery& d, proton::message& m) override {
        // Quick filter on the reactor thread (no blocking), then enqueue
        // center_freq for the decode worker thread.  Previously this called
        // fetcher_.collect() directly from on_message, blocking the proton
        // event loop for 3–13 s and causing AMQP heartbeat timeouts.
        try {
            auto j = json::parse(m.body().get<std::string>());
            if (j.value("msg_type", "") == "RF_DETECTION") {
                double cf = j.value("center_freq_hz", 0.0);
                if (cf > 0) {
                    bool freq_ok = true;
                    if (mon_.p25cfg_.control_freq_hz > 0)
                        freq_ok = std::abs(cf - mon_.p25cfg_.control_freq_hz) <= 25000.0;
                    if (freq_ok) {
                        double bw = j.value("bandwidth_hz", 0.0);
                        if (bw <= 0 || (bw >= 8000.0 && bw <= 20000.0)) {
                            std::lock_guard lk(mon_.decode_mu_);
                            mon_.decode_queue_.push_back(cf);
                            mon_.decode_cv_.notify_one();
                        }
                    }
                }
            }
        } catch (...) {}
        d.accept();
    }

    void on_transport_error(proton::transport&) override {
        sender_wq_.store(nullptr);
    }

    void on_sendable(proton::sender&) override {}

    proton::sender                   sender_;
    std::atomic<proton::work_queue*> sender_wq_{nullptr};
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
    handler_ = new Handler(*this);
    pub_container_ = std::make_unique<proton::container>(*handler_);
    pub_thread_ = std::thread([this]{ pub_container_->run(); });

    decode_running_ = true;
    decode_thread_ = std::thread([this]{
        while (decode_running_) {
            double freq_hz = 0.0;
            {
                std::unique_lock lk(decode_mu_);
                decode_cv_.wait(lk, [this]{
                    return !decode_queue_.empty() || !decode_running_;
                });
                if (!decode_running_ && decode_queue_.empty()) break;
                freq_hz = decode_queue_.front();
                decode_queue_.pop_front();
            }
            if (freq_hz > 0) decode_control_channel(freq_hz);
        }
    });

    spdlog::info("[P25Monitor] started (control_freq={:.4f} MHz)",
                 p25cfg_.control_freq_hz / 1e6);
}

void P25Monitor::stop() {
    decode_running_ = false;
    decode_cv_.notify_all();
    if (decode_thread_.joinable()) decode_thread_.join();

    if (pub_container_) {
        pub_container_->stop();
        if (pub_thread_.joinable()) pub_thread_.join();
        pub_container_.reset();
        delete handler_;
        handler_ = nullptr;
    }
}

// ── Detection processing ───────────────────────────────────────────────────────

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

    std::string body = j.dump();
    spdlog::debug("[P25Monitor] publishing grant: {}", body);

    // publish_grant is now called from the decode worker thread, not the
    // reactor thread.  sender_.send() must run on the reactor thread, so we
    // post via sender_wq_ (stored atomically in on_connection_open).
    if (handler_) {
        if (auto* wq = handler_->sender_wq_.load()) {
            proton::message msg(body);
            msg.content_type("application/json");
            wq->add([this, msg]() mutable {
                if (handler_ && handler_->sender_) handler_->sender_.send(msg);
            });
        }
    }
}

} // namespace demod

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
