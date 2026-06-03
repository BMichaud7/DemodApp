/**
 * @file P25Phase2Demod.hpp
 * @brief P25 Phase 2 H-DQPSK TDMA voice channel decoder. Returns raw dibits (AMBE+2 future).
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class P25Phase2Demod : public IDemod {
public:
    explicit P25Phase2Demod() = default;
    ~P25Phase2Demod() override = default;
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
