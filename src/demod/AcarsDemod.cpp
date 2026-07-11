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
#include "demod/AcarsDemod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <string>
#include <spdlog/spdlog.h>

namespace demod {

// ACARS: AM envelope → 2400 bps FSK (2400 Hz mark, 1200 Hz space)
static constexpr double ACARS_BAUD  = 2400.0;
static constexpr double ACARS_MARK  = 2400.0;  // Hz
static constexpr double ACARS_SPACE = 1200.0;  // Hz

DemodResult AcarsDemod::process(const std::vector<std::complex<float>>& iq,
                                 au::QuantityD<au::Hertz>   sr,
                                 au::QuantityD<au::Hertz>   center_freq,
                                 au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "ACARS";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(ACARS_BAUD);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0 ? au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    double sr_hz = sr.in(au::hertz);

    // Step 1: AM envelope detection → baseband audio
    std::vector<float> audio;
    audio.reserve(iq.size());
    ampmodem am = ampmodem_create(0.85f, LIQUID_AMPMODEM_DSB, 0);
    for (const auto& s : iq) {
        float demod_out;
        ampmodem_demodulate(am, s, &demod_out);
        audio.push_back(demod_out);
    }
    ampmodem_destroy(am);

    // Step 2: correlate with mark (2400 Hz) and space (1200 Hz) tones
    float sps = static_cast<float>(sr_hz / ACARS_BAUD);
    unsigned isps = static_cast<unsigned>(std::round(sps));
    if (isps < 2) { return r; }

    std::vector<uint8_t> bits;
    for (size_t i = 0; i + isps <= audio.size(); i += isps) {
        // Goertzel-style: energy at mark vs space frequency.
        // Accumulate both I and Q components so the energy estimate is
        // phase-independent. Using only cos would null out signals that
        // happen to be in quadrature with the reference, causing ~50% errors.
        double mi=0, mq=0, si=0, sq=0;
        for (unsigned k = 0; k < isps; ++k) {
            double t = static_cast<double>(i + k) / sr_hz;
            double s  = audio[i + k];
            double wm = 2*M_PI*ACARS_MARK *t;
            double ws = 2*M_PI*ACARS_SPACE*t;
            mi += s * std::cos(wm);  mq += s * std::sin(wm);
            si += s * std::cos(ws);  sq += s * std::sin(ws);
        }
        double e_mark  = mi*mi + mq*mq;
        double e_space = si*si + sq*sq;
        bits.push_back(e_mark > e_space ? 1 : 0);
    }

    // Step 3: scan for ACARS preamble (0x2B2B2B2B = "++++" = prekey)
    // and extract message text (SOH ... ETX)
    bool in_msg = false;
    std::string text;
    uint8_t byte = 0; int nbits = 0;
    std::vector<uint8_t> decoded_bytes;
    for (uint8_t b : bits) {
        byte |= static_cast<uint8_t>(b << nbits++);
        if (nbits == 8) {
            decoded_bytes.push_back(byte);
            byte = 0; nbits = 0;
        }
    }
    for (uint8_t c : decoded_bytes) {
        if (c == 0x01) { in_msg = true; continue; }  // SOH
        if (c == 0x03 || c == 0x17) { in_msg = false; break; }  // ETX/ETB
        if (in_msg && c >= 0x20 && c < 0x7F) text += static_cast<char>(c);
    }

    if (!text.empty()) {
        spdlog::info("ACARS message: '{}'", text);
        r.bits.assign(text.begin(), text.end());
    } else {
        r.bits = decoded_bytes;
    }
    spdlog::debug("AcarsDemod: {} bytes decoded", r.bits.size());
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
