/**
 * @file FlexDemod.hpp
 * @brief FLEX paging (4-FSK 1600/3200/6400 baud). Alphanumeric, numeric, voice-alert messages.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class FlexDemod : public IDemod {
public:
    explicit FlexDemod() = default;
    ~FlexDemod() override = default;
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
