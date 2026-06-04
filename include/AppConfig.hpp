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
 * @file AppConfig.hpp
 * @brief Application configuration structures and XML loader.
 */
#pragma once
#include <string>
#include <cstdint>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

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
    int                       rank                = 4;              ///< IQ fetch priority rank (higher = higher priority).
    au::QuantityD<au::Seconds> audio_duration      = au::seconds(5.0);   ///< IQ capture duration for audio modes.
    au::QuantityD<au::Seconds> digital_duration    = au::seconds(2.0);   ///< IQ capture duration for digital modes.
    au::QuantityD<au::Hertz>  audio_sample_rate   = au::hertz(48'000.0); ///< Target output PCM sample rate.
    float                     min_confidence      = 0.70f;          ///< Minimum classifier confidence to trigger demodulation.
};

/**
 * @class AppConfig
 * @brief Top-level application configuration container.
 *
 * Aggregates broker, output, and engine configuration sections.
 * Load from an XML file with fromXml().
 */
/// P25 trunked-system monitor configuration.
struct P25MonitorConfig {
    bool        enabled          = false;
    double      control_freq_hz  = 0.0;    ///< 0 = auto-detect from rf.detections
    double      sample_rate_hz   = 48000.0;
    double      capture_s        = 3.0;
    std::string grant_topic      = "rf.p25.grants";
    std::vector<uint32_t> tg_whitelist; ///< empty = all talk groups
};

/**
 * @brief Threat detection validator enable/disable flags for DemodApp.
 *
 * Loaded from demod.xml <threat> block. Master enabled must be true
 * for any individual validator to be active.
 */
struct ThreatDetectionConfig {
    bool enabled{false};          ///< Master switch.
    bool adsb_enabled{true};      ///< ADS-B physics + CRC-rate validator.
    bool ais_enabled{true};       ///< AIS vessel physics + MMSI validator.
    bool eas_enabled{true};       ///< EAS/SAME originator + event validator.
    bool dsc_enabled{true};       ///< DSC distress MMSI validator.
    bool p25_rogue_enabled{true}; ///< P25 rogue site (WACN change) detector.
    std::string alert_topic{"rf.alerts"}; ///< AMQP topic for alert messages.
};

struct AppConfig {
    BrokerConfig          broker;  ///< AMQP broker settings.
    OutputConfig          output;  ///< Output settings.
    EngineConfig          engine;  ///< Engine tuning settings.
    P25MonitorConfig      p25;     ///< P25 control channel monitor settings.
    ThreatDetectionConfig threat;  ///< Threat detection validator flags.
    std::string           local_ip = "127.0.0.1"; ///< Local IP for IQ fetch callbacks.

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
