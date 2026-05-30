#include "DemodService.hpp"
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

#include <proton/container.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/reconnect_options.hpp>
#include <proton/sender.hpp>
#include <proton/receiver.hpp>
#include <proton/delivery.hpp>
#include <proton/work_queue.hpp>
#include <proton/transport.hpp>

#include <nlohmann/json.hpp>
#include <sdr/Base64.hpp>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

namespace demod {

using json = nlohmann::json;

// ── WAV writer ────────────────────────────────────────────────────────────────

static void writeWav(const std::string& path, const std::vector<float>& pcm,
                     int sample_rate)
{
    // Convert float32 → int16
    std::vector<int16_t> samples(pcm.size());
    for (size_t i = 0; i < pcm.size(); ++i)
        samples[i] = static_cast<int16_t>(
            std::clamp(pcm[i], -1.0f, 1.0f) * 32767.0f);

    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot create WAV: " + path);

    uint32_t data_size = static_cast<uint32_t>(samples.size() * 2);
    uint32_t file_size = 36 + data_size;
    uint32_t sr = static_cast<uint32_t>(sample_rate);
    uint32_t byte_rate = sr * 2;
    uint16_t block_align = 2;
    uint16_t bits_per_sample = 16;
    uint16_t num_channels = 1;
    uint16_t audio_fmt = 1;  // PCM

    auto w4 = [&](uint32_t v){ f.write(reinterpret_cast<const char*>(&v), 4); };
    auto w2 = [&](uint16_t v){ f.write(reinterpret_cast<const char*>(&v), 2); };

    f.write("RIFF", 4); w4(file_size); f.write("WAVE", 4);
    f.write("fmt ", 4); w4(16); w2(audio_fmt); w2(num_channels);
    w4(sr); w4(byte_rate); w2(block_align); w2(bits_per_sample);
    f.write("data", 4); w4(data_size);
    f.write(reinterpret_cast<const char*>(samples.data()), data_size);
}

// ── AMQP publish handler ──────────────────────────────────────────────────────

class ServiceAmqpHandler : public proton::messaging_handler {
public:
    ServiceAmqpHandler(const BrokerConfig& cfg,
                       std::function<void(const PendingDemod&)> on_result,
                       std::function<void(const std::string&, const PendingDemod&)> on_start_stream,
                       std::function<void(const std::string&)> on_stop)
        : cfg_(cfg)
        , on_result_(std::move(on_result))
        , on_start_stream_(std::move(on_start_stream))
        , on_stop_(std::move(on_stop))
    {}

    void on_container_start(proton::container& c) override {
        proton::connection_options opts;
        if (!cfg_.username.empty()) {
            opts.sasl_allowed_mechs("PLAIN");
            opts.sasl_allow_insecure_mechs(true);
            opts.user(cfg_.username).password(cfg_.password);
        } else {
            opts.sasl_allowed_mechs("ANONYMOUS");
        }
        proton::reconnect_options ropts;
        ropts.delay(proton::duration(2000));
        ropts.max_delay(proton::duration(30000));
        ropts.max_attempts(0);
        opts.reconnect(ropts);
        c.connect(cfg_.url, opts);
    }

    void on_connection_open(proton::connection& c) override {
        c.open_receiver(cfg_.demod_request_queue);
        sender_ = c.open_sender(cfg_.demod_topic);
    }

    void on_sender_open(proton::sender& s) override {
        sender_ = s;
        wq_     = &s.work_queue();
        std::lock_guard lk(mu_);
        ready_ = true;
        ready_cv_.notify_all();
    }

