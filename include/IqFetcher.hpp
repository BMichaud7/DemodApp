#pragma once
#include "AppConfig.hpp"
#include <complex>
#include <memory>
#include <mutex>
#include <vector>

namespace demod {

class IqTaskChannel;  // fwd

class IqFetcher {
public:
    IqFetcher(const BrokerConfig& broker, const std::string& local_ip, int rank);
    ~IqFetcher();

    // Tune to center_freq_hz, collect for duration_ms at sample_rate_sps.
    // Returns interleaved float32 I/Q on success, empty on error.
    std::vector<std::complex<float>> collect(double center_freq_hz,
                                             double bandwidth_hz,
                                             double sample_rate_sps,
                                             int64_t duration_ms,
                                             const std::string& request_id);

    double lastSampleRate() const { return last_sr_; }

private:
    BrokerConfig broker_;
    std::string  local_ip_;
    int          rank_;
    double       last_sr_{0.0};
    std::mutex   collect_mu_;
    std::unique_ptr<IqTaskChannel> ch_;
};

} // namespace demod
