/**
 * @file TestSignals.hpp
 * @brief Header-only helpers for generating synthetic IQ test signals.
 */
#pragma once
#include <complex>
#include <cmath>
#include <vector>

namespace TestSignals {

/**
 * @brief Generate a wide-band FM modulated IQ signal.
 *
 * Integrates a sine tone to produce the instantaneous phase, then
 * creates exp(j*phase) samples.
 *
 * @param sr           Sample rate in Hz.
 * @param dur_s        Duration in seconds.
 * @param audio_hz     Modulating tone frequency in Hz.
 * @param deviation_hz Peak FM deviation in Hz.
 * @return IQ samples at the given sample rate.
 */
static inline std::vector<std::complex<float>>
makeFm(double sr, double dur_s, double audio_hz, double deviation_hz)
{
    size_t N = static_cast<size_t>(sr * dur_s);
    std::vector<std::complex<float>> iq(N);
    double kf    = deviation_hz / sr;
    double phase = 0.0;
    for (size_t i = 0; i < N; ++i) {
        double audio = std::sin(2.0 * M_PI * audio_hz * static_cast<double>(i) / sr);
        phase += 2.0 * M_PI * kf * audio;
        iq[i] = std::complex<float>(static_cast<float>(std::cos(phase)),
                                    static_cast<float>(std::sin(phase)));
    }
    return iq;
}

/**
 * @brief Generate an AM DSB (double-sideband with carrier) IQ signal at baseband.
 *
 * Signal: (1 + mod_index * sin(2π*fa*t)) — real part only; imaginary = 0.
 *
 * @param sr        Sample rate in Hz.
 * @param dur_s     Duration in seconds.
 * @param audio_hz  Modulating tone frequency in Hz.
 * @param mod_index Modulation index [0, 1] (default 0.8).
 * @return IQ samples representing the AM DSB signal.
 */
static inline std::vector<std::complex<float>>
makeAmDsb(double sr, double dur_s, double audio_hz, double mod_index = 0.8)
{
    size_t N = static_cast<size_t>(sr * dur_s);
    std::vector<std::complex<float>> iq(N);
    for (size_t i = 0; i < N; ++i) {
        float s = static_cast<float>(
            1.0 + mod_index * std::sin(2.0 * M_PI * audio_hz * static_cast<double>(i) / sr));
        iq[i] = std::complex<float>(s, 0.0f);
    }
    return iq;
}

/**
 * @brief Generate a binary FSK IQ signal with alternating 0/1 symbols.
 *
 * Symbol 0 uses tone at −deviation_hz, symbol 1 uses +deviation_hz.
 *
 * @param sr           Sample rate in Hz.
 * @param dur_s        Duration in seconds.
 * @param deviation_hz Frequency deviation from centre in Hz.
 * @param baud_rate    Symbol rate in symbols/second.
 * @return IQ samples of the FSK signal.
 */
static inline std::vector<std::complex<float>>
makeFsk(double sr, double dur_s, double deviation_hz, double baud_rate)
{
    size_t N   = static_cast<size_t>(sr * dur_s);
    int    sps = static_cast<int>(sr / baud_rate);
    std::vector<std::complex<float>> iq(N);
    double phase = 0.0;
    for (size_t i = 0; i < N; ++i) {
        int sym = (static_cast<int>(i) / sps) % 2; // alternating 0/1
        double freq = (sym == 0) ? -deviation_hz : +deviation_hz;
        phase += 2.0 * M_PI * freq / sr;
        iq[i] = std::complex<float>(static_cast<float>(std::cos(phase)),
                                    static_cast<float>(std::sin(phase)));
    }
    return iq;
}

/**
 * @brief Generate a BPSK IQ signal with alternating 0/1 symbols.
 *
 * Symbol mapping: 0 → +1, 1 → −1.  Rectangular pulse shaping (no RRC).
 *
 * @param sr       Sample rate in Hz.
 * @param dur_s    Duration in seconds.
 * @param sym_rate Symbol rate in symbols/second.
 * @return IQ samples of the BPSK signal.
 */
static inline std::vector<std::complex<float>>
makeBpsk(double sr, double dur_s, double sym_rate)
{
    size_t N   = static_cast<size_t>(sr * dur_s);
    int    sps = std::max(1, static_cast<int>(sr / sym_rate));
    std::vector<std::complex<float>> iq(N);
    for (size_t i = 0; i < N; ++i) {
        int sym  = (static_cast<int>(i) / sps) % 2;
        float s  = (sym == 0) ? 1.0f : -1.0f;
        iq[i]    = std::complex<float>(s, 0.0f);
    }
    return iq;
}

/**
 * @brief Generate a CW (Morse) IQ signal spelling "SOS" (... --- ...).
 *
 * The carrier is a simple unit-amplitude complex exponential at ~400 Hz
 * keyed on/off according to the SOS Morse pattern.
 *
 * @param sr          Sample rate in Hz.
 * @param dit_samples Number of samples per dit element.
 * @return IQ samples of the keyed CW signal.
 */
static inline std::vector<std::complex<float>>
makeCw(double sr, int dit_samples)
{
    // SOS pattern: S=... O=--- S=...
    // Encoded as sequence of (key_down, duration_in_dits):
    //   dit=1, dah=3, intra-element gap=1, letter gap=3, word gap=7
    // SOS with proper spacing:
    //  S: . _ . _ .   (dit, gap, dit, gap, dit)
    //  letter gap
    //  O: - _ - _ -   (dah, gap, dah, gap, dah)
    //  letter gap
    //  S: . _ . _ .
    struct Element { bool key; int dits; };
    std::vector<Element> seq = {
        // S
        {true, 1}, {false, 1}, {true, 1}, {false, 1}, {true, 1},
        // letter gap
        {false, 3},
        // O
        {true, 3}, {false, 1}, {true, 3}, {false, 1}, {true, 3},
        // letter gap
        {false, 3},
        // S
        {true, 1}, {false, 1}, {true, 1}, {false, 1}, {true, 1},
        // trailing silence
        {false, 7},
    };

    size_t total = 0;
    for (auto& e : seq)
        total += static_cast<size_t>(e.dits * dit_samples);

    std::vector<std::complex<float>> iq(total);
    double carrier_hz = 400.0; // place carrier slightly off DC
    size_t pos = 0;
    for (auto& e : seq) {
        size_t len = static_cast<size_t>(e.dits * dit_samples);
        for (size_t i = 0; i < len; ++i) {
            double t = static_cast<double>(pos + i) / sr;
            float re = e.key ? static_cast<float>(std::cos(2.0 * M_PI * carrier_hz * t)) : 0.0f;
            float im = e.key ? static_cast<float>(std::sin(2.0 * M_PI * carrier_hz * t)) : 0.0f;
            iq[pos + i] = std::complex<float>(re, im);
        }
        pos += len;
    }
    return iq;
}

/**
 * @brief Generate a Bell 202 AFSK IQ signal for a given bit sequence.
 *
 * FM-modulates alternating mark (1200 Hz) or space (2200 Hz) audio tones onto
 * a baseband carrier, matching the kf used by AfskDemod (deviation = 1200 Hz).
 * Phase is continuous across symbol boundaries.
 *
 * @param sr   Sample rate in Hz (typically 9600).
 * @param bits Bit sequence: 1 = mark (1200 Hz), 0 = space (2200 Hz).
 * @return IQ samples of the FM-modulated AFSK signal.
 */
static inline std::vector<std::complex<float>>
makeAfsk(double sr, const std::vector<int>& bits)
{
    constexpr double k_mark  = 1200.0;
    constexpr double k_space = 2200.0;
    constexpr double k_baud  = 1200.0;
    const int sps = std::max(1, static_cast<int>(std::round(sr / k_baud)));
    const double kf = 1200.0 / sr; // matches AfskDemod's kf

    const size_t N = bits.size() * static_cast<size_t>(sps);
    std::vector<std::complex<float>> iq(N);

    double fm_phase    = 0.0;
    double audio_phase = 0.0;
    size_t idx = 0;
    for (int bit : bits) {
        double tone_hz = (bit != 0) ? k_mark : k_space;
        for (int i = 0; i < sps; ++i) {
            double audio  = std::sin(audio_phase);
            audio_phase  += 2.0 * M_PI * tone_hz / sr;
            fm_phase     += 2.0 * M_PI * kf * audio;
            iq[idx++] = std::complex<float>(
                static_cast<float>(std::cos(fm_phase)),
                static_cast<float>(std::sin(fm_phase)));
        }
    }
    return iq;
}

} // namespace TestSignals
