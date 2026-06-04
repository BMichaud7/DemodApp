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
 * @file AdsbDemod.hpp
 * @brief ADS-B Mode S aircraft transponder decoder (PPM at 1090 MHz). ICAO + position.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class AdsbDemod : public IDemod {
public:
    explicit AdsbDemod() = default;
    ~AdsbDemod() override = default;
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
