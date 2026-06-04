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
 * @file DtmfDemod.hpp
 * @brief DTMF (Dual-Tone Multi-Frequency) detector using Goertzel algorithm. Digits 0-9 A-D * #.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class DtmfDemod : public IDemod {
public:
    explicit DtmfDemod() = default;
    ~DtmfDemod() override = default;
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
