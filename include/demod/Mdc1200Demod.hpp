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
 * @file Mdc1200Demod.hpp
 * @brief MDC-1200 Motorola PTT ID protocol (2-FSK 1200 baud). Radio unit ID, emergency, status.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class Mdc1200Demod : public IDemod {
public:
    explicit Mdc1200Demod() = default;
    ~Mdc1200Demod() override = default;
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
