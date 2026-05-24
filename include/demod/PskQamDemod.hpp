#pragma once
#include "IDemod.hpp"
#include <string>

namespace demod {

// Coherent PSK/QAM/ASK demodulator using liquid-dsp symsync + Costas loop.
// Handles: BPSK, QPSK, 8PSK, 16PSK, 32PSK,
//          QAM16, QAM32, QAM64, QAM256,
//          4ASK, 16ASK
class PskQamDemod : public IDemod {
public:
    explicit PskQamDemod(const std::string& modulation,
                         double symbol_rate_hint_sps = 0.0);
    ~PskQamDemod() override;

    DemodResult process(const std::vector<std::complex<float>>& iq,
                        double sr_sps, double center_freq_hz,
                        int64_t timestamp_ms) override;

private:
    std::string mod_;
    double      sym_rate_hint_;
};

} // namespace demod
