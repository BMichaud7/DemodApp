/**
 * @file DscDemod.hpp
 * @brief DSC Digital Selective Calling marine safety (GMSK 1200 baud). MMSI + distress type.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class DscDemod : public IDemod {
public:
    explicit DscDemod() = default;
    ~DscDemod() override = default;
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
