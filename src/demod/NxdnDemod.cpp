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
#include "demod/NxdnDemod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

static constexpr double NXDN_SYMBOL_RATE = 4800.0;
static constexpr double NXDN_DEV_OUTER   = 3150.0;

static uint8_t slice_nxdn(float s) {
    if (s >  1.5f) return 1;
    if (s >  0.0f) return 0;
    if (s > -1.5f) return 2;
    return 3;
}

DemodResult NxdnDemod::process(const std::vector<std::complex<float>>& iq,
                                au::QuantityD<au::Hertz>   sr,
                                au::QuantityD<au::Hertz>   center_freq,
                                au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "NXDN";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(NXDN_SYMBOL_RATE);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0 ? au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    float sps = static_cast<float>(sr.in(au::hertz)/NXDN_SYMBOL_RATE);
    float mod  = static_cast<float>(NXDN_DEV_OUTER/NXDN_SYMBOL_RATE);

    freqdem fdem = freqdem_create(mod);
    symsync_rrrf sync = symsync_rrrf_create_rnyquist(
        LIQUID_FIRFILT_RRC, static_cast<unsigned>(std::round(sps)), 5, 0.2f, 32);
    symsync_rrrf_set_lf_bw(sync, 0.01f);

    std::vector<uint8_t> bits;
    float buf[4]; unsigned n;
    for (const auto& s:iq) {
        float fd; freqdem_demodulate(fdem,s,&fd);
        symsync_rrrf_execute(sync,&fd,1,buf,&n);
        for (unsigned i=0;i<n;++i) {
            uint8_t d=slice_nxdn(buf[i]);
            bits.push_back((d>>1)&1); bits.push_back(d&1);
        }
    }
    freqdem_destroy(fdem); symsync_rrrf_destroy(sync);
    r.bits.reserve((bits.size()+7)/8);
    for (size_t i=0;i+8<=bits.size();i+=8) {
        uint8_t byte=0;
        for(int b=0;b<8;++b) byte|=static_cast<uint8_t>(bits[i+b]<<(7-b));
        r.bits.push_back(byte);
    }
    spdlog::debug("NxdnDemod: {} bytes",r.bits.size());
    return r;
}

} // namespace demod