    void on_message(proton::delivery& d, proton::message& m) override {
        d.accept();
        try {
            std::string body = proton::get<std::string>(m.body());
            auto j = json::parse(body);
            std::string msg_type = j.value("msg_type", "");

            if (msg_type == "STOP_DEMOD_STREAM") {
                std::string sid = j.value("stream_id", std::string(""));
                if (!sid.empty() && on_stop_) on_stop_(sid);
                return;
            }

            // DEMOD_REQUEST and START_DEMOD_STREAM share the same signal fields
            if (msg_type != "DEMOD_REQUEST" && msg_type != "START_DEMOD_STREAM") return;

            PendingDemod p;
            p.modulation  = j.value("modulation", std::string(""));
            p.center_freq = au::hertz(j.value("center_freq_hz",  0.0));
            p.bandwidth   = au::hertz(j.value("bandwidth_hz",    0.0));
            p.symbol_rate = au::hertz(j.value("symbol_rate_sps", 0.0));
            p.confidence  = (float)j.value("confidence", 0.0);
            p.timestamp = au::seconds(j.value("timestamp_ms", (int64_t)0) / 1000.0);

            if (p.modulation.empty() || p.center_freq <= au::hertz(0.0)) return;

            if (msg_type == "START_DEMOD_STREAM") {
                std::string sid = j.value("stream_id", std::string(""));
                if (!sid.empty() && on_start_stream_) on_start_stream_(sid, p);
            } else {
                if (on_result_) on_result_(p);
            }
        } catch (const std::exception& ex) {
            spdlog::debug("DemodService: parse error: {}", ex.what());
        }
    }

    void publish(const std::string& body) {
        std::unique_lock lk(mu_);
        if (!ready_cv_.wait_for(lk, std::chrono::seconds(5),
                                [this]{ return ready_; })) return;
        if (!wq_) return;
        std::string b = body;
        wq_->add([this, b]() mutable {
            if (sender_ && sender_.credit() > 0) {
                proton::message msg;
                msg.body(b);
                msg.content_type("application/json");
                msg.durable(false);
                sender_.send(msg);
            }
        });
    }

    void close() {
        if (wq_) wq_->add([this]{ sender_.connection().close(); });
    }

