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
    int total_candidates=0, crc_failures=0;
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
        ++total_candidates;

        // Pack to bytes
        uint8_t frame[14]={};
        for(int b=0;b<std::min((int)bits.size(),112);++b)
            frame[b/8]|=static_cast<uint8_t>(bits[b]<<(7-b%8));

        // CRC check
        int frame_len=(bits.size()>=112)?14:7;
        uint32_t crc=crc24(frame,frame_len-3);
        uint32_t fcrc=(static_cast<uint32_t>(frame[frame_len-3])<<16)|
                      (static_cast<uint32_t>(frame[frame_len-2])<<8)|frame[frame_len-1];
        if(crc!=fcrc){ ++crc_failures; continue; }

        std::string icao=icao_from_frame(frame);
        uint8_t df=(frame[0]>>3)&0x1F;

        // ── Spoofing heuristics ───────────────────────────────────────────
        // DF17/18 extended squitter carries position/velocity in ME field
        if(df==17||df==18){
            uint8_t me_type=(frame[4]>>3)&0x1F;
            // Airborne velocity (type 19): check groundspeed
            if(me_type==19){
                int ew_raw=((frame[6]&0x03)<<8)|frame[7];
                int ns_raw=((frame[8]&0x7F)<<3)|(frame[9]>>5);
                double ew_kt=(ew_raw>0)?(ew_raw-1):0;
                double ns_kt=(ns_raw>0)?(ns_raw-1):0;
                double gs=std::hypot(ew_kt,ns_kt);
                if(gs>700.0){
                    r.alert_json="{\"type\":\"ADSB_SPOOFING\",\"severity\":\"HIGH\","
                        "\"details\":\"ICAO "+icao+" impossible groundspeed "+
                        std::to_string((int)gs)+" knots (max realistic ~600 kt)\"}";
                    spdlog::warn("[ALERT] ADS-B spoofing: ICAO={} GS={:.0f} kt", icao, gs);
                }
            }
            // Airborne position (types 9-18): check altitude sanity
            if(me_type>=9&&me_type<=18){
                int alt_raw=((frame[5]&0xFF)<<4)|((frame[6]>>4)&0x0F);
                // Gillham code decode (simplified): alt in 25ft increments
                int alt_ft=(alt_raw*25)-1000;
                if(alt_ft>60000||alt_ft<-1500){
                    r.alert_json="{\"type\":\"ADSB_SPOOFING\",\"severity\":\"HIGH\","
                        "\"details\":\"ICAO "+icao+" impossible altitude "+
                        std::to_string(alt_ft)+" ft\"}";
                    spdlog::warn("[ALERT] ADS-B spoofing: ICAO={} alt={} ft", icao, alt_ft);
                }
            }
        }
        spdlog::info("ADS-B: DF={} ICAO={}", df, icao);
        messages += "ICAO:" + icao + " DF:" + std::to_string(df) + " ";
        i+=112*us;
    }

    // High CRC failure rate → jamming
    if(total_candidates>10 && crc_failures*100/total_candidates>80){
        r.alert_json="{\"type\":\"ADSB_JAMMING\",\"severity\":\"HIGH\","
            "\"details\":\"1090 MHz CRC failure rate "+
            std::to_string(crc_failures*100/total_candidates)+
            "% ("+std::to_string(crc_failures)+"/"+std::to_string(total_candidates)+
            " candidates) — possible jammer\"}";
        spdlog::warn("[ALERT] ADS-B jamming: CRC fail rate {}%", crc_failures*100/total_candidates);
    }

    if(!messages.empty()) r.bits.assign(messages.begin(),messages.end());
    return r;
}

} // namespace demod
