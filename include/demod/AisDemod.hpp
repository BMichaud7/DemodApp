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
 * @file AisDemod.hpp
 * @brief AIS (Automatic Identification System) demodulator.
 *
 * AIS: GMSK, 9600 bps, BT=0.4, 25 kHz channel.
 * Marine VHF channels 87B (161.975 MHz) and 88B (162.025 MHz).
 * Output: DemodClass::Bits containing NMEA-style decoded sentences
 *         (AIVDM/AIVDO format) where possible, raw bits otherwise.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class AisDemod : public IDemod {
public:
    explicit AisDemod() = default;
    ~AisDemod() override = default;

    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
