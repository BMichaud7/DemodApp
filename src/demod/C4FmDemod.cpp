/**
 * @file C4FmDemod.cpp
 * @brief C4FM demodulator using liquid-dsp.
 *
 * C4FM: 4-level FSK, ±600/±1800 Hz deviation, 4800 sym/s = 9600 bps.
 * Approach:
 *   1. FM discriminator (freqdem) — converts instantaneous frequency
 *      deviations to a baseband real signal.
 *   2. Matched filter / symbol synchroniser (symsync_rrrf) — recovers
 *      symbol timing at exactly 4800 samples/symbol after interpolation.
 *   3. 4-level slicer — maps deviations to dibits:
 *        +1800 Hz → dibit 01 (positive outer)
 *         +600 Hz → dibit 00 (positive inner)
 *         −600 Hz → dibit 10 (negative inner)
 *        −1800 Hz → dibit 11 (negative outer)
 *      (per TIA-102.BAAA dibits, MSB first)
 */
#include "demod/C4FmDemod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <stdexcept>

namespace demod::p25 {

struct C4FmDemod::Impl {
    double sample_rate;
    double symbol_rate;

    freqdem   fdem   = nullptr;
    symsync_rrrf symsync = nullptr;

    // FM modulation index = max_dev / symbol_rate = 1800/4800 = 0.375
    static constexpr float MOD_INDEX = 1800.0f / 4800.0f;

    Impl(double sr, double sym_rate) : sample_rate(sr), symbol_rate(sym_rate) {
        float sps = static_cast<float>(sr / sym_rate);

        // FM discriminator
        fdem = freqdem_create(MOD_INDEX);

        // Symbol synchroniser (RRC filter, 5 taps/side, β=0.2)
        symsync = symsync_rrrf_create_rnyquist(
            LIQUID_FIRFILT_RRC,
            static_cast<unsigned>(std::round(sps)),
            5,
            0.2f,
            32);
        symsync_rrrf_set_lf_bw(symsync, 0.01f);
    }

    ~Impl() {
        if (fdem)    freqdem_destroy(fdem);
        if (symsync) symsync_rrrf_destroy(symsync);
    }

    // Slice a demodulated FM sample to a dibit (0–3)
    static uint8_t slice(float s) {
        // Normalise: inner threshold at 0.5 (±600/1800 → ±0.333/±1.0)
        if (s > 0.666f) return 0b01;   // +outer → 01
        if (s > 0.0f)   return 0b00;   // +inner → 00
        if (s > -0.666f)return 0b10;   // -inner → 10
        return 0b11;                    // -outer → 11
    }
};

C4FmDemod::C4FmDemod(double sample_rate, double symbol_rate)
    : impl_(std::make_unique<Impl>(sample_rate, symbol_rate)) {}

C4FmDemod::~C4FmDemod() = default;

std::vector<uint8_t> C4FmDemod::process(
    const std::vector<std::complex<float>>& samples)
{
    std::vector<uint8_t> dibits;
    dibits.reserve(samples.size() / static_cast<int>(impl_->sample_rate / impl_->symbol_rate) + 4);

    // Temporary output buffer for symsync (up to 4 symbols per input sample)
    float sym_out[4];
    unsigned n_sym;

    for (const auto& s : samples) {
        // Step 1: FM discriminate → instantaneous frequency (normalised)
        float freq_sample;
        freqdem_demodulate(impl_->fdem, s, &freq_sample);

        // Step 2: Symbol synchronisation
        symsync_rrrf_execute(impl_->symsync, &freq_sample, 1, sym_out, &n_sym);

        // Step 3: Slice to dibits
        for (unsigned i = 0; i < n_sym; ++i)
            dibits.push_back(Impl::slice(sym_out[i]));
    }
    return dibits;
}

void C4FmDemod::reset() {
    freqdem_reset(impl_->fdem);
    symsync_rrrf_reset(impl_->symsync);
}

} // namespace demod::p25
