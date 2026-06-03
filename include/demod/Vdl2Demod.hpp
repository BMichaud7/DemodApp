/**
 * @file Vdl2Demod.hpp
 * @brief VDL Mode 2 aviation digital datalink (D8PSK 31.5 kbps). Aircraft datalink messages.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class Vdl2Demod : public IDemod {
public:
    explicit Vdl2Demod() = default;
    ~Vdl2Demod() override = default;
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
