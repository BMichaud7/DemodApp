/**
 * @file AcarsDemod.hpp
 * @brief ACARS (Aircraft Communication Addressing and Reporting System) demodulator.
 *
 * ACARS: AM-modulated 2400 bps FSK (2400 Hz mark / 1200 Hz space subcarrier).
 * VHF frequencies: 129.125, 131.550, 136.900 MHz (and others).
 * Output: DemodClass::Bits containing decoded ACARS message text.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class AcarsDemod : public IDemod {
public:
    explicit AcarsDemod() = default;
    ~AcarsDemod() override = default;

    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
