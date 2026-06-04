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
#include "demod/FskDemod.hpp"
#include <liquid/liquid.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

namespace demod {

FskDemod::FskDemod(unsigned int m_ary, bool is_ook)
    : m_ary_(m_ary), is_ook_(is_ook) {}

FskDemod::~FskDemod() = default;

DemodResult FskDemod::process(const std::vector<std::complex<float>>& iq,
                               au::QuantityD<au::Hertz>   sr,
                               au::QuantityD<au::Hertz>   center_freq,
                               au::QuantityD<au::Seconds> timestamp)
{
    // Extract raw values for DSP math
    const double sr_sps         = sr.in(au::hertz);
    const double center_freq_hz = center_freq.in(au::hertz);

    DemodResult r;
    r.type        = DemodClass::Bits;
    r.center_freq = center_freq;
    r.timestamp_ms = static_cast<int64_t>(timestamp.in(au::seconds) * 1000.0);
    r.duration    = au::seconds(iq.size() / sr_sps);

    const int bps = static_cast<int>(std::log2(m_ary_));  // bits per symbol
    r.bits_per_symbol = bps;

    if (is_ook_) {
        r.modulation = "OOK";
        // Envelope threshold detection: above median → 1, below → 0
        std::vector<float> env(iq.size());
        for (size_t i = 0; i < iq.size(); ++i)
            env[i] = std::abs(iq[i]);

        // Rough symbol clock: assume k=8 samples/symbol
        constexpr int k = 8;
        int n_sym = static_cast<int>(iq.size()) / k;

        // Compute threshold as median of envelope
        std::vector<float> sorted_env = env;
        std::nth_element(sorted_env.begin(),
                         sorted_env.begin() + sorted_env.size() / 2,
                         sorted_env.end());
        float threshold = sorted_env[sorted_env.size() / 2];

        r.bits.reserve(static_cast<size_t>((n_sym + 7) / 8));
        uint8_t byte_acc = 0;
        int bit_pos = 7;
        for (int s = 0; s < n_sym; ++s) {
            // Average envelope over the symbol period
            float avg = 0;
            for (int j = 0; j < k; ++j)
                avg += env[static_cast<size_t>(s * k + j)];
            avg /= k;
            int bit = (avg >= threshold) ? 1 : 0;
            byte_acc |= (bit << bit_pos);
            if (--bit_pos < 0) {
                r.bits.push_back(byte_acc);
                byte_acc = 0;
                bit_pos  = 7;
            }
        }
        if (bit_pos < 7) r.bits.push_back(byte_acc);
        r.sample_rate = au::hertz(sr_sps / k);

    } else {
        r.modulation = (m_ary_ == 2) ? "FSK" : (m_ary_ == 4) ? "4FSK" : "8FSK";

        // k = samples/symbol. Use 8 × M as a heuristic; clamped to [4, 64].
        unsigned int k = std::clamp(8u * m_ary_, 4u, 64u);
        float bw = 0.25f;  // normalized bandwidth for fskdem

        fskdem demod = fskdem_create(m_ary_, k, bw);

        int n_sym = static_cast<int>(iq.size()) / static_cast<int>(k);
        r.bits.reserve(static_cast<size_t>((n_sym * bps + 7) / 8));

        uint8_t byte_acc = 0;
        int bit_pos = 7;

        auto* buf = reinterpret_cast<liquid_float_complex*>(
                        const_cast<std::complex<float>*>(iq.data()));

        for (int s = 0; s < n_sym; ++s) {
            unsigned int sym = fskdem_demodulate(demod, buf + s * k);
            // Pack bits MSB-first within each symbol
            for (int b = bps - 1; b >= 0; --b) {
                int bit = (sym >> b) & 1;
                byte_acc |= (bit << bit_pos);
                if (--bit_pos < 0) {
                    r.bits.push_back(byte_acc);
                    byte_acc = 0;
                    bit_pos  = 7;
                }
            }
        }
        if (bit_pos < 7) r.bits.push_back(byte_acc);
        fskdem_destroy(demod);
        r.sample_rate = au::hertz(sr_sps / k);  // effective symbol rate
    }

    spdlog::info("FskDemod: {:.3f} MHz {} → {} bytes",
                 center_freq_hz / 1e6, r.modulation, r.bits.size());
    return r;
}

} // namespace demod

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
