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
#include "demod/Psk31Demod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

// PSK31: BPSK, 31.25 baud (31 Hz bandwidth!), Varicode encoding
// PSK63: 62.5 baud variant
static constexpr double PSK31_BAUD = 31.25;

// Varicode table: variable-length Huffman-like code for each ASCII char
// Each character ends with 00; space = 1, E = 11, etc.
static const uint32_t VARICODE[128] = {
    0b1010101011,0b1011011011,0b1011101101,0b1101110111, // 0-3
    0b1011101011,0b1101011111,0b1011101111,0b1011111101, // 4-7
    0b1011111111,0b11100111,  0b11101,     0b1101101111, // 8-11 (\b\t\n\v)
    0b1011011101,0b11111,     0b1101110101,0b1110101011, // 12-15
    0b1011110111,0b1011110101,0b1110101101,0b1110101111, // 16-19
    0b1101011011,0b1101101011,0b1101101101,0b1101010111, // 20-23
    0b1101111011,0b1101111101,0b1110110111,0b1101010101, // 24-27
    0b1101011101,0b1110111011,0b1011111011,0b1101111111, // 28-31
    0b1,         0b111111111, 0b101011111, 0b111110101,  // 32-35  (' '!"#)
    0b111011011, 0b1011010101,0b1010111011,0b101111111,  // 36-39
    0b11111011,  0b11110111,  0b101101111, 0b111011111,  // 40-43
    0b1110101,   0b110101,    0b1010111,   0b110101111,  // 44-47
    0b10110111,  0b10111101,  0b11101101,  0b11111111,   // 48-51 (0-3)
    0b101110111, 0b101011011, 0b101101011, 0b110101101,  // 52-55 (4-7)
    0b110101011, 0b110110111, 0b11110101,  0b110111101,  // 56-59 (8-9:;)
    0b111101101, 0b1110111,   0b111101111, 0b111010111,  // 60-63
    0b1010110111,0b1110011,   0b1011110,   0b1011011,    // 64-67 (@A-C)
    0b11001,     0b1110,      0b11011,     0b111001,     // 68-71 (D-G)
    0b101111,    0b1111,      0b111100,    0b101110,     // 72-75 (H-K)
    0b110010,    0b1110,      0b110,       0b101010,     // 76-79 (L-O)
    0b111010,    0b10111011,  0b101100,    0b11100,      // 80-83 (P-S)
    0b10110,     0b110111,    0b1111010,   0b101101,     // 84-87 (T-W)
    0b1011011,   0b1110010,   0b1011101,   0b1110100,    // 88-91 (X-[)
    0b10111011,  0b1110000,   0b101111011, 0b110010011,  // 92-95
};

DemodResult Psk31Demod::process(const std::vector<std::complex<float>>& iq,
                                 au::QuantityD<au::Hertz>   sr,
                                 au::QuantityD<au::Hertz>   center_freq,
                                 au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "PSK31";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(PSK31_BAUD);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0?au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    float sps=static_cast<float>(sr.in(au::hertz)/PSK31_BAUD);
    modemcf mod=modemcf_create(LIQUID_MODEM_BPSK);

    // Downsample to ~1 sample/symbol; PSK31 baud is slow so this is sufficient
    std::vector<uint8_t> bits;
    unsigned step = std::max(1u, static_cast<unsigned>(std::round(sps)));
    unsigned prev=0;
    for(size_t i=0; i<iq.size(); i+=step){
        unsigned sym=0; modemcf_demodulate(mod,iq[i],&sym);
        bits.push_back((sym^prev)&1);
        prev=sym;
    }
    modemcf_destroy(mod);

    // Varicode decode: two consecutive 0s = character boundary
    std::string text;
    uint32_t accum=0; int nbits=0; int zeros=0;
    for(uint8_t b:bits){
        if(b==0){ ++zeros; if(zeros>=2&&nbits>0){
            // Try to decode Varicode character
            for(int c=32;c<96;++c){
                if(accum==VARICODE[c]){text+=static_cast<char>(c);break;}
            }
            accum=0;nbits=0;zeros=0;
        } else { accum=(accum<<1)|0; ++nbits; }
        } else { zeros=0; accum=(accum<<1)|1; ++nbits; }
    }

    if(!text.empty()){
        spdlog::info("PSK31: '{}'",text.substr(0,80));
        r.bits.assign(text.begin(),text.end());
    } else {
        r.bits.reserve((bits.size()+7)/8);
        for(size_t i=0;i+8<=(size_t)bits.size();i+=8){
            uint8_t byte=0;for(int b=0;b<8;++b)byte|=static_cast<uint8_t>(bits[i+b]<<(7-b));r.bits.push_back(byte);}
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
