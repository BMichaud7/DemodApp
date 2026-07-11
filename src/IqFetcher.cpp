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
#include "IqFetcher.hpp"
#include <sdr/Types.hpp>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

#include <proton/container.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/reconnect_options.hpp>
#include <proton/sender.hpp>
#include <proton/sender_options.hpp>
#include <proton/receiver.hpp>
#include <proton/receiver_options.hpp>
#include <proton/source_options.hpp>
#include <proton/target_options.hpp>
#include <proton/delivery.hpp>
#include <proton/work_queue.hpp>
#include <proton/transport.hpp>
#include <proton/symbol.hpp>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>

namespace demod {

using json = nlohmann::json;
using namespace std::chrono;

// ── Persistent AMQP task channel (mirrors IqCollector from AnalysisApp) ───────

class IqTaskChannel : public proton::messaging_handler {
public:
    IqTaskChannel(std::string url, std::string user, std::string pass,
                  std::string req_queue)
        : url_(std::move(url)), user_(std::move(user)), pass_(std::move(pass))
        , req_queue_(std::move(req_queue)) {}

    ~IqTaskChannel() { stop(); }

    void start(int timeout_ms = 60000) {
        container_ = std::make_unique<proton::container>(*this);
        thread_ = std::thread([this]{ container_->run(); });
        std::unique_lock lk(mu_);
        ready_cv_.wait_for(lk, milliseconds(timeout_ms), [this]{ return ready_; });
    }

    void stop() {
        if (container_) {
            if (auto* wq = wq_.load())
                wq->add([this]{ sender_.connection().close(); });
            else
                // wq_ is null when the broker was never reached (reconnect loop
                // still running). Without this, thread_.join() blocks forever
                // under max_attempts(0) — same fix as GpsApp::AmqpPublisher.
                container_->stop();
            if (thread_.joinable()) thread_.join();
            wq_.store(nullptr);
            container_.reset();
        }
    }

    std::string exchange(const std::string& body, const std::string& corr_id,
                         int timeout_ms) {
        {
            std::unique_lock lk(mu_);
            if (!ready_cv_.wait_for(lk, milliseconds(timeout_ms),
                                    [this]{ return ready_; })) return {};
            pending_corr_ = corr_id;
            pending_body_.clear();
            pending_done_ = false;
        }
        if (auto* wq = wq_.load())
            wq->add([this, body]() mutable {
                proton::message msg;
                msg.body(body);
                msg.content_type("application/json");
                msg.reply_to(reply_addr_);
                // Do NOT check credit() — proton queues when credit arrives.
                // Checking credit causes silent drops when the receiver hasn't
                // yet propagated credit back (same fix as AmqpClient::sendOn).
                if (sender_) sender_.send(msg);
            });
        std::unique_lock lk(mu_);
        result_cv_.wait_for(lk, milliseconds(timeout_ms),
                            [this]{ return pending_done_; });
        return pending_body_;
    }

    void send(const std::string& body) {
        std::unique_lock lk(mu_);
        if (!ready_) return;
        auto* wq = wq_.load();
        if (!wq) return;
        wq->add([this, body]() mutable {
            proton::message msg;
            msg.body(body);
            msg.content_type("application/json");
            if (sender_) sender_.send(msg);
        });
    }

    void on_container_start(proton::container& c) override {
        proton::connection_options opts;
        if (!user_.empty()) {
            opts.sasl_allowed_mechs("PLAIN");
            opts.sasl_allow_insecure_mechs(true);
            opts.user(user_).password(pass_);
        } else {
            opts.sasl_allowed_mechs("ANONYMOUS");
        }
        proton::reconnect_options ropts;
        ropts.delay(proton::duration(2000));
        ropts.max_delay(proton::duration(30000));
        ropts.max_attempts(0);
        opts.reconnect(ropts);
        c.connect(url_, opts);
    }

    void on_connection_open(proton::connection& conn) override {
        proton::sender_options sopts;
        sopts.target(proton::target_options().capabilities({proton::symbol("queue")}));
        sender_ = conn.open_sender(req_queue_, sopts);
        proton::receiver_options ropts;
        ropts.source(proton::source_options().dynamic(true));
        conn.open_receiver("", ropts);
    }

