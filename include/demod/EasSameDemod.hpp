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
