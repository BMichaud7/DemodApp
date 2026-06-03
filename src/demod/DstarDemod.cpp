#include "demod/DstarDemod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

// D-STAR: GMSK 4800 bps, BT=0.5
static constexpr double DSTAR_BAUD = 4800.0;

DemodResult DstarDemod::process(const std::vector<std::complex<float>>& iq,
                                 au::QuantityD<au::Hertz>   sr,
                                 au::QuantityD<au::Hertz>   center_freq,
                                 au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "DSTAR";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(DSTAR_BAUD);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0 ? au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    float sps = static_cast<float>(sr.in(au::hertz)/DSTAR_BAUD);
    // GMSK mod index = 0.5 → mod_index = BT/2 in freqdem terms ≈ 0.25
    freqdem fdem = freqdem_create(0.25f);
    symsync_rrrf sync = symsync_rrrf_create_rnyquist(
        LIQUID_FIRFILT_GMSKRX, static_cast<unsigned>(std::round(sps)), 5, 0.3f, 32);
    symsync_rrrf_set_lf_bw(sync, 0.01f);

    std::vector<uint8_t> bits;
    float buf[4]; unsigned n;
    for (const auto& s:iq) {
        float fd; freqdem_demodulate(fdem,s,&fd);
        symsync_rrrf_execute(sync,&fd,1,buf,&n);
        for (unsigned i=0;i<n;++i) bits.push_back(buf[i]>0.0f?1:0);
    }
    freqdem_destroy(fdem); symsync_rrrf_destroy(sync);
    r.bits.reserve((bits.size()+7)/8);
    for (size_t i=0;i+8<=bits.size();i+=8) {
        uint8_t byte=0;
        for(int b=0;b<8;++b) byte|=static_cast<uint8_t>(bits[i+b]<<(7-b));
        r.bits.push_back(byte);
    }
    spdlog::debug("DstarDemod: {} bytes", r.bits.size());
    return r;
}

} // namespace demod
