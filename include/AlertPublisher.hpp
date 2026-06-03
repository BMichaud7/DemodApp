#pragma once
/**
 * @file AlertPublisher.hpp
 * @brief Publishes RF_ALERT JSON to the rf.alerts AMQP topic.
 * Used by DemodRouter to forward alerts from demodulator validators.
 */
#include <proton/container.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/sender.hpp>
#include <proton/work_queue.hpp>
#include <atomic>
#include <thread>
#include <string>

namespace demod {

class AlertPublisher : public proton::messaging_handler {
public:
    AlertPublisher(std::string url, std::string user, std::string pass,
                   std::string topic = "rf.alerts");
    ~AlertPublisher();

    void start();
    void stop();
    void publish(const std::string& alert_json, double freq_hz);

private:
    void on_container_start(proton::container& c) override;
    void on_connection_open(proton::connection& c) override;
    void on_sender_open(proton::sender& s) override;
    void on_error(const proton::error_condition& e) override;

    std::string          url_, user_, pass_, topic_;
    proton::container    container_;
    proton::sender       sender_;
    proton::work_queue*  wq_{nullptr};
    std::thread          thread_;
    std::atomic<bool>    stopping_{false};
};

} // namespace demod
