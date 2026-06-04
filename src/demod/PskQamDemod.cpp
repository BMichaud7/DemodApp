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
#include "demod/PskQamDemod.hpp"
#include <liquid/liquid.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

namespace demod {

static modulation_scheme schemeFor(const std::string& mod) {
    static const std::unordered_map<std::string, modulation_scheme> table{
        {"BPSK",   LIQUID_MODEM_BPSK},
        {"QPSK",   LIQUID_MODEM_QPSK},
        {"8PSK",   LIQUID_MODEM_PSK8},
        {"16PSK",  LIQUID_MODEM_PSK16},
        {"32PSK",  LIQUID_MODEM_PSK32},
        {"QAM16",  LIQUID_MODEM_QAM16},
        {"QAM32",  LIQUID_MODEM_QAM32},
        {"QAM64",  LIQUID_MODEM_QAM64},
        {"QAM256", LIQUID_MODEM_QAM256},
        {"4ASK",   LIQUID_MODEM_ASK4},
        {"16ASK",  LIQUID_MODEM_ASK16},
    };
    auto it = table.find(mod);
    return (it != table.end()) ? it->second : LIQUID_MODEM_QPSK;
}


PskQamDemod::PskQamDemod(const std::string& modulation,
                         au::QuantityD<au::Hertz> symbol_rate_hint_sps)
    : mod_(modulation), sym_rate_hint_(symbol_rate_hint_sps.in(au::hertz)) {}

PskQamDemod::~PskQamDemod() = default;

DemodResult PskQamDemod::process(const std::vector<std::complex<float>>& iq,
                                  au::QuantityD<au::Hertz>   sr,
                                  au::QuantityD<au::Hertz>   center_freq,
                                  au::QuantityD<au::Seconds> timestamp)
{
    // Extract raw values for DSP math
    const double sr_sps         = sr.in(au::hertz);
    const double center_freq_hz = center_freq.in(au::hertz);

    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = mod_;
    r.center_freq = center_freq;
    r.timestamp_ms = static_cast<int64_t>(timestamp.in(au::seconds) * 1000.0);
    r.duration    = au::seconds(iq.size() / sr_sps);

    modulation_scheme scheme = schemeFor(mod_);

    // Estimate samples/symbol from hint or bandwidth heuristic
    double sym_rate = (sym_rate_hint_ > 0) ? sym_rate_hint_ : sr_sps / 4.0;
    unsigned int k = static_cast<unsigned int>(std::round(sr_sps / sym_rate));
    k = std::clamp(k, 2u, 32u);

    // RRC filter parameters
    constexpr unsigned int m    = 5;    // filter half-length (symbols)
    constexpr float        beta = 0.35f; // roll-off
    constexpr unsigned int npfb = 32;   // polyphase filter banks

    // ── Timing recovery ───────────────────────────────────────────────────────
    symsync_crcf sync = symsync_crcf_create_rnyquist(
        LIQUID_FIRFILT_ARKAISER, k, m, beta, npfb);
    symsync_crcf_set_lf_bw(sync, 0.02f);

    // ── Carrier recovery (decision-directed Costas loop) ─────────────────────
    nco_crcf carrier = nco_crcf_create(LIQUID_VCO);
    nco_crcf_pll_set_bandwidth(carrier, 0.02f);

    // ── Symbol demodulator ────────────────────────────────────────────────────
    modem dem = modem_create(scheme);
    int bps = static_cast<int>(modem_get_bps(dem));
    r.bits_per_symbol = bps;
    r.sample_rate     = au::hertz(sym_rate);

    r.bits.reserve(static_cast<size_t>(iq.size() / k * bps / 8 + 8));
    uint8_t byte_acc = 0;
    int     bit_pos  = 7;

    // Process sample by sample through timing recovery
    std::vector<liquid_float_complex> sym_buf(k + m + 4);
    auto* in = reinterpret_cast<liquid_float_complex*>(
                   const_cast<std::complex<float>*>(iq.data()));

    for (unsigned int i = 0; i < iq.size(); ++i) {
        // 1. Mix down (carrier removal)
        liquid_float_complex x_down;
        nco_crcf_mix_down(carrier, in[i], &x_down);
        nco_crcf_step(carrier);

        // 2. Timing recovery: push one sample
        unsigned int n_sym = 0;
        symsync_crcf_execute(sync, &x_down, 1, sym_buf.data(), &n_sym);

        for (unsigned int s = 0; s < n_sym; ++s) {
            // 3. Demodulate
            unsigned int sym_out = 0;
            modem_demodulate(dem, sym_buf[s], &sym_out);

            // 4. Update carrier PLL from decision-directed phase error
            float phase_err = modem_get_demodulator_phase_error(dem);
            nco_crcf_pll_step(carrier, phase_err);

            // 5. Pack bits (MSB first within each symbol)
            for (int b = bps - 1; b >= 0; --b) {
                int bit = (sym_out >> b) & 1;
                byte_acc |= (bit << bit_pos);
                if (--bit_pos < 0) {
                    r.bits.push_back(byte_acc);
                    byte_acc = 0;
                    bit_pos  = 7;
                }
            }
        }
    }
    if (bit_pos < 7) r.bits.push_back(byte_acc);

    symsync_crcf_destroy(sync);
    nco_crcf_destroy(carrier);
    modem_destroy(dem);

    spdlog::info("PskQamDemod: {:.3f} MHz {} k={} → {} bytes",
                 center_freq_hz / 1e6, mod_, k, r.bits.size());
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
