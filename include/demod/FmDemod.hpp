#pragma once
#include "IDemod.hpp"

namespace demod {

class FmDemod : public IDemod {
public:
    // deviation_hz: FM carrier deviation (75kHz for WBFM, 2.5-5kHz for NBFM)
    explicit FmDemod(double deviation_hz, int output_sample_rate = 48000);
    ~FmDemod() override;

    DemodResult process(const std::vector<std::complex<float>>& iq,
                        double sr_sps, double center_freq_hz,
                        int64_t timestamp_ms) override;

private:
    double deviation_hz_;
    int    out_sr_;
};

} // namespace demod
