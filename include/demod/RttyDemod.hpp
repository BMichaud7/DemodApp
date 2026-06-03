/**
 * @file RttyDemod.hpp
 * @brief RTTY Baudot radioteletype (2-FSK, auto-detect 45/50/75/110 baud, ITA-2).
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class RttyDemod : public IDemod {
public:
    explicit RttyDemod() = default;
    ~RttyDemod() override = default;
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
