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
 * @file DstarDemod.hpp
 * @brief D-STAR digital amateur voice demodulator — returns raw bit stream.
 *
 * D-STAR: GMSK, 4800 bps (data) / AMBE voice at 3600 bps.
 * BT=0.5 Gaussian filter, 12.5 kHz channel spacing.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class DstarDemod : public IDemod {
public:
    explicit DstarDemod() = default;
    ~DstarDemod() override = default;

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
