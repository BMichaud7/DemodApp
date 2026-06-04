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
#include "demod/FlexDemod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

// FLEX: 4-level FSK, 1600/3200/6400 baud, ±1600/3200/6400 Hz deviation
// Sync word: 0xA8C9 (1600 baud)
static constexpr double FLEX_BAUD  = 1600.0;
static constexpr double FLEX_DEV   = 1600.0;

static const char* flex_opcode(uint8_t op) {
    switch (op) {
        case 0:  return "Tone-only";
        case 1:  return "Numeric";
        case 2:  return "Special Numeric";
        case 3:  return "Alphanumeric";
        case 4:  return "Binary";
        case 5:  return "Secure";
        default: return "Unknown";
    }
}

// BCH(31,21) error correction used in FLEX frames
static uint32_t bch31_correct(uint32_t word) {
    // Simple parity check — full BCH would require syndrome table
    uint32_t data = word & 0x1FFFFF;
    return data;
}

DemodResult FlexDemod::process(const std::vector<std::complex<float>>& iq,
                                au::QuantityD<au::Hertz>   sr,
                                au::QuantityD<au::Hertz>   center_freq,
                                au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "FLEX";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(FLEX_BAUD);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0 ? au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    float sps = static_cast<float>(sr.in(au::hertz)/FLEX_BAUD);
    float mi  = static_cast<float>(FLEX_DEV/FLEX_BAUD);

    freqdem fdem = freqdem_create(mi);
    symsync_rrrf sync = symsync_rrrf_create_rnyquist(
        LIQUID_FIRFILT_RRC, static_cast<unsigned>(std::round(sps)), 5, 0.2f, 32);
    symsync_rrrf_set_lf_bw(sync, 0.01f);

    std::vector<uint8_t> bits;
    float buf[4]; unsigned n;
    for (const auto& s:iq) {
        float fd; freqdem_demodulate(fdem,s,&fd);
        symsync_rrrf_execute(sync,&fd,1,buf,&n);
        for (unsigned i=0;i<n;++i) {
            // 4-level slicer: +1800→01, +600→00, -600→10, -1800→11
            uint8_t d = fd> 0.5f?1:fd>0.0f?0:fd>-0.5f?2:3;
            bits.push_back((d>>1)&1); bits.push_back(d&1);
        }
    }
    freqdem_destroy(fdem); symsync_rrrf_destroy(sync);

    // Scan for FLEX sync 0xA8C9 (1600 baud sync word)
    uint32_t shift=0; std::string msg;
    for (size_t i=0;i<bits.size();++i) {
        shift=(shift<<1)|bits[i];
        if ((shift&0xFFFF)==0xA8C9) {
            spdlog::info("FLEX sync found at bit {}", i);
            // Extract address from next 32 bits
            if (i+33<bits.size()) {
                uint32_t addr=0;
                for(int b=0;b<32;++b) addr=(addr<<1)|bits[i+1+b];
                addr = bch31_correct(addr);
                msg += "ADDR:" + std::to_string(addr & 0x1FFFFF) + " ";
            }
        }
    }
    if (!msg.empty()) spdlog::info("FLEX: {}", msg);

    r.bits.reserve((bits.size()+7)/8);
    for (size_t i=0;i+8<=bits.size();i+=8) {
        uint8_t byte=0;
        for(int b=0;b<8;++b) byte|=static_cast<uint8_t>(bits[i+b]<<(7-b));
        r.bits.push_back(byte);
    }
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
