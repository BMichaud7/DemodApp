#pragma once
#include <string>
#include <cstdint>

namespace demod {

struct BrokerConfig {
    std::string url            = "amqp://localhost:5672";
    std::string username;
    std::string password;
    std::string analysis_topic = "rf.analysis";
    std::string task_queue     = "sdr.tasks";
    std::string demod_topic    = "rf.demod";
};

struct OutputConfig {
    std::string output_dir = "/tmp/sdr-demod";
    bool        publish_amqp = true;
};

struct EngineConfig {
    int     rank               = 3;
    float   min_confidence     = 0.70f;
    int64_t cooldown_ms        = 30'000;
    int64_t audio_duration_ms  = 5'000;
    int64_t digital_duration_ms= 2'000;
    int     audio_sample_rate  = 48'000;
};

struct AppConfig {
    BrokerConfig broker;
    OutputConfig output;
    EngineConfig engine;
    std::string  local_ip = "127.0.0.1";

    static AppConfig fromXml(const std::string& path);
};

} // namespace demod
