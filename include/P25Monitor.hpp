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
 * @file P25Monitor.hpp
 * @brief P25 control channel monitor service.
 *
 * Subscribes to rf.detections from AcquisitionApp, recognises P25
 * control channel signals, fetches IQ, decodes TSBKs, filters grants
 * by talk-group whitelist, and publishes to rf.p25.grants.
 *
 * When a grant fires for a whitelisted TG, the monitor requests the
 * controller to tune to the voice channel via the IQ task system.
 * The resulting audio (once decoded by a future component) will be
 * routed to SpeechApp via rf.demod.
 *
 * Audio decoding (IMBE→PCM) is NOT performed here — this monitor is
 * ID-only until higher-performance hardware is available.
 */
#pragma once
#include "AppConfig.hpp"
#include "IqFetcher.hpp"
#include "demod/P25Types.hpp"
#include "demod/C4FmDemod.hpp"
#include "demod/P25ControlDecoder.hpp"

#include <proton/container.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/delivery.hpp>
#include <proton/message.hpp>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <memory>
#include <atomic>
#include <deque>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace demod {

/// Configuration block for the P25 monitor (loaded from demod.xml).
struct P25Config {
    bool     enabled            = false;
    double   control_freq_hz   = 0.0;       ///< Known control channel freq (0 = auto-detect)
    double   sample_rate_hz    = 48000.0;   ///< IQ sample rate for C4FM decode
    double   capture_s         = 3.0;       ///< IQ capture per demod cycle
    std::string grant_topic    = "rf.p25.grants";
    std::unordered_set<uint32_t> tg_whitelist; ///< empty = all TGs
};

/**
 * @class P25Monitor
 * @brief Monitors a P25 trunked system control channel.
 *
 * Lifecycle:
 *   1. start() — connects AMQP, begins listening for rf.detections
 *   2. On a detection near control_freq_hz, fetches IQ and decodes
 *   3. For each whitelisted channel grant, publishes to rf.p25.grants
 *   4. stop() — clean shutdown
 */
class P25Monitor {
public:
    explicit P25Monitor(const AppConfig& cfg, const P25Config& p25cfg,
                        IqFetcher& fetcher);
    ~P25Monitor();

    void start();
    void stop();

private:
    class Handler;

    void decode_control_channel(double freq_hz);
    void on_grant(const p25::ChannelGrant& grant);
    void publish_grant(const p25::ChannelGrant& grant);

    AppConfig         cfg_;
    P25Config         p25cfg_;
    IqFetcher&        fetcher_;

    std::unique_ptr<p25::C4FmDemod>         c4fm_;
    std::unique_ptr<p25::P25ControlDecoder> decoder_;

    // AMQP publisher for grants
    Handler*                                handler_{nullptr};
    std::unique_ptr<proton::container>      pub_container_;
    std::thread                             pub_thread_;
    std::mutex                              pub_mu_;

    // Worker thread: offloads blocking IQ fetch + decode from the reactor
    // thread so proton can keep processing AMQP heartbeats during the 3–13 s
    // capture window.
    std::deque<double>          decode_queue_;
    std::mutex                  decode_mu_;
    std::condition_variable     decode_cv_;
    std::thread                 decode_thread_;
    std::atomic<bool>           decode_running_{false};

    // Tracking: recently granted channels (freq → TG)
    std::unordered_map<uint32_t, double>    active_grants_;
    std::mutex                              grant_mu_;
};

} // namespace demod

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
