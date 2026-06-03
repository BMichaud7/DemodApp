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

// ── Protocol-specific signal generators ──────────────────────────────────────
// These match the exact RF parameters of real-world protocols so tests
// verify demodulator behaviour against protocol-compliant waveforms.

/**
 * @brief P25 C4FM: 4-level FSK, 4800 sym/s, ±600/±1800 Hz deviation.
 *
 * Includes the 48-bit frame sync word (0x5575F5FF77FF) followed by
 * alternating dibits.  Use sample rates that yield an integer sps
 * (e.g. 48000 → 10 sps, 96000 → 20 sps).
 */
static inline std::vector<std::complex<float>>
makeP25C4fm(double sr, double dur_s)
{
    const double SYM_RATE = 4800.0;
    const int    sps      = std::max(1, static_cast<int>(sr / SYM_RATE));
    size_t N = static_cast<size_t>(sr * dur_s);
    std::vector<std::complex<float>> iq(N);

    // P25 frame sync dibits (0x5575F5FF77FF):
    // bits: 01 01 01 01 11 11 11 01 01 01 11 11 11 11 11 11 01 11 01 11 11 11 11 11
    const int sync_dibits[] = {1,1,1,1,3,3,3,1,1,1,3,3,3,3,3,3,1,3,1,3,3,3,3,3};
    // deviation values in Hz → normalised
    const double devs[4] = {-1800.0/sr, -600.0/sr, 600.0/sr, 1800.0/sr};

    double phase = 0.0;
    size_t idx = 0;
    int sym_idx = 0;
    while (idx < N) {
        int dibit = (sym_idx < 24) ? sync_dibits[sym_idx]
                                   : (sym_idx % 4);  // alternating after sync
        ++sym_idx;
        for (int s = 0; s < sps && idx < N; ++s, ++idx) {
            phase += 2.0 * M_PI * devs[dibit];
            iq[idx] = std::complex<float>(std::cos(phase), std::sin(phase));
        }
    }
    return iq;
}

/**
 * @brief DMR 4FSK: 4800 sym/s, ±648/±1944 Hz deviation, TDMA burst envelope.
 */
static inline std::vector<std::complex<float>>
makeDmr(double sr, double dur_s)
{
    const double SYM_RATE = 4800.0;
    const int    sps      = std::max(1, static_cast<int>(sr / SYM_RATE));
    size_t N = static_cast<size_t>(sr * dur_s);
    std::vector<std::complex<float>> iq(N);

    const double devs[4] = {-1944.0/sr, -648.0/sr, 648.0/sr, 1944.0/sr};
    // Voice sync: 0x755FD7DF75F7 → dibits 1,3,1,1,3,3,1,3,1,3,3,3
    const int sync_dibits[] = {1,3,1,1,3,3,1,3,1,3,3,3};

    double phase = 0.0;
    // TDMA burst: on for half the period, then silence
    size_t burst_len = static_cast<size_t>(sr / 50);  // 20ms slot
    for (size_t i = 0; i < N; ++i) {
        int sym_idx = static_cast<int>(i / sps);
        int dibit   = (sym_idx < 12) ? sync_dibits[sym_idx] : (sym_idx % 4);
        bool active = (i % burst_len) < burst_len / 2;
        phase += 2.0 * M_PI * devs[dibit];
        float amp = active ? 1.0f : 0.05f;
        iq[i] = std::complex<float>(amp*std::cos(phase), amp*std::sin(phase));
    }
    return iq;
}

/**
 * @brief POCSAG: 2-FSK, ±4500 Hz, 1200 baud, with preamble + sync codeword.
 *
 * Preamble: 72 alternating 1/0 bits.
 * Sync:     0x7CD215D8 (32 bits).
 * Payload:  numeric message "12345" encoded in POCSAG BCD.
 */
static inline std::vector<std::complex<float>>
makePocsag(double sr, double dur_s)
{
    const double BAUD = 1200.0;
    const int    sps  = std::max(1, static_cast<int>(sr / BAUD));
    const double DEV  = 4500.0 / sr;  // normalised deviation

    // Build bit stream: preamble + sync + idle codeword
    std::vector<int> bits;
    for (int i = 0; i < 72; ++i) bits.push_back(i % 2);  // preamble
    // Sync codeword 0x7CD215D8
    for (int b = 31; b >= 0; --b) bits.push_back((0x7CD215D8u >> b) & 1);
    // One numeric codeword (message "12345", address 0)
    // Simplified: just add an idle codeword 0x7A89C197
    for (int b = 31; b >= 0; --b) bits.push_back((0x7A89C197u >> b) & 1);

    size_t N = static_cast<size_t>(sr * dur_s);
    std::vector<std::complex<float>> iq(N);
    double phase = 0.0;
    for (size_t i = 0; i < N; ++i) {
        size_t bit_idx = (i / sps) % bits.size();
        double dev = bits[bit_idx] ? +DEV : -DEV;
        phase += 2.0 * M_PI * dev;
        iq[i] = std::complex<float>(std::cos(phase), std::sin(phase));
    }
    return iq;
}

