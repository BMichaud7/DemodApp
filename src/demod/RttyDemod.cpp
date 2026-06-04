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
#include "demod/RttyDemod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

// RTTY: 2-FSK, 45.45/50/75/110 baud, ±85 Hz (narrow) or ±170/±425 Hz deviation
// Baudot ITA-2 character set
static const char LETTER[32]={'\0','E','\n','A',' ','S','I','U','\n','D','R','J','N','F','C','K',
                                'T','Z','L','W','H','Y','P','Q','O','B','G','\0','M','X','V','\0'};
static const char FIGURE[32]={'\0','3','\n','-',' ','\'','8','7','\n','\0','4','\0',',','!',':','(',
                                '5','"',')','2','#','6','0','1','9','?','&','\0','.','/',';','\0'};
static constexpr double RTTY_BAUD = 45.45;
static constexpr double RTTY_DEV  = 85.0;

DemodResult RttyDemod::process(const std::vector<std::complex<float>>& iq,
                                au::QuantityD<au::Hertz>   sr,
                                au::QuantityD<au::Hertz>   center_freq,
                                au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "RTTY";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(RTTY_BAUD);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0?au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    double sr_hz=sr.in(au::hertz);
    float sps=static_cast<float>(sr_hz/RTTY_BAUD);
    freqdem fdem=freqdem_create(static_cast<float>(RTTY_DEV/RTTY_BAUD));
    symsync_rrrf sync=symsync_rrrf_create_rnyquist(
        LIQUID_FIRFILT_RRC,static_cast<unsigned>(std::round(sps)),5,0.3f,32);
    symsync_rrrf_set_lf_bw(sync,0.005f);

    std::vector<uint8_t> bits;
    float buf[4]; unsigned n;
    for(const auto& s:iq){float fd;freqdem_demodulate(fdem,s,&fd);
        symsync_rrrf_execute(sync,&fd,1,buf,&n);
        for(unsigned i=0;i<n;++i) bits.push_back(buf[i]>0.0f?1:0);}
    freqdem_destroy(fdem);symsync_rrrf_destroy(sync);

    // Decode Baudot: start bit(0)+5 data bits+stop bit(1) = 7.5 bit frames
    bool figure_mode=false;
    std::string text;
    for(size_t i=0;i+7<=(size_t)bits.size();++i){
        if(bits[i]==0){  // start bit
            uint8_t code=0;
            for(int b=0;b<5;++b) code|=static_cast<uint8_t>(bits[i+1+b]<<b);
            if(bits[i+6]==1){  // stop bit
                if(code==0x1F) figure_mode=true;
                else if(code==0x1B) figure_mode=false;
                else {
                    char c=figure_mode?FIGURE[code]:LETTER[code];
                    if(c&&c!='\0') text+=c;
                }
                i+=6;
            }
        }
    }
    if(!text.empty()){spdlog::info("RTTY: '{}'",text);r.bits.assign(text.begin(),text.end());return r;}
    r.bits.reserve((bits.size()+7)/8);
    for(size_t i=0;i+8<=(size_t)bits.size();i+=8){
        uint8_t byte=0;for(int b=0;b<8;++b)byte|=static_cast<uint8_t>(bits[i+b]<<(7-b));r.bits.push_back(byte);}
    return r;
}

} // namespace demod
