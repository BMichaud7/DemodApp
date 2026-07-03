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
#include "demod/PocsagDemod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <cstring>
#include <spdlog/spdlog.h>

namespace demod {

// POCSAG: 2-FSK, ±4500 Hz, auto-detect 512/1200/2400 bps
static constexpr double POCSAG_DEV = 4500.0;
// POCSAG sync codeword: 0x7CD215D8
static constexpr uint32_t POCSAG_SYNC = 0x7CD215D8u;

// POCSAG alphanumeric decode (7-bit ASCII, 20 chars per batch)
static std::string decode_alpha(const uint32_t* codewords, int n) {
    std::string out;
    int bit_buf = 0, nbits = 0;
    for (int i = 0; i < n; ++i) {
        uint32_t cw = codewords[i] >> 1;  // strip message type bit
        for (int b = 19; b >= 0; --b) {
            bit_buf |= ((cw >> b) & 1) << nbits++;
            if (nbits == 7) {
                char c = static_cast<char>(bit_buf & 0x7F);
                if (c >= 0x20 && c < 0x7F) out += c;
                bit_buf = 0; nbits = 0;
            }
        }
    }
    return out;
}

DemodResult PocsagDemod::process(const std::vector<std::complex<float>>& iq,
                                  au::QuantityD<au::Hertz>   sr,
                                  au::QuantityD<au::Hertz>   center_freq,
                                  au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "POCSAG";
    r.center_freq = center_freq;
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0 ? au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    double sr_hz  = sr.in(au::hertz);
    // Try 1200 bps as most common POCSAG rate
    double baud   = 1200.0;
    float  sps    = static_cast<float>(sr_hz / baud);
    // kf = deviation / sample_rate (NOT deviation / baud — that gives kf>>0.5,
    // which inverts the discriminator scale and breaks timing recovery).
    float  mi     = std::clamp(static_cast<float>(POCSAG_DEV / sr_hz), 0.01f, 0.49f);

    freqdem fdem  = freqdem_create(mi);
    symsync_rrrf sync = symsync_rrrf_create_rnyquist(
        LIQUID_FIRFILT_RRC, static_cast<unsigned>(std::round(sps)), 5, 0.2f, 32);
    symsync_rrrf_set_lf_bw(sync, 0.005f);

    std::vector<uint8_t> bits;
    float buf[4]; unsigned n;
    for (const auto& s:iq) {
        float fd; freqdem_demodulate(fdem,s,&fd);
        symsync_rrrf_execute(sync,&fd,1,buf,&n);
        for (unsigned i=0;i<n;++i) bits.push_back(buf[i]>0.0f?1:0);
    }
    freqdem_destroy(fdem); symsync_rrrf_destroy(sync);
    r.sample_rate = au::hertz(baud);

    // Search for sync codeword (0x7CD215D8)
    if (bits.size() < 32) { return r; }
    uint32_t shift = 0;
    std::vector<uint32_t> messages;
    int sync_count = 0;
    for (size_t i = 0; i < bits.size(); ++i) {
        shift = (shift << 1) | bits[i];
        if (shift == POCSAG_SYNC) {
            ++sync_count;
            // Read up to 16 codewords following sync
            size_t start = i + 1;
            for (int cw_i = 0; cw_i < 16 && start + 32 <= bits.size(); ++cw_i, start += 32) {
                uint32_t cw = 0;
                for (int b = 0; b < 32; ++b) cw = (cw << 1) | bits[start + b];
                if (cw == 0x7A89C197u) break;  // idle codeword
                if ((cw >> 31) & 1) messages.push_back(cw);  // message codeword
            }
        }
    }

    if (!messages.empty()) {
        std::string text = decode_alpha(messages.data(), (int)messages.size());
        if (!text.empty()) {
            spdlog::info("POCSAG message: '{}'", text);
            r.bits.assign(text.begin(), text.end());
            return r;
        }
    }

    // Fall back to raw bits
    r.bits.reserve((bits.size()+7)/8);
    for (size_t i=0;i+8<=bits.size();i+=8) {
        uint8_t byte=0;
        for(int b=0;b<8;++b) byte|=static_cast<uint8_t>(bits[i+b]<<(7-b));
        r.bits.push_back(byte);
    }
    spdlog::debug("PocsagDemod: {} syncs, {} bytes", sync_count, r.bits.size());
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
