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
 * @file EasSameDemod.hpp
 * @brief EAS/SAME Emergency Alert System decoder (FSK 520 baud AFSK). ZCZC header decode.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class EasSameDemod : public IDemod {
public:
    explicit EasSameDemod() = default;
    ~EasSameDemod() override = default;
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
