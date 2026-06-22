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
#pragma once
/**
 * @file AlertPublisher.hpp
 * @brief AMQP publisher for RF_ALERT JSON messages on the @c rf.alerts topic.
 *
 * DemodRouter calls AlertPublisher::publish() whenever a demodulator validator
 * sets a non-empty @c DemodResult::alert_json.  AcquisitionApp's AlertConsumer
 * subscribes to the same topic and persists the alerts to the @c rf_alerts
 * PostgreSQL table.
 *
 * @par Message format
 * @code{.json}
 * {
 *   "type":     "ADSB_SPOOFING",
 *   "severity": "HIGH",
 *   "freq_hz":  1090000000.0,
 *   "details":  "ICAO ABCDEF impossible groundspeed 2200 knots"
 * }
 * @endcode
 *
 * @see DemodRouter, AlertConsumer (AcquisitionApp), ThreatDetectionConfig
 */
#include <proton/container.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/reconnect_options.hpp>
#include <proton/sender.hpp>
#include <proton/work_queue.hpp>
#include <atomic>
#include <thread>
#include <string>

namespace demod {

/**
 * @class AlertPublisher
 * @brief Thread-safe AMQP publisher for @c rf.alerts threat alert messages.
 *
 * Runs a @c proton::container in a background thread.  Call start() before
 * the first publish() call and stop() during application shutdown.
 *
 * @note publish() is safe to call from the DemodRouter worker thread after
 *       start() returns.
 */
class AlertPublisher : public proton::messaging_handler {
public:
    /**
     * @brief Construct the publisher.
     * @param url   AMQP broker URL (e.g. @c "amqp://localhost:5672").
     * @param user  AMQP username (empty = anonymous).
     * @param pass  AMQP password.
     * @param topic Destination topic address (default: @c "rf.alerts").
     */
    AlertPublisher(std::string url, std::string user, std::string pass,
                   std::string topic = "rf.alerts");

    /// @brief Stop the publisher and join the AMQP thread.
    ~AlertPublisher();

    /// @brief Start the background AMQP sender thread.
    void start();

    /// @brief Close the AMQP connection and join the thread.
    void stop();

    /**
     * @brief Publish a threat alert JSON string.
     *
     * Injects @p freq_hz into the JSON body before sending.  Silently
     * discarded if the sender is not yet connected or is stopping.
     * @param alert_json JSON object string produced by a demodulator validator.
     * @param freq_hz    Centre frequency of the detected signal (Hz); injected
     *                   as the @c freq_hz field in the outgoing JSON.
     */
    void publish(const std::string& alert_json, double freq_hz);

private:
    void on_container_start(proton::container& c) override;
    void on_connection_open(proton::connection& c) override;
    void on_sender_open(proton::sender& s) override;
    void on_error(const proton::error_condition& e) override;

    std::string          url_;      ///< Broker URL.
    std::string          user_;     ///< AMQP username.
    std::string          pass_;     ///< AMQP password.
    std::string          topic_;    ///< Destination topic address.
    proton::container    container_;///< Proton messaging container.
    proton::sender       sender_;   ///< Outbound AMQP sender link.
    proton::work_queue*  wq_{nullptr}; ///< Work queue for thread-safe publish.
    std::thread          thread_;   ///< Thread running container_.run().
    std::atomic<bool>    stopping_{false}; ///< Set by stop() to suppress error logs.
};

} // namespace demod

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
