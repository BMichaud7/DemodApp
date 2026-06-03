/**
 * @file NavtexDemod.hpp
 * @brief NAVTEX maritime safety broadcasts (FSK 100 baud, SITOR-B/ITA-3, 518/490 kHz).
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class NavtexDemod : public IDemod {
public:
    explicit NavtexDemod() = default;
    ~NavtexDemod() override = default;
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