    void on_receiver_open(proton::receiver& r) override {
        reply_addr_ = r.source().address();
        wq_.store(&r.work_queue());
        std::lock_guard lk(mu_);
        ready_ = true;
        ready_cv_.notify_all();
    }

    void on_message(proton::delivery& d, proton::message& msg) override {
        d.accept();
        try {
            std::string b = proton::get<std::string>(msg.body());
            auto j = json::parse(b);
            std::lock_guard lk(mu_);
            if (!pending_corr_.empty() &&
                (j.value("request_id","") == pending_corr_ ||
                 j.value("correlation_id","") == pending_corr_)) {
                pending_body_ = b;
                pending_done_ = true;
                pending_corr_.clear();
                result_cv_.notify_all();
            }
        } catch (...) {}
    }

    void on_transport_error(proton::transport&) override {
        std::lock_guard lk(mu_);
        ready_ = false;
    }
    void on_connection_error(proton::connection&) override {}

private:
    std::string url_, user_, pass_, req_queue_;
    std::unique_ptr<proton::container> container_;
    std::thread thread_;
    proton::sender                   sender_;
    std::atomic<proton::work_queue*> wq_{nullptr};
    std::string                      reply_addr_;
    std::mutex mu_;
    std::condition_variable ready_cv_, result_cv_;
    bool ready_{false};
    std::string pending_corr_, pending_body_;
    bool pending_done_{false};
};

// ── UDP helpers ───────────────────────────────────────────────────────────────

static int openBoundUdpSocket(uint16_t port = 0) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    int rcvbuf = 32 * 1024 * 1024;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd); return -1;
    }
    return fd;
}

static std::vector<std::complex<float>> receiveIq(int fd, int n_samples,
                                                    int timeout_ms) {
    std::vector<std::complex<float>> iq;
    iq.reserve(static_cast<size_t>(n_samples));
    auto deadline = steady_clock::now() + milliseconds(timeout_ms);
    std::vector<uint8_t> buf(65536);

    while (static_cast<int>(iq.size()) < n_samples) {
        auto now = steady_clock::now();
        if (now >= deadline) break;
        long rem = duration_cast<milliseconds>(deadline - now).count();
        fd_set fds; FD_ZERO(&fds); FD_SET(fd, &fds);
        timeval tv{rem / 1000, (rem % 1000) * 1000};
        if (::select(fd + 1, &fds, nullptr, nullptr, &tv) <= 0) break;
        ssize_t n = ::recv(fd, buf.data(), buf.size(), 0);
        if (n < static_cast<ssize_t>(sdr::IQ_PACKET_HEADER_SIZE)) continue;
        const auto* hdr = reinterpret_cast<const sdr::IqPacketHeader*>(buf.data());
        if (hdr->magic != sdr::IQ_PACKET_MAGIC) continue;
        int nsamp = std::min((int)hdr->num_samples,
                             (int)(n - sdr::IQ_PACKET_HEADER_SIZE)
                             / (int)sizeof(std::complex<float>));
        const auto* s = reinterpret_cast<const std::complex<float>*>(
                            buf.data() + sdr::IQ_PACKET_HEADER_SIZE);
        iq.insert(iq.end(), s, s + nsamp);
    }
    return iq;
}

// ── IqFetcher ────────────────────────────────────────────────────────────────

IqFetcher::IqFetcher(const BrokerConfig& broker,
                     const std::string& local_ip, int rank)
    : broker_(broker), local_ip_(local_ip), rank_(rank)
{
    ch_ = std::make_unique<IqTaskChannel>(broker_.url, broker_.username,
                                          broker_.password, broker_.task_queue);
    ch_->start(60000);
}

IqFetcher::~IqFetcher() = default;

