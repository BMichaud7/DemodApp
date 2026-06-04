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
#include "demod/DtmfDemod.hpp"
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

// DTMF: 8 standard tones
static const double ROW[] = {697,770,852,941};
static const double COL[] = {1209,1336,1477,1633};
static const char DIGIT[4][4] = {
    {'1','2','3','A'},{'4','5','6','B'},
    {'7','8','9','C'},{'*','0','#','D'}
};

static double goertzel(const float* samples, int n, double freq, double sr) {
    double coeff = 2.0*std::cos(2.0*M_PI*freq/sr);
    double s1=0,s2=0;
    for(int i=0;i<n;++i) {
        double s = samples[i]+coeff*s1-s2;
        s2=s1; s1=s;
    }
    return s1*s1+s2*s2-coeff*s1*s2;
}

DemodResult DtmfDemod::process(const std::vector<std::complex<float>>& iq,
                                au::QuantityD<au::Hertz>   sr,
                                au::QuantityD<au::Hertz>   center_freq,
                                au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "DTMF";
    r.center_freq = center_freq;
    r.sample_rate = sr;
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0?au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    // Demodulate AM envelope → real audio
    std::vector<float> audio;
    audio.reserve(iq.size());
    for (const auto& s:iq) audio.push_back(std::abs(s));

    double sr_hz = sr.in(au::hertz);
    int block = std::max(64, static_cast<int>(sr_hz/50)); // 20 ms blocks
    std::string digits;
    char last=0;

    for (size_t i=0; i+block<=(size_t)audio.size(); i+=block/2) {
        const float* buf = audio.data()+i;
        double total=0;
        for(int k=0;k<block;++k) total+=buf[k]*buf[k];
        if (total/block < 1e-6) { last=0; continue; }

        // Find dominant row and column
        int br=-1,bc=-1; double mr=0,mc=0;
        for(int ri=0;ri<4;++ri){
            double e=goertzel(buf,block,ROW[ri],sr_hz);
            if(e>mr){mr=e;br=ri;}
        }
        for(int ci=0;ci<4;++ci){
            double e=goertzel(buf,block,COL[ci],sr_hz);
            if(e>mc){mc=e;bc=ci;}
        }
        if(br>=0&&bc>=0&&mr>total*0.1&&mc>total*0.1){
            char d=DIGIT[br][bc];
            if(d!=last){ digits+=d; last=d; }
        } else { last=0; }
    }

    if (!digits.empty()) {
        spdlog::info("DTMF: '{}'", digits);
        r.bits.assign(digits.begin(),digits.end());
    }
    return r;
}

} // namespace demod
