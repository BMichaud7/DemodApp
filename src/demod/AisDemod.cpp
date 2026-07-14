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
#include "demod/AisDemod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <string>
#include <spdlog/spdlog.h>

namespace demod {

// AIS: GMSK 9600 bps, BT=0.4, NRZI encoded, HDLC framed
static constexpr double AIS_BAUD = 9600.0;

// NRZI decode: bit=1 means no transition, bit=0 means transition
static std::vector<uint8_t> nrzi_decode(const std::vector<uint8_t>& bits) {
    std::vector<uint8_t> out; out.reserve(bits.size());
    uint8_t prev = 0;
    for (uint8_t b : bits) {
        out.push_back(b == prev ? 1 : 0);
        prev = b;
    }
    return out;
}

// HDLC flag = 0x7E = 01111110; unstuff after 5 consecutive 1s
static std::vector<uint8_t> hdlc_unstuff(const std::vector<uint8_t>& bits) {
    std::vector<uint8_t> out;
    int ones = 0;
    for (uint8_t b : bits) {
        if (b == 1) { ++ones; out.push_back(1); }
        else if (ones == 5) { ones = 0; /* stuffed 0, discard */ }
        else { ones = 0; out.push_back(0); }
    }
    return out;
}

DemodResult AisDemod::process(const std::vector<std::complex<float>>& iq,
                               au::QuantityD<au::Hertz>   sr,
                               au::QuantityD<au::Hertz>   center_freq,
                               au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "AIS";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(AIS_BAUD);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0 ? au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    float sps = static_cast<float>(sr.in(au::hertz)/AIS_BAUD);
    freqdem fdem = freqdem_create(0.25f);  // GMSK mod index ~0.25
    symsync_rrrf sync = symsync_rrrf_create_rnyquist(
        LIQUID_FIRFILT_GMSKRX, static_cast<unsigned>(std::round(sps)), 5, 0.4f, 32);
    symsync_rrrf_set_lf_bw(sync, 0.01f);

    std::vector<uint8_t> raw_bits;
    float buf[4]; unsigned n;
    for (const auto& s:iq) {
        float fd; freqdem_demodulate(fdem,s,&fd);
        symsync_rrrf_execute(sync,&fd,1,buf,&n);
        for (unsigned i=0;i<n;++i) raw_bits.push_back(buf[i]>0.0f?1:0);
    }
    freqdem_destroy(fdem); symsync_rrrf_destroy(sync);

    // NRZI + HDLC unstuff
    auto decoded = hdlc_unstuff(nrzi_decode(raw_bits));

    // Pack to bytes
    r.bits.reserve((decoded.size()+7)/8);
    for (size_t i=0;i+8<=decoded.size();i+=8) {
        uint8_t byte=0;
        for(int b=0;b<8;++b) byte|=static_cast<uint8_t>(decoded[i+b]<<b); // LSB first per AIS
        r.bits.push_back(byte);
    }
    spdlog::debug("AisDemod: {} raw bits → {} bytes after NRZI/HDLC",
                  raw_bits.size(), r.bits.size());

    // ── AIS spoofing heuristics ───────────────────────────────────────────
    // AIS message type 1/2/3 (position report): minimum 28 bytes
    if (r.bits.size() >= 28) {
        const auto& b = r.bits;
        int msg_type = (b[0] >> 2) & 0x3F;
        if (msg_type >= 1 && msg_type <= 3) {
            // MMSI: bits 8-37 (30 bits)
            uint32_t mmsi = 0;
            for (int i=8;i<38;++i) mmsi = (mmsi<<1)|((decoded.size()>i)?(decoded[i]&1):0);
            // SOG (speed over ground): bits 50-59, 1/10 knot units
            uint32_t sog_raw = 0;
            for (int i=50;i<60&&i<(int)decoded.size();++i)
                sog_raw = (sog_raw<<1)|(decoded[i]&1);
            double sog_kt = sog_raw / 10.0;

            // Vessels >102.2 knots are flagged as "not available" in the spec.
            // Anything above 50 knots for a surface vessel is deeply suspicious.
            if (sog_kt > 50.0 && sog_kt < 102.2) {
                r.alert_json = "{\"type\":\"AIS_SPOOFING\",\"severity\":\"HIGH\","
                    "\"details\":\"MMSI " + std::to_string(mmsi) +
                    " impossible SOG " + std::to_string((int)sog_kt) +
                    " knots (surface vessel max ~50 kt)\"}";
                spdlog::warn("[ALERT] AIS spoofing: MMSI={} SOG={:.1f} kt", mmsi, sog_kt);
            }
            // MMSI sanity: must be 9 digits, 100000000–999999999
            // Only set if no higher-severity alert already fired.
            if (r.alert_json.empty() && (mmsi < 100000000 || mmsi > 999999999)) {
                r.alert_json = "{\"type\":\"AIS_SPOOFING\",\"severity\":\"MEDIUM\","
                    "\"details\":\"Invalid MMSI " + std::to_string(mmsi) +
                    " (not 9 digits)\"}";
                spdlog::warn("[ALERT] AIS spoofing: invalid MMSI={}", mmsi);
            }
        }
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