std::vector<std::complex<float>> IqFetcher::collect(au::QuantityD<au::Hertz>   center_freq,
                                                     au::QuantityD<au::Hertz>   bandwidth,
                                                     au::QuantityD<au::Hertz>   sample_rate,
                                                     au::QuantityD<au::Seconds> duration,
                                                     const std::string& req_id) {
    std::lock_guard guard(collect_mu_);

    // Extract raw values for JSON serialisation
    const double center_freq_hz  = center_freq.in(au::hertz);
    const double bandwidth_hz    = bandwidth.in(au::hertz);
    const double sample_rate_sps = sample_rate.in(au::hertz);
    const double duration_s      = duration.in(au::seconds);
    const int64_t duration_ms    = static_cast<int64_t>(duration_s * 1000.0);

    auto now_ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();

    // Pre-bind a UDP socket before submitting the task
    int      fd   = openBoundUdpSocket(0);
    uint16_t port = 0;
    if (fd >= 0) {
        sockaddr_in sa{}; socklen_t sl = sizeof(sa);
        if (getsockname(fd, reinterpret_cast<sockaddr*>(&sa), &sl) == 0)
            port = ntohs(sa.sin_port);
    }

    json streaming = {{"dest_ip", local_ip_}};
    if (port > 0) streaming["dest_ports"] = json::array({(int)port});

    std::string req_body = json{
        {"msg_type",       "TASK_REQUEST"},
        {"schema_version", sdr::SCHEMA_VERSION},
        {"request_id",     req_id},
        {"timestamp_ms",   now_ms},
        {"task_type",      "NARROWBAND"},
        {"rank",           rank_},
        {"schedule", {{"mode","IMMEDIATE"},{"duration_ms", duration_ms}}},
        {"rf", {
            {"center_freq_hz",  center_freq_hz},
            {"bandwidth_hz",    bandwidth_hz},
            {"sample_rate_sps", sample_rate_sps},
            {"rx_count",        1}
        }},
        {"streaming", streaming}
    }.dump();

    int timeout = (int)(duration_ms + 10000);
    std::string resp = ch_->exchange(req_body, req_id, timeout);
    if (resp.empty()) {
        spdlog::error("IqFetcher: no response for req={}", req_id);
        if (fd >= 0) ::close(fd);
        return {};
    }

    std::string task_id;
    int udp_port = 0;
    try {
        auto j = json::parse(resp);
        if (j.value("status","") != "ACCEPTED") {
            spdlog::warn("IqFetcher: rejected: {}", j.value("reject_reason","?"));
            if (fd >= 0) ::close(fd);
            return {};
        }
        task_id = j.value("task_id","");
        if (j.contains("streams") && !j["streams"].empty()) {
            udp_port = j["streams"][0].value("udp_port", 0);
            last_sr_ = j["streams"][0].value("sample_rate_sps", sample_rate_sps);
        }
    } catch (...) {
        if (fd >= 0) ::close(fd);
        return {};
    }

    if (udp_port == 0) {
        spdlog::error("IqFetcher: ACCEPTED has no udp_port");
        if (fd >= 0) ::close(fd);
        return {};
    }

    // Use pre-bound socket if port matches, else re-bind
    if (fd >= 0 && port > 0 && udp_port != port) {
        ::close(fd);
        fd = openBoundUdpSocket(static_cast<uint16_t>(udp_port));
    } else if (fd < 0) {
        // Initial bind failed; try binding to the server-assigned port now.
        fd = openBoundUdpSocket(static_cast<uint16_t>(udp_port));
    }

    if (fd < 0) {
        spdlog::error("IqFetcher: cannot open UDP socket for port {}", udp_port);
        return {};
    }

    int n_samples = static_cast<int>(sample_rate_sps * duration_s);
    auto iq = receiveIq(fd, n_samples, timeout);
    ::close(fd);

    spdlog::info("IqFetcher: {} samples at {:.3f} MHz sr={:.0f}",
                 iq.size(), center_freq_hz / 1e6, last_sr_);

    if (!task_id.empty()) {
        ch_->send(json{
            {"msg_type",  "TASK_STOP"},
            {"request_id", req_id + "_stop"},
            {"task_id",    task_id},
            {"timestamp_ms", now_ms},
            {"reason",     "demod complete"}
        }.dump());
    }

    return iq;
}

} // namespace demod

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
