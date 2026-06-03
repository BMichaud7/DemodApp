/**
 * @file Psk31Demod.hpp
 * @brief PSK31/PSK63 HF amateur digital (BPSK 31 baud, Varicode). Decoded text output.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

class Psk31Demod : public IDemod {
public:
    explicit Psk31Demod() = default;
    ~Psk31Demod() override = default;
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;
};

} // namespace demod
