/**
 * @file DemodRouter.hpp
 * @brief Routes detected signals to the appropriate IDemod implementation.
 */
#pragma once
#include "AppConfig.hpp"
#include "IqFetcher.hpp"
#include "demod/IDemod.hpp"
#include <functional>
#include <string>
#include "au/units/hertz.hh"

namespace demod {

/// @brief Callback invoked with a completed DemodResult.
using ResultCallback = std::function<void(const DemodResult&)>;

/**
 * @class DemodRouter
 * @brief Maps modulation class names to demodulation parameters, fetches IQ
 *        samples via IqFetcher, and dispatches to the appropriate IDemod.
 *
 * Called by the DemodService worker thread for every analysis result that
 * clears the confidence threshold and cooldown gate.
 *
 * @note Thread-safety: route() must be called from a single worker thread.
 *       DemodRouter is not internally synchronised.
 */
class DemodRouter {
public:
    /**
     * @brief Construct a DemodRouter.
     *
     * @param cfg       Application configuration (engine and broker settings).
     * @param fetcher   Reference to the shared IqFetcher instance.
     * @param on_result Callback invoked once per completed demodulation.
     */
    DemodRouter(const AppConfig& cfg, IqFetcher& fetcher,
                ResultCallback on_result);

    /**
     * @brief Route a detected signal to the correct demodulator.
     *
     * Determines fetch parameters via paramsFor(), collects IQ, dispatches
     * to the appropriate IDemod implementation, and calls on_result().
     *
     * @param modulation  Modulation string (e.g. "FM_WB", "CW", "AFSK").
     * @param center_freq Centre frequency.
     * @param bandwidth   Reported signal bandwidth (zero = use default).
     * @param symbol_rate Symbol rate hint (zero = unknown).
     * @param confidence  Classifier confidence (informational).
     * @param timestamp_ms Analysis timestamp in milliseconds.
     * @param request_id  Correlation ID for the IQ fetch request.
     * @param stream_id   Non-empty when called from a streaming session;
     *                    propagated into the DemodResult for subscriber filtering.
     * @return true  if IQ was successfully fetched and a result was produced.
     * @return false if IQ fetch returned empty (signal gone or SDR busy).
     */
    bool route(const std::string&       modulation,
               au::QuantityD<au::Hertz> center_freq,
               au::QuantityD<au::Hertz> bandwidth,
               au::QuantityD<au::Hertz> symbol_rate,
               float                    confidence,
               int64_t                  timestamp_ms,
               const std::string&       request_id,
               const std::string&       stream_id = "");

private:
    AppConfig      cfg_;       ///< Application configuration.
    IqFetcher&     fetcher_;   ///< Shared IQ fetcher.
    ResultCallback on_result_; ///< Result callback.

    /**
     * @brief Determine DemodParams for a given modulation.
     *
     * @param mod         Modulation string.
     * @param bandwidth   Reported bandwidth (zero = use default).
     * @param symbol_rate Symbol rate hint (zero = unknown).
     * @return DemodParams Fetch and dispatch parameters.
     */
    DemodParams paramsFor(const std::string&       mod,
                          au::QuantityD<au::Hertz> bandwidth,
                          au::QuantityD<au::Hertz> symbol_rate) const;
};

} // namespace demod
