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
#include "demod/P25Phase2Demod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

// P25 Phase 2: π/4-DQPSK (H-DQPSK), 12000 sym/s, 2 TDMA timeslots
// Carrier BW: 12.5 kHz. Returns raw dibits (AMBE+2 voice decode future)
static constexpr double P2_SYM_RATE = 12000.0;

DemodResult P25Phase2Demod::process(const std::vector<std::complex<float>>& iq,
                                     au::QuantityD<au::Hertz>   sr,
                                     au::QuantityD<au::Hertz>   center_freq,
                                     au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "P25_PHASE2";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(P2_SYM_RATE);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0?au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    float sps=static_cast<float>(sr.in(au::hertz)/P2_SYM_RATE);
    // π/4-DQPSK: differential QPSK with π/4 rotation per symbol
    modemcf mod=modemcf_create(LIQUID_MODEM_DPSK4);

    // Downsample to ~1 sample/symbol (symsync_cccf not available in liquid-dsp 1.4)
    std::vector<uint8_t> bits;
    unsigned step=std::max(1u,static_cast<unsigned>(std::round(sps)));
    for(size_t i=0;i<iq.size();i+=step){
        unsigned sym_idx=0;
        modemcf_demodulate(mod,iq[i],&sym_idx);
        bits.push_back((sym_idx>>1)&1);
        bits.push_back( sym_idx    &1);
    }
    modemcf_destroy(mod);

    spdlog::debug("P25Ph2: {} symbols",bits.size()/2);
    r.bits.reserve((bits.size()+7)/8);
    for(size_t i=0;i+8<=(size_t)bits.size();i+=8){
        uint8_t byte=0;for(int b=0;b<8;++b)byte|=static_cast<uint8_t>(bits[i+b]<<(7-b));r.bits.push_back(byte);}
    return r;
}

} // namespace demod
