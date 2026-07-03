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
#include "demod/DmrDemod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <vector>
#include <spdlog/spdlog.h>

namespace demod {

static constexpr double DMR_SYMBOL_RATE = 4800.0;
static constexpr double DMR_DEV_OUTER   = 1944.0;  // Hz
static constexpr double DMR_MOD_INDEX   = DMR_DEV_OUTER / DMR_SYMBOL_RATE;

// Slice freqdem output (normalised to ±1 for outer deviation) to a DMR dibit.
// Inner deviation is 1/3 of outer (±648 Hz vs ±1944 Hz).  The threshold between
// inner and outer is at the midpoint: (1/3 + 1) / 2 ≈ 0.667.
static uint8_t slice_dmr(float s) {
    if (s >  0.667f) return 1;  // +outer
    if (s >  0.0f)   return 0;  // +inner
    if (s > -0.667f) return 2;  // -inner
    return 3;                    // -outer
}

DemodResult DmrDemod::process(const std::vector<std::complex<float>>& iq,
                               au::QuantityD<au::Hertz>   sr,
                               au::QuantityD<au::Hertz>   center_freq,
                               au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type         = DemodClass::Bits;
    r.modulation   = "DMR";
    r.center_freq  = center_freq;
    r.sample_rate  = au::hertz(DMR_SYMBOL_RATE);
    r.timestamp_ms = static_cast<int64_t>(timestamp.in(au::seconds) * 1000);
    r.duration     = sr.in(au::hertz) > 0
                   ? au::seconds(iq.size() / sr.in(au::hertz)) : au::seconds(0.0);

    if (iq.empty()) return r;

    float sps = static_cast<float>(sr.in(au::hertz) / DMR_SYMBOL_RATE);

    // kf = deviation / sample_rate (not deviation / symbol_rate).
    float kf_dmr = std::clamp(static_cast<float>(DMR_DEV_OUTER / sr.in(au::hertz)), 0.01f, 0.49f);
    freqdem fdem = freqdem_create(kf_dmr);
    symsync_rrrf symsync = symsync_rrrf_create_rnyquist(
        LIQUID_FIRFILT_RRC, static_cast<unsigned>(std::round(sps)), 5, 0.2f, 32);
    symsync_rrrf_set_lf_bw(symsync, 0.01f);

    std::vector<uint8_t> bits;
    bits.reserve(iq.size() / static_cast<int>(sps) * 2 + 16);

    float sym_out[4]; unsigned n_sym;
    for (const auto& s : iq) {
        float fd;
        freqdem_demodulate(fdem, s, &fd);
        symsync_rrrf_execute(symsync, &fd, 1, sym_out, &n_sym);
        for (unsigned i = 0; i < n_sym; ++i) {
            uint8_t dibit = slice_dmr(sym_out[i]);
            bits.push_back((dibit >> 1) & 1);
            bits.push_back( dibit       & 1);
        }
    }
    freqdem_destroy(fdem);
    symsync_rrrf_destroy(symsync);

    // Pack bits into bytes
    r.bits.reserve((bits.size() + 7) / 8);
    for (size_t i = 0; i + 8 <= bits.size(); i += 8) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; ++b) byte |= static_cast<uint8_t>(bits[i+b] << (7-b));
        r.bits.push_back(byte);
    }

    spdlog::debug("DmrDemod: {} symbols → {} bytes", bits.size()/2, r.bits.size());
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