    void on_transport_error(proton::transport& t) override {
        spdlog::warn("DemodService: transport error: {}", t.error().what());
    }
    void on_connection_error(proton::connection& c) override {
        spdlog::warn("DemodService: connection error: {}", c.error().what());
    }

private:
    BrokerConfig cfg_;
    std::function<void(const PendingDemod&)>              on_result_;
    std::function<void(const std::string&, const PendingDemod&)> on_start_stream_;
    std::function<void(const std::string&)>               on_stop_;
    proton::sender sender_;
    proton::work_queue* wq_{nullptr};
    std::mutex mu_;
    std::condition_variable ready_cv_;
    bool ready_{false};
};

// ── DemodService ──────────────────────────────────────────────────────────────

DemodService::DemodService(const AppConfig& cfg)
    : cfg_(cfg)
    , fetcher_(cfg.broker, cfg.local_ip, cfg.engine.rank)
    , router_(cfg, fetcher_, [this](const DemodResult& r){ publishResult(r); })
{}

DemodService::~DemodService() { stop(); }

void DemodService::start() {
    if (running_.exchange(true)) return;
    ::mkdir(cfg_.output.output_dir.c_str(), 0755);
    worker_thread_ = std::thread(&DemodService::workerLoop, this);
    sub_thread_    = std::thread(&DemodService::subscriptionLoop, this);
    spdlog::info("DemodService: started (req={} pub={})",
                 cfg_.broker.demod_request_queue, cfg_.broker.demod_topic);
}

void DemodService::stop() {
    if (!running_.exchange(false)) return;

    // Signal all stream loops to exit.
    {
        std::lock_guard lk(streams_mu_);
        for (auto& [id, s] : streams_)
            s->active.store(false);
        streams_.clear();
    }

    q_cv_.notify_all();
    if (amqp_handler_) amqp_handler_->close();
    if (sub_thread_.joinable())    sub_thread_.join();
    if (worker_thread_.joinable()) worker_thread_.join();

    // Join stream threads (each may still be mid-fetch — wait for them).
    {
        std::lock_guard lk(thread_mu_);
        for (auto& t : stream_threads_)
            if (t.joinable()) t.join();
        stream_threads_.clear();
    }
}

void DemodService::subscriptionLoop() {
    while (running_.load()) {
        auto on_det   = [this](const PendingDemod& p){ onDemodRequest(p); };
        auto on_start = [this](const std::string& sid, const PendingDemod& p){ startStream(sid, p); };
        auto on_stop  = [this](const std::string& sid){ stopStream(sid); };
        amqp_handler_ = std::make_shared<ServiceAmqpHandler>(cfg_.broker, on_det, on_start, on_stop);
        auto container = std::make_shared<proton::container>(*amqp_handler_);
        try {
            container->run();
        } catch (const std::exception& ex) {
            spdlog::error("DemodService: AMQP error: {}", ex.what());
        }
        if (!running_.load()) break;
        spdlog::info("DemodService: reconnecting in 3s...");
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

void DemodService::onDemodRequest(const PendingDemod& p) {
    spdlog::info("DemodService: DEMOD_REQUEST {:.3f} MHz mod={}",
                 p.center_freq.in(au::hertz) / 1e6, p.modulation);
    {
        std::lock_guard lk(q_mu_);
        queue_.push(p);
    }
    q_cv_.notify_one();
}

void DemodService::workerLoop() {
    while (running_.load()) {
        PendingDemod p;
        {
            std::unique_lock lk(q_mu_);
            q_cv_.wait_for(lk, std::chrono::milliseconds(200),
                           [this]{ return !queue_.empty() || !running_.load(); });
            if (!running_.load() && queue_.empty()) break;
            if (queue_.empty()) continue;
            p = queue_.front();
            queue_.pop();
        }
        try {
            std::string req_id = "demod-" +
                std::to_string(std::chrono::steady_clock::now()
                               .time_since_epoch().count());
            router_.route(p.modulation, p.center_freq, p.bandwidth,
                          p.symbol_rate, p.confidence,
                          p.timestamp, req_id);
        } catch (const std::exception& ex) {
            spdlog::error("DemodService: worker error: {}", ex.what());
        }
    }
}

// ── Streaming ─────────────────────────────────────────────────────────────────

void DemodService::startStream(const std::string& stream_id, const PendingDemod& p)
{
    {
        std::lock_guard lk(streams_mu_);
        if (streams_.count(stream_id)) {
            spdlog::warn("DemodService: stream {} already active, ignoring START", stream_id);
            return;
        }
        auto sp = std::make_shared<StreamSession>();
        sp->params = p;
        streams_[stream_id] = std::move(sp);
    }
    spdlog::info("DemodService: starting stream {} for {:.3f} MHz {}",
                 stream_id, p.center_freq.in(au::hertz) / 1e6, p.modulation);
    std::lock_guard lk(thread_mu_);
    stream_threads_.emplace_back([this, stream_id]() {
        StreamPtr session;
        {
            std::lock_guard slk(streams_mu_);
            auto it = streams_.find(stream_id);
            if (it == streams_.end()) return;
            session = it->second;
        }
        streamLoop(session, stream_id);
    });
}

void DemodService::stopStream(const std::string& stream_id)
{
    std::lock_guard lk(streams_mu_);
    auto it = streams_.find(stream_id);
    if (it == streams_.end()) {
        spdlog::warn("DemodService: STOP_DEMOD_STREAM for unknown stream {}", stream_id);
        return;
    }
    it->second->active.store(false);
    streams_.erase(it);
    spdlog::info("DemodService: stream {} stop requested", stream_id);
}

void DemodService::streamLoop(StreamPtr session, std::string stream_id)
{
    constexpr int MAX_CONSECUTIVE_FAILURES = 3;
    int failures = 0;
    uint64_t chunk = 0;

    while (session->active.load() && running_.load()) {
        std::string req_id = stream_id + "-" + std::to_string(chunk++);
        auto& p = session->params;
        bool ok = router_.route(p.modulation, p.center_freq, p.bandwidth,
                                p.symbol_rate, p.confidence,
                                p.timestamp, req_id, stream_id);
        if (!ok) {
            if (++failures >= MAX_CONSECUTIVE_FAILURES) {
                spdlog::warn("DemodService: stream {} stopping — {} consecutive IQ failures",
                             stream_id, failures);
                break;
            }
        } else {
            failures = 0;
        }
    }

    spdlog::info("DemodService: stream {} ended (chunk={})", stream_id, chunk);
    // Remove session entry if it hasn't been removed already (e.g. natural stop).
    std::lock_guard lk(streams_mu_);
    streams_.erase(stream_id);
}

void DemodService::publishResult(const DemodResult& r) {
    // ── Write file ───────────────────────────────────────────────────────────
    const double cf_hz = r.center_freq.in(au::hertz);
    const double sr_hz = r.sample_rate.in(au::hertz);
    const int64_t dur_ms = static_cast<int64_t>(r.duration.in(au::seconds) * 1000.0);

    auto ts = std::to_string(r.timestamp_ms);
    std::ostringstream fn;
    fn << cfg_.output.output_dir << "/"
       << std::fixed << std::setprecision(3) << (cf_hz / 1e6)
       << "MHz_" << r.modulation;
    if (!r.stream_id.empty())
        fn << "_stream-" << r.stream_id.substr(0, 8);
    fn << "_" << ts;

    if (r.type == DemodClass::Audio && !r.audio.empty()) {
        std::string wav_path = fn.str() + ".wav";
        try {
            writeWav(wav_path, r.audio, static_cast<int>(sr_hz));
            spdlog::info("DemodService: wrote {}", wav_path);
        } catch (const std::exception& ex) {
            spdlog::warn("DemodService: WAV write failed: {}", ex.what());
        }
    } else if (r.type == DemodClass::Bits && !r.bits.empty()) {
        std::string bits_path = fn.str() + ".bits";
        std::ofstream bf(bits_path, std::ios::binary);
        if (bf) {
            bf.write(reinterpret_cast<const char*>(r.bits.data()),
                     static_cast<std::streamsize>(r.bits.size()));
            spdlog::info("DemodService: wrote {}", bits_path);
        }
    } else if (r.type == DemodClass::RawIq && !r.raw_iq.empty()) {
        std::string iq_path = fn.str() + ".iq";
        std::ofstream iqf(iq_path, std::ios::binary);
        if (iqf) {
            iqf.write(reinterpret_cast<const char*>(r.raw_iq.data()),
                      static_cast<std::streamsize>(
                          r.raw_iq.size() * sizeof(std::complex<float>)));
            spdlog::info("DemodService: wrote {}", iq_path);
        }
    }

    // ── Publish to rf.demod ──────────────────────────────────────────────────
    if (!cfg_.output.publish_amqp || !amqp_handler_) return;

    json j;
    j["msg_type"]        = "DEMOD_RESULT";
    j["schema_version"]  = "1.0";
    j["center_freq_hz"]  = cf_hz;
    j["modulation"]      = r.modulation;
    j["timestamp_ms"]    = r.timestamp_ms;
    j["duration_ms"]     = dur_ms;
    if (!r.stream_id.empty())
        j["stream_id"]   = r.stream_id;

    if (r.type == DemodClass::Audio) {
        j["demod_class"]     = "audio";
        j["sample_rate_hz"]  = sr_hz;
        j["channels"]        = 1;
        j["format"]          = "pcm_f32le";
        j["num_samples"]     = r.audio.size();
        if (!r.audio.empty())
            j["data_b64"] = sdr::base64::encode(
                r.audio.data(), r.audio.size() * sizeof(float));
    } else if (r.type == DemodClass::Bits) {
        j["demod_class"]      = "bits";
        j["symbol_rate_sps"]  = sr_hz;
        j["bits_per_symbol"]  = r.bits_per_symbol;
        j["num_bits"]         = r.bits.size() * 8;
        if (!r.bits.empty())
            j["data_b64"] = sdr::base64::encode(
                r.bits.data(), r.bits.size());
    } else {
        j["demod_class"]     = "raw_iq";
        j["sample_rate_hz"]  = sr_hz;
        j["num_samples"]     = r.raw_iq.size();
        if (!r.raw_iq.empty())
            j["data_b64"] = sdr::base64::encode(
                r.raw_iq.data(), r.raw_iq.size() * sizeof(std::complex<float>));
    }

    amqp_handler_->publish(j.dump());
}

} // namespace demod
