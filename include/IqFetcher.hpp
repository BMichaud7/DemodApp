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
 * @file IqFetcher.hpp
 * @brief IQ sample acquisition from the SdrTaskApi.
 */
#pragma once
#include "AppConfig.hpp"
#include <complex>
#include <memory>
#include <mutex>
#include <vector>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

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
     * @param center_freq     Desired centre frequency.
     * @param bandwidth       Desired capture bandwidth.
     * @param sample_rate     Desired sample rate.
     * @param duration        Capture duration.
     * @param request_id      Correlation ID for the SDR task request.
     * @return std::vector<std::complex<float>> IQ samples on success;
     *         empty vector on error or timeout.
     */
    std::vector<std::complex<float>> collect(au::QuantityD<au::Hertz>   center_freq,
                                             au::QuantityD<au::Hertz>   bandwidth,
                                             au::QuantityD<au::Hertz>   sample_rate,
                                             au::QuantityD<au::Seconds> duration,
                                             const std::string&         request_id);

    /**
     * @brief Return the actual sample rate from the most recent collect() call.
     *
     * @return au::QuantityD<au::Hertz> Sample rate reported by the SDR backend,
     *                or 0 Hz if no successful collect() has been made.
     */
    au::QuantityD<au::Hertz> lastSampleRate() const { return au::hertz(last_sr_); }

private:
    BrokerConfig broker_;   ///< AMQP broker configuration.
    std::string  local_ip_; ///< Local callback IP address.
    int          rank_;     ///< Priority rank for SDR task requests.
    double       last_sr_{0.0}; ///< Sample rate from last successful collect.
    std::mutex   collect_mu_;   ///< Serializes concurrent collect() calls.
    std::unique_ptr<IqTaskChannel> ch_; ///< Internal AMQP task channel.
};

} // namespace demod
