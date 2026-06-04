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
#include "demod/Vdl2Demod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

// VDL Mode 2: D8PSK (Differential 8-PSK), 10500 sym/s = 31.5 kbps
// Aviation VHF 136.900 MHz primary, others 136-137 MHz
static constexpr double VDL2_SYM_RATE = 10500.0;

DemodResult Vdl2Demod::process(const std::vector<std::complex<float>>& iq,
                                au::QuantityD<au::Hertz>   sr,
                                au::QuantityD<au::Hertz>   center_freq,
                                au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "VDL2";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(VDL2_SYM_RATE);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0?au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    float sps=static_cast<float>(sr.in(au::hertz)/VDL2_SYM_RATE);
    modemcf mod=modemcf_create(LIQUID_MODEM_DPSK8);

    // Downsample to ~1 sample/symbol before demodulation
    std::vector<uint8_t> bits;
    unsigned step = std::max(1u, static_cast<unsigned>(std::round(sps)));
    for(size_t i=0; i<iq.size(); i+=step){
        unsigned sym=0; modemcf_demodulate(mod,iq[i],&sym);
        bits.push_back((sym>>2)&1);
        bits.push_back((sym>>1)&1);
        bits.push_back( sym    &1);
    }
    modemcf_destroy(mod);

    // VDL2 uses AVLC framing (similar to HDLC): 0x7E flags
    std::vector<uint8_t> bytes;
    for(size_t i=0;i+8<=(size_t)bits.size();i+=8){
        uint8_t byte=0;for(int b=0;b<8;++b) byte|=static_cast<uint8_t>(bits[i+b]<<b);
        bytes.push_back(byte);
    }
    // Find AVLC frame (starts/ends with 0x7E)
    std::string msg;
    for(size_t i=0;i+2<(size_t)bytes.size();++i){
        if(bytes[i]==0x7E&&bytes[i+1]==0x7E) continue;
        if(bytes[i]==0x7E){
            for(size_t j=i+1;j<bytes.size()&&bytes[j]!=0x7E;++j)
                if(bytes[j]>=0x20&&bytes[j]<0x7F) msg+=bytes[j];
            if(!msg.empty()){spdlog::info("VDL2: {}",msg.substr(0,80));break;}
        }
    }
    if(!msg.empty()) r.bits.assign(msg.begin(),msg.end());
    else r.bits=bytes;
    return r;
}

} // namespace demod
