#pragma once
#include "AppConfig.hpp"
#include "IqFetcher.hpp"
#include "demod/IDemod.hpp"
#include <functional>
#include <string>

namespace demod {

using ResultCallback = std::function<void(const DemodResult&)>;

// Maps modulation class names (from ONNX classifier) to demod parameters,
// fetches IQ via IqFetcher, and dispatches to the correct IDemod implementation.
class DemodRouter {
public:
    DemodRouter(const AppConfig& cfg, IqFetcher& fetcher,
                ResultCallback on_result);

    // Called by DemodService worker thread.
    void route(const std::string& modulation,
               double center_freq_hz,
               double bandwidth_hz,
               double symbol_rate_sps,
               float  confidence,
               int64_t timestamp_ms,
               const std::string& request_id);

private:
    AppConfig     cfg_;
    IqFetcher&    fetcher_;
    ResultCallback on_result_;

    DemodParams paramsFor(const std::string& mod,
                          double bandwidth_hz,
                          double symbol_rate_sps) const;
};

} // namespace demod
