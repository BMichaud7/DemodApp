/**
 * @file IqFetcher.hpp
 * @brief IQ sample acquisition from the SdrTaskApi.
 */
#pragma once
#include "AppConfig.hpp"
#include <complex>
#include <memory>
#include <mutex>
#include <vector>

namespace demod {

class IqTaskChannel;  ///< Forward declaration of the internal AMQP channel.

/**
 * @class IqFetcher
 * @brief Requests and collects IQ samples from the SDR resource manager via
 *        the SdrTaskApi / AMQP task channel.
 *
 * Each call to collect() sends a tune-and-capture task to the SDR backend
 * and blocks until the requested IQ block is returned or a timeout occurs.
 *
 * @note collect() acquires an internal mutex, so concurrent calls from
 *       multiple threads will serialize.
 */
class IqFetcher {
public:
    /**
     * @brief Construct an IqFetcher.
     *
     * @param broker   Broker connection parameters.
     * @param local_ip Local IP address used for SDR task callbacks.
     * @param rank     IQ fetch priority rank (higher = preferred hardware).
     */
    IqFetcher(const BrokerConfig& broker, const std::string& local_ip, int rank);

    /// @brief Destructor.
    ~IqFetcher();

    /**
     * @brief Tune the SDR and collect a block of IQ samples.
     *
     * @param center_freq_hz  Desired centre frequency in Hz.
     * @param bandwidth_hz    Desired capture bandwidth in Hz.
     * @param sample_rate_sps Desired sample rate in samples/second.
     * @param duration_ms     Capture duration in milliseconds.
     * @param request_id      Correlation ID for the SDR task request.
     * @return std::vector<std::complex<float>> IQ samples on success;
     *         empty vector on error or timeout.
     */
    std::vector<std::complex<float>> collect(double center_freq_hz,
                                             double bandwidth_hz,
                                             double sample_rate_sps,
                                             int64_t duration_ms,
                                             const std::string& request_id);

    /**
     * @brief Return the actual sample rate from the most recent collect() call.
     *
     * @return double Sample rate in sps reported by the SDR backend,
     *                or 0.0 if no successful collect() has been made.
     */
    double lastSampleRate() const { return last_sr_; }

private:
    BrokerConfig broker_;   ///< AMQP broker configuration.
    std::string  local_ip_; ///< Local callback IP address.
    int          rank_;     ///< Priority rank for SDR task requests.
    double       last_sr_{0.0}; ///< Sample rate from last successful collect.
    std::mutex   collect_mu_;   ///< Serializes concurrent collect() calls.
    std::unique_ptr<IqTaskChannel> ch_; ///< Internal AMQP task channel.
};

} // namespace demod
