#include "demod/AdsbDemod.hpp"
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

// ADS-B Mode S: Pulse-Position Modulation at 1090 MHz
// Bit period: 1 μs (1 Mbps). Preamble: 8 μs (4 pulses).
// Short squitter: 56 bits. Long squitter (extended): 112 bits.
// Works with any SDR that reaches 1090 MHz (HackRF, USRP, etc.)

static uint32_t crc24(const uint8_t* data, int len) {
    static const uint32_t POLY = 0xFFF409;
    uint32_t crc = 0;
    for (int i=0;i<len;++i){
        crc ^= static_cast<uint32_t>(data[i])<<16;
        for(int b=0;b<8;++b) crc=(crc&0x800000)?(crc<<1)^POLY:(crc<<1);
    }
    return crc&0xFFFFFF;
}

static std::string icao_from_frame(const uint8_t* f){
    char buf[8]; snprintf(buf,8,"%02X%02X%02X",f[1],f[2],f[3]); return buf;
}

DemodResult AdsbDemod::process(const std::vector<std::complex<float>>& iq,
                                au::QuantityD<au::Hertz>   sr,
                                au::QuantityD<au::Hertz>   center_freq,
                                au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "ADS_B";
    r.center_freq = center_freq;
    r.sample_rate = sr;
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0?au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    double sr_hz=sr.in(au::hertz);
    int us = static_cast<int>(sr_hz/1e6);  // samples per microsecond
    if(us<1) us=1;

    // Magnitude signal
    std::vector<float> mag;
    mag.reserve(iq.size());
    for(const auto& s:iq) mag.push_back(std::abs(s));

    float noise=0; for(size_t i=0;i<std::min((size_t)100,mag.size());++i) noise+=mag[i]; noise/=100;
    float thresh=noise*3.5f;

    std::string messages;
    for(size_t i=0;i+240*us<(size_t)mag.size();++i){
        // Look for Mode S preamble: pulses at 0,1,3.5,4.5 μs
        bool p0=mag[i]>thresh, p1=mag[i+us]>thresh;
        bool p2=mag[i+3*us+us/2]>thresh, p3=mag[i+4*us+us/2]>thresh;
        if(!(p0&&p1&&p2&&p3)) continue;

        // Decode bits using PPM: compare first vs second half of each 2μs slot
        std::vector<uint8_t> bits;
        for(int b=0;b<112;++b){
            size_t off=i+(8+b*2)*us;
            if(off+2*us>(size_t)mag.size()) break;
            float a=mag[off], b2=mag[off+us];
            bits.push_back(a>b2?1:0);
        }
        if(bits.size()<56) continue;

        // Pack to bytes
        uint8_t frame[14]={};
        for(int b=0;b<std::min((int)bits.size(),112);++b)
            frame[b/8]|=static_cast<uint8_t>(bits[b]<<(7-b%8));

        // CRC check
        int frame_len=(bits.size()>=112)?14:7;
        uint32_t crc=crc24(frame,frame_len-3);
        uint32_t fcrc=(static_cast<uint32_t>(frame[frame_len-3])<<16)|
                      (static_cast<uint32_t>(frame[frame_len-2])<<8)|frame[frame_len-1];
        if(crc!=fcrc) continue;

        std::string icao=icao_from_frame(frame);
        uint8_t df=(frame[0]>>3)&0x1F;
        spdlog::info("ADS-B: DF={} ICAO={}", df, icao);
        messages += "ICAO:" + icao + " DF:" + std::to_string(df) + " ";
        i+=112*us;
    }
    if(!messages.empty()) r.bits.assign(messages.begin(),messages.end());
    return r;
}

} // namespace demod
