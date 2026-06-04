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
#include "AppConfig.hpp"
#include "DemodService.hpp"
#include <spdlog/spdlog.h>
#include <csignal>
#include <atomic>

static std::atomic<bool> g_running{true};

static void sigHandler(int) { g_running = false; }

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    std::string config_path = "/etc/sdr-demod/demod.xml";
    if (argc > 1) config_path = argv[1];

    demod::AppConfig cfg;
    try {
        cfg = demod::AppConfig::fromXml(config_path);
    } catch (const std::exception& ex) {
        spdlog::error("Config error: {}", ex.what());
        return 1;
    }

    // Apply env overrides
    if (const char* v = std::getenv("BROKER_URL"))    cfg.broker.url = v;
    if (const char* v = std::getenv("BROKER_USER"))   cfg.broker.username = v;
    if (const char* v = std::getenv("BROKER_PASS"))   cfg.broker.password = v;
    if (const char* v = std::getenv("LOCAL_IP"))      cfg.local_ip = v;
    if (const char* v = std::getenv("OUTPUT_DIR"))    cfg.output.output_dir = v;
    if (const char* v = std::getenv("SDR_LOG_LEVEL")) {
        spdlog::set_level(spdlog::level::from_str(v));
    }

    spdlog::info("sdr-demod starting (req={} out={})",
                 cfg.broker.demod_request_queue, cfg.output.output_dir);

    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    demod::DemodService svc(cfg);
    svc.start();

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    spdlog::info("Shutting down...");
    svc.stop();
    return 0;
}

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
