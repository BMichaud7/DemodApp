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
#include "demod/NavtexDemod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

// NAVTEX: FSK 100 baud, ±150 Hz, SITOR-B (FEC), ITA-3 character set
// Primary 518 kHz, secondary 490 kHz, optional 4209.5 kHz
static constexpr double NAVTEX_BAUD = 100.0;
static constexpr double NAVTEX_DEV  = 150.0;

// ITA-3 / CCIR 476 character set (7-bit, 4-of-7 code)
static const char ITA3_LETTER[128]={0};  // simplified — use raw bits

DemodResult NavtexDemod::process(const std::vector<std::complex<float>>& iq,
                                  au::QuantityD<au::Hertz>   sr,
                                  au::QuantityD<au::Hertz>   center_freq,
                                  au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "NAVTEX";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(NAVTEX_BAUD);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0?au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    float sps=static_cast<float>(sr.in(au::hertz)/NAVTEX_BAUD);
    freqdem fdem=freqdem_create(static_cast<float>(NAVTEX_DEV/NAVTEX_BAUD));
    symsync_rrrf sync=symsync_rrrf_create_rnyquist(
        LIQUID_FIRFILT_RRC,static_cast<unsigned>(std::round(sps)),5,0.5f,32);
    symsync_rrrf_set_lf_bw(sync,0.005f);

    std::vector<uint8_t> bits;
    float buf[4]; unsigned n;
    for(const auto& s:iq){float fd;freqdem_demodulate(fdem,s,&fd);
        symsync_rrrf_execute(sync,&fd,1,buf,&n);
        for(unsigned i=0;i<n;++i) bits.push_back(buf[i]>0.0f?1:0);}
    freqdem_destroy(fdem);symsync_rrrf_destroy(sync);

    // SITOR-B: 7-bit code with 4-of-7 constraint (error detection)
    // Extract bytes and scan for ZCZC header (NAVTEX start)
    std::vector<uint8_t> bytes;
    for(size_t i=0;i+7<=(size_t)bits.size();i+=7){
        uint8_t byte=0;
        for(int b=0;b<7;++b) byte|=static_cast<uint8_t>(bits[i+b]<<b);
        // Map ITU-T ITA-3 to ASCII (simplified direct mapping)
        if(byte>=32&&byte<127) bytes.push_back(byte);
        else if(byte==0x0D) bytes.push_back('\n');
    }

    std::string raw(bytes.begin(),bytes.end());
    auto pos=raw.find("ZCZC");
    if(pos!=std::string::npos){
        std::string msg=raw.substr(pos,std::min((size_t)512,raw.size()-pos));
        spdlog::info("NAVTEX: {}",msg.substr(0,80));
        r.bits.assign(msg.begin(),msg.end());
        return r;
    }
    r.bits=bytes;
    return r;
}

} // namespace demod
