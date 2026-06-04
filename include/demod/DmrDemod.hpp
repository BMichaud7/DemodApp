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
 * @file DmrDemod.hpp
 * @brief DMR (Digital Mobile Radio) demodulator — returns raw bit stream.
 *
 * DMR: TDMA 2-slot, 4FSK, 4800 sym/s (9600 bps), ±648/±1944 Hz deviation.
 * Common in commercial and public safety (MOTOTRBO, Hytera, etc.).
 *
 * Output: DemodClass::Bits containing the raw dibit stream.
 * AMBE+2 voice decoding requires a separate codec (not included).
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class DmrDemod : public IDemod {
public:
    explicit DmrDemod() = default;
    ~DmrDemod() override = default;

    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
