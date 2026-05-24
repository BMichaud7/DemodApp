#pragma once
#include "IDemod.hpp"
#include <string>

namespace demod {

// Handles: AM_DSB, AM_DSB_SC, AM_SSB_USB, AM_SSB_LSB, TONE
class AmDemod : public IDemod {
public:
    // modulation: one of "AM_DSB", "AM_DSB_SC", "AM_SSB_USB", "AM_SSB_LSB", "TONE"
    explicit AmDemod(const std::string& modulation, int output_sample_rate = 48000);
    ~AmDemod() override;

    DemodResult process(const std::vector<std::complex<float>>& iq,
                        double sr_sps, double center_freq_hz,
                        int64_t timestamp_ms) override;

private:
    std::string mod_;
    int         out_sr_;
};

} // namespace demod
