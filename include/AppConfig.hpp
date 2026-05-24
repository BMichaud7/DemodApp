/**
 * @file AppConfig.hpp
 * @brief Application configuration structures and XML loader.
 */
#pragma once
#include <string>
#include <cstdint>

namespace demod {

/**
 * @brief AMQP broker connection parameters.
 */
struct BrokerConfig {
    std::string url                  = "amqp://localhost:5672"; ///< Broker URL.
    std::string username;                                       ///< Optional username.
    std::string password;                                       ///< Optional password.
    std::string demod_request_queue  = "rf.demod.request";     ///< Queue for inbound DEMOD_REQUEST messages.
    std::string task_queue           = "sdr.tasks";             ///< Queue for IQ task requests.
    std::string demod_topic          = "rf.demod";              ///< Topic for demod output.
};

/**
 * @brief File and AMQP output configuration.
 */
struct OutputConfig {
    std::string output_dir  = "/tmp/sdr-demod"; ///< Directory for file output.
    bool        publish_amqp = true;            ///< Publish results over AMQP.
};

/**
 * @brief Demodulation engine tuning parameters.
 */
struct EngineConfig {
    int     rank                = 3;         ///< IQ fetch priority rank.
    int64_t audio_duration_ms   = 5'000;     ///< IQ capture duration for audio modes (ms).
    int64_t digital_duration_ms = 2'000;     ///< IQ capture duration for digital modes (ms).
    int     audio_sample_rate   = 48'000;    ///< Target output PCM sample rate in Hz.
};

/**
 * @class AppConfig
 * @brief Top-level application configuration container.
 *
 * Aggregates broker, output, and engine configuration sections.
 * Load from an XML file with fromXml().
 */
struct AppConfig {
    BrokerConfig broker; ///< AMQP broker settings.
    OutputConfig output; ///< Output settings.
    EngineConfig engine; ///< Engine tuning settings.
    std::string  local_ip = "127.0.0.1"; ///< Local IP for IQ fetch callbacks.

    /**
     * @brief Parse an XML configuration file and return an AppConfig.
     *
     * @param path Filesystem path to the XML configuration file.
     * @return AppConfig Populated configuration; unrecognised elements are
     *                   silently ignored and defaults are preserved.
     * @note Throws std::runtime_error if the file cannot be opened or parsed.
     */
    static AppConfig fromXml(const std::string& path);
};

} // namespace demod
