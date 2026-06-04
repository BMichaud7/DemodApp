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
 * @file NxdnDemod.hpp
 * @brief NXDN digital voice demodulator — returns raw bit stream.
 *
 * NXDN: FDMA 4FSK, 4800 sym/s, ±1050/±3150 Hz deviation, 12.5 kHz channel.
 * Used by Icom (IDAS) and Kenwood (NEXEDGE) in industrial and public safety.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class NxdnDemod : public IDemod {
public:
    explicit NxdnDemod() = default;
    ~NxdnDemod() override = default;

    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
