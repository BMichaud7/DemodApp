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
#include "AlertPublisher.hpp"
#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/message.hpp>
#include <spdlog/spdlog.h>

namespace demod {

AlertPublisher::AlertPublisher(std::string url, std::string user, std::string pass,
                               std::string topic)
    : url_(std::move(url)), user_(std::move(user)), pass_(std::move(pass)),
      topic_(std::move(topic)), container_(*this) {}

AlertPublisher::~AlertPublisher() { stop(); }

void AlertPublisher::start() {
    thread_ = std::thread([this]{ container_.run(); });
}

void AlertPublisher::stop() {
    stopping_ = true;
    if (wq_)
        wq_->add([this]{ sender_.connection().close(); });
    else
        // Connection never reached on_sender_open (e.g. still mid-reconnect
        // after Artemis wasn't up at start()), so there's no work queue to
        // post a close through -- stop the reactor directly so thread_.join()
        // below can't block forever now that reconnect_options below retries
        // indefinitely. Matches AcquisitionApp::AmqpPublisher / TaskAmqpChannel.
        container_.stop();
    if (thread_.joinable()) thread_.join();
}

void AlertPublisher::publish(const std::string& alert_json, double freq_hz) {
    if (!wq_ || stopping_) return;
    // Embed freq_hz into the JSON before sending
    std::string body = alert_json;
    if (!body.empty() && body.back() == '}') {
        body.pop_back();
        body += ",\"freq_hz\":" + std::to_string(freq_hz) + "}";
    }
    proton::message msg(body);
    wq_->add([this, msg]() mutable {
        if (sender_) sender_.send(msg);
    });
}

void AlertPublisher::on_container_start(proton::container& c) {
    proton::connection_options opts;
    if (!user_.empty()) {
        opts.sasl_allowed_mechs("PLAIN");
        opts.sasl_allow_insecure_mechs(true);
        opts.user(user_).password(pass_);
    } else {
        opts.sasl_allowed_mechs("ANONYMOUS");
    }
    // Without this, a failed initial connection (Artemis not up yet at
    // process start, the common case) is permanent: proton tears the
    // container down, wq_ never gets set, and publish() silently no-ops
    // forever -- a threat-detection alert pipeline that looks enabled but
    // never actually sends anything. Same fix as GpsApp::AmqpPublisher and
    // AcquisitionApp::AmqpPublisher/TaskAmqpChannel, ported here.
    proton::reconnect_options ropts;
    ropts.delay(proton::duration(2000));
    ropts.max_delay(proton::duration(30000));
    ropts.max_attempts(0);
    opts.reconnect(ropts);
    c.connect(url_, opts);
}

void AlertPublisher::on_connection_open(proton::connection& c) {
    c.open_sender(topic_);
}

void AlertPublisher::on_sender_open(proton::sender& s) {
    sender_ = s;
    wq_     = &s.work_queue();
    spdlog::info("[AlertPublisher] connected → {}", topic_);
}

void AlertPublisher::on_error(const proton::error_condition& e) {
    if (!stopping_)
        spdlog::warn("[AlertPublisher] error: {}", e.what());
}

} // namespace demod

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
