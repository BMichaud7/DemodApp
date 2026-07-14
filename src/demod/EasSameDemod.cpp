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
#include "demod/EasSameDemod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

// EAS/SAME: AFSK 520 baud, mark=2083 Hz, space=1563 Hz
// Preamble: 16×0xAB, then "ZCZC-" header
static constexpr double EAS_BAUD  = 520.833;
static constexpr double EAS_MARK  = 2083.3;
static constexpr double EAS_SPACE = 1562.5;

DemodResult EasSameDemod::process(const std::vector<std::complex<float>>& iq,
                                   au::QuantityD<au::Hertz>   sr,
                                   au::QuantityD<au::Hertz>   center_freq,
                                   au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "EAS_SAME";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(EAS_BAUD);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0?au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    double sr_hz = sr.in(au::hertz);
    // AM demodulate → audio
    std::vector<float> audio;
    audio.reserve(iq.size());
    ampmodem am = ampmodem_create(0.85f,LIQUID_AMPMODEM_DSB,0);
    for(const auto& s:iq){float d;ampmodem_demodulate(am,s,&d);audio.push_back(d);}
    ampmodem_destroy(am);

    // Discriminate mark vs space using Goertzel on each bit period
    int sps = std::max(1,static_cast<int>(sr_hz/EAS_BAUD));
    std::vector<uint8_t> bits;
    for(size_t i=0;i+sps<=(size_t)audio.size();i+=sps) {
        double em=0,es=0;
        const float* buf=audio.data()+i;
        double coef_m=2*std::cos(2*M_PI*EAS_MARK/sr_hz);
        double coef_s=2*std::cos(2*M_PI*EAS_SPACE/sr_hz);
        double s1m=0,s2m=0,s1s=0,s2s=0;
        for(int k=0;k<sps;++k){
            double sm=buf[k]+coef_m*s1m-s2m; s2m=s1m; s1m=sm;
            double ss=buf[k]+coef_s*s1s-s2s; s2s=s1s; s1s=ss;
        }
        em=s1m*s1m+s2m*s2m-coef_m*s1m*s2m;
        es=s1s*s1s+s2s*s2s-coef_s*s1s*s2s;
        bits.push_back(em>es?1:0);  // mark=1, space=0
    }

    // Find ZCZC header
    std::vector<uint8_t> bytes;
    for(size_t i=0;i+8<=(size_t)bits.size();i+=8){
        uint8_t byte=0;
        for(int b=0;b<8;++b) byte|=static_cast<uint8_t>(bits[i+b]<<b);
        bytes.push_back(byte);
    }
    std::string raw(bytes.begin(),bytes.end());
    auto pos=raw.find("ZCZC");
    if(pos!=std::string::npos){
        std::string header=raw.substr(pos,std::min((size_t)256,raw.size()-pos));
        spdlog::info("EAS/SAME: {}", header);
        r.bits.assign(header.begin(),header.end());

        // ── EAS spoofing heuristics ───────────────────────────────────────
        // ZCZC-ORG-EVT-PSSCCC+TTTT-JJJHHMM-LLLLLLLL-
        // Valid originators: PEP, EAS, CIV, WXR, NWS, EAN
        static const std::array<const char*,6> VALID_ORG = {"PEP","EAS","CIV","WXR","NWS","EAN"};
        // High-severity events that would be unusual without a real emergency
        static const std::array<const char*,6> RARE_EVENTS = {"EAN","NPT","EVI","CDW","VOW","EQW"};

        auto dash1 = header.find('-', 5);
        auto dash2 = (dash1!=std::string::npos) ? header.find('-', dash1+1) : std::string::npos;
        if(dash1!=std::string::npos && dash2!=std::string::npos){
            std::string org = header.substr(5, dash1-5);
            std::string evt = header.substr(dash1+1, dash2-dash1-1);
            bool valid_org = false;
            for(auto& o:VALID_ORG) if(org==o){valid_org=true;break;}
            if(!valid_org){
                r.alert_json="{\"type\":\"EAS_SPOOFING\",\"severity\":\"HIGH\","
                    "\"details\":\"Invalid EAS originator code '"+org+
                    "' in alert: "+header.substr(0,80)+"\"}";
                spdlog::warn("[ALERT] EAS spoofing: invalid originator '{}'", org);
            }
            // Flag rare/national-level events for manual review (only if no
            // higher-severity alert already set for this message).
            for(auto& e:RARE_EVENTS){
                if(evt==e && r.alert_json.empty()){
                    r.alert_json="{\"type\":\"EAS_SPOOFING\",\"severity\":\"MEDIUM\","
                        "\"details\":\"Rare EAS event code '"+evt+
                        "' — verify authenticity: "+header.substr(0,80)+"\"}";
                    spdlog::warn("[ALERT] EAS suspicious rare event '{}'", evt);
                    break;
                }
            }
        }
        return r;
    }
    r.bits=bytes;
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
