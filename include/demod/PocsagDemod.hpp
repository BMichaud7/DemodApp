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
 * @file PocsagDemod.hpp
 * @brief POCSAG paging protocol demodulator.
 *
 * POCSAG: 2-FSK, ±4500 Hz deviation.
 * Baud rates: 512, 1200, 2400 bps (auto-detected).
 * Decodes numeric and alphanumeric messages.
 * Output: DemodClass::Bits with decoded pager message text.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class PocsagDemod : public IDemod {
public:
    explicit PocsagDemod() = default;
    ~PocsagDemod() override = default;

    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
