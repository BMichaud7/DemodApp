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
     * @param modulation     Modulation string (e.g. "FM_WB", "CW", "AFSK").
     * @param center_freq_hz Centre frequency in Hz.
     * @param bandwidth_hz   Reported signal bandwidth in Hz (0 = use default).
     * @param symbol_rate_sps Symbol rate hint in sps (0 = unknown).
     * @param confidence     Classifier confidence (informational).
     * @param timestamp_ms   Analysis timestamp in milliseconds.
     * @param request_id     Correlation ID for the IQ fetch request.
     */
    void route(const std::string& modulation,
               double center_freq_hz,
               double bandwidth_hz,
               double symbol_rate_sps,
               float  confidence,
               int64_t timestamp_ms,
               const std::string& request_id);

private:
    AppConfig      cfg_;       ///< Application configuration.
    IqFetcher&     fetcher_;   ///< Shared IQ fetcher.
    ResultCallback on_result_; ///< Result callback.

    /**
     * @brief Determine DemodParams for a given modulation.
     *
     * @param mod            Modulation string.
     * @param bandwidth_hz   Reported bandwidth in Hz (0 = use default).
     * @param symbol_rate_sps Symbol rate hint in sps (0 = unknown).
     * @return DemodParams   Fetch and dispatch parameters.
     */
    DemodParams paramsFor(const std::string& mod,
                          double bandwidth_hz,
                          double symbol_rate_sps) const;
};

} // namespace demod