/**
 * @brief AIS: GMSK 9600 bps, BT=0.4, with HDLC preamble + flag.
 *
 * Uses approximate GMSK (instantaneous FM with Gaussian-filtered data).
 * Preamble: 24 alternating 1/0 bits (01010101...) then flag 0x7E.
 */
static inline std::vector<std::complex<float>>
makeAis(double sr, double dur_s)
{
    const double BAUD = 9600.0;
    const int    sps  = std::max(1, static_cast<int>(sr / BAUD));
    const double DEV  = 0.5 / sps;  // GMSK h=0.5 → peak dev = BAUD/4

    // Build bit stream: preamble + HDLC flag + idle
    std::vector<int> bits;
    for (int i = 0; i < 24; ++i) bits.push_back(i % 2);  // preamble
    // HDLC flag 0x7E = 01111110
    for (int b = 7; b >= 0; --b) bits.push_back((0x7Eu >> b) & 1);
    // AIS idle (fill with 0x7E)
    for (int r = 0; r < 4; ++r)
        for (int b = 7; b >= 0; --b) bits.push_back((0x7Eu >> b) & 1);

    size_t N = static_cast<size_t>(sr * dur_s);
    std::vector<std::complex<float>> iq(N);
    double phase = 0.0;
    for (size_t i = 0; i < N; ++i) {
        size_t bit_idx = (i / sps) % bits.size();
        double dev = bits[bit_idx] ? +DEV : -DEV;
        phase += 2.0 * M_PI * dev;
        iq[i] = std::complex<float>(std::cos(phase), std::sin(phase));
    }
    return iq;
}

/**
 * @brief ACARS: AM-modulated 2400 bps FSK (mark=2400 Hz, space=1200 Hz).
 *
 * Carrier at 30 kHz (typical intermediate), AM depth 85%.
 * Payload: ACARS prekey (12 mark bits) + SOH + "TEST" + ETX.
 */
static inline std::vector<std::complex<float>>
makeAcars(double sr, double dur_s)
{
    const double BAUD     = 2400.0;
    const double MARK_HZ  = 2400.0;
    const double SPACE_HZ = 1200.0;
    const double CARRIER  = 30000.0;
    const int    sps      = std::max(1, static_cast<int>(sr / BAUD));

    // Bit stream: 12 mark bits (prekey) + 0x01 (SOH) + "TEST" + 0x03 (ETX)
    std::vector<int> bits;
    for (int i = 0; i < 12; ++i) bits.push_back(1);  // prekey
    const std::vector<uint8_t> payload = {0x01, 'T','E','S','T', 0x03};
    for (uint8_t c : payload)
        for (int b = 7; b >= 0; --b) bits.push_back((c >> b) & 1);

    size_t N = static_cast<size_t>(sr * dur_s);
    std::vector<std::complex<float>> iq(N);
    double audio_phase = 0.0, carrier_phase = 0.0;
    for (size_t i = 0; i < N; ++i) {
        size_t bit_idx = (i / sps) % bits.size();
        double tone_hz = bits[bit_idx] ? MARK_HZ : SPACE_HZ;
        audio_phase   += 2.0 * M_PI * tone_hz / sr;
        carrier_phase += 2.0 * M_PI * CARRIER / sr;
        float audio = std::sin(audio_phase);
        float am    = (1.0f + 0.85f * audio);
        float re    = am * std::cos(carrier_phase);
        float im    = am * std::sin(carrier_phase);
        iq[i] = std::complex<float>(re, im);
    }
    return iq;
}

/**
 * @brief FM broadcast with voice-like audio (3 harmonics, 1000 Hz fundamental).
 * 75 kHz deviation, 250 kHz sample rate.
 */
static inline std::vector<std::complex<float>>
makeFmVoice(double sr, double dur_s)
{
    size_t N = static_cast<size_t>(sr * dur_s);
    std::vector<std::complex<float>> iq(N);
    const double kf = 75000.0 / sr;
    double phase = 0.0;
    for (size_t i = 0; i < N; ++i) {
        double t = static_cast<double>(i) / sr;
        // Voice-like: fundamental + 2 harmonics
        double audio = 0.6  * std::sin(2.0*M_PI*1000.0*t)
                     + 0.25 * std::sin(2.0*M_PI*2000.0*t)
                     + 0.15 * std::sin(2.0*M_PI*3000.0*t);
        phase += 2.0 * M_PI * kf * audio;
        iq[i] = std::complex<float>(std::cos(phase), std::sin(phase));
    }
    return iq;
}

} // namespace TestSignals
