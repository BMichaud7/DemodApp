#include "demod/AfskDemod.hpp"
#include <liquid/liquid.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <vector>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

namespace demod {

// Bell 202 constants
static constexpr double k_mark_hz  = 1200.0;
static constexpr double k_space_hz = 2200.0;
static constexpr double k_baud     = 1200.0;

AfskDemod::AfskDemod() = default;
AfskDemod::~AfskDemod() = default;

DemodResult AfskDemod::process(const std::vector<std::complex<float>>& iq,
                                double sr_sps,
                                double center_freq_hz,
                                int64_t timestamp_ms)
{
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "AFSK";
    r.center_freq = au::hertz(center_freq_hz);
    r.sample_rate = au::hertz(sr_sps);
    r.timestamp_ms = timestamp_ms;
    r.duration    = au::seconds(iq.empty() ? 0.0 : iq.size() / sr_sps);

    if (iq.empty()) return r;

    // ── Step 1: FM-discriminate IQ → audio ──────────────────────────────────
    // Use kf tuned for a ±1200 Hz deviation (space tone is 2200 Hz, so
    // worst-case deviation from centre ≈ 1000 Hz; use 1200 Hz for margin).
    float kf = static_cast<float>(1200.0 / sr_sps);
    kf = std::clamp(kf, 0.01f, 0.49f);

    freqdem fm = freqdem_create(kf);
    if (!fm) throw std::runtime_error("AfskDemod: freqdem_create failed");

    const size_t N = iq.size();
    std::vector<float> audio(N);
    freqdem_demodulate_block(fm,
        reinterpret_cast<liquid_float_complex*>(
            const_cast<std::complex<float>*>(iq.data())),
        static_cast<unsigned int>(N),
        audio.data());
    freqdem_destroy(fm);

    // ── Step 2: samples per symbol ──────────────────────────────────────────
    const int k = static_cast<int>(std::round(sr_sps / k_baud));
    if (k < 2) {
        spdlog::warn("AfskDemod: sr_sps={:.0f} too low for 1200 baud", sr_sps);
        return r;
    }

    // ── Steps 3–4: correlate each symbol window ──────────────────────────────
    // Compute correlator coefficients outside loop for efficiency.
    // mark_corr  = |Σ audio[i] * exp(-j2π*1200*i/sr)|²
    // space_corr = |Σ audio[i] * exp(-j2π*2200*i/sr)|²
    const double tw_mark  = -2.0 * M_PI * k_mark_hz  / sr_sps;
    const double tw_space = -2.0 * M_PI * k_space_hz / sr_sps;

    std::vector<uint8_t> raw_bits;
    raw_bits.reserve(static_cast<size_t>(N / k + 1));

    size_t pos = 0;
    while (pos + static_cast<size_t>(k) <= N) {
        std::complex<double> mark_acc(0.0, 0.0);
        std::complex<double> space_acc(0.0, 0.0);
        for (int i = 0; i < k; ++i) {
            double s = static_cast<double>(audio[pos + static_cast<size_t>(i)]);
            double phi_m = tw_mark  * i;
            double phi_s = tw_space * i;
            mark_acc  += s * std::complex<double>(std::cos(phi_m), std::sin(phi_m));
            space_acc += s * std::complex<double>(std::cos(phi_s), std::sin(phi_s));
        }
        double mc = std::norm(mark_acc);
        double sc = std::norm(space_acc);
        // Bell 202: mark=1, space=0
        raw_bits.push_back(sc > mc ? 0 : 1);
        pos += static_cast<size_t>(k);
    }

    // ── Step 5: pack bits → bytes MSB-first ────────────────────────────────
    size_t n_bits = raw_bits.size();
    r.bits.reserve((n_bits + 7) / 8);
    for (size_t b = 0; b + 8 <= n_bits; b += 8) {
        uint8_t byte = 0;
        for (int i = 0; i < 8; ++i)
            byte = static_cast<uint8_t>((byte << 1) | raw_bits[b + static_cast<size_t>(i)]);
        r.bits.push_back(byte);
    }

    spdlog::info("AfskDemod: {:.3f} MHz → {} bits ({} bytes) k={}",
                 center_freq_hz / 1e6, n_bits, r.bits.size(), k);
    return r;
}

} // namespace demod
