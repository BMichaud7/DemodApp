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
    return r;
}

} // namespace demod
