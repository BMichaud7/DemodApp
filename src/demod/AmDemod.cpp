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
#include "demod/AmDemod.hpp"
#include <liquid/liquid.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

namespace demod {

AmDemod::AmDemod(const std::string& modulation, au::QuantityD<au::Hertz> output_rate)
    : mod_(modulation), out_rate_(output_rate) {}

AmDemod::~AmDemod() = default;

DemodResult AmDemod::process(const std::vector<std::complex<float>>& iq,
                              au::QuantityD<au::Hertz>   sr,
                              au::QuantityD<au::Hertz>   center_freq,
                              au::QuantityD<au::Seconds> timestamp)
{
    // Extract raw values for DSP math
    const double sr_sps         = sr.in(au::hertz);
    const double center_freq_hz = center_freq.in(au::hertz);
    const double out_sr_hz      = out_rate_.in(au::hertz);
    const int    out_sr_int     = static_cast<int>(out_sr_hz);

    DemodResult r;
    r.type        = DemodClass::Audio;
    r.modulation  = mod_;
    r.center_freq = center_freq;
    r.sample_rate = out_rate_;
    r.timestamp_ms = static_cast<int64_t>(timestamp.in(au::seconds) * 1000.0);
    r.duration    = au::seconds(iq.size() / sr_sps);

    liquid_ampmodem_type type = LIQUID_AMPMODEM_DSB;
    int suppressed = 0;
    if (mod_ == "AM_DSB_SC") { type = LIQUID_AMPMODEM_DSB; suppressed = 1; }
    else if (mod_ == "AM_SSB_USB") { type = LIQUID_AMPMODEM_USB; suppressed = 1; }
    else if (mod_ == "AM_SSB_LSB") { type = LIQUID_AMPMODEM_LSB; suppressed = 1; }

    ampmodem demod = ampmodem_create(1.0f, type, suppressed);
    if (!demod) throw std::runtime_error("ampmodem_create failed");

    std::vector<float> baseband(iq.size());
    ampmodem_demodulate_block(demod,
        reinterpret_cast<liquid_float_complex*>(
            const_cast<std::complex<float>*>(iq.data())),
        static_cast<unsigned int>(iq.size()),
        baseband.data());
    ampmodem_destroy(demod);

    // Resample to out_sr_int
    float rate = static_cast<float>(out_sr_int) / static_cast<float>(sr_sps);
    msresamp_rrrf resamp = msresamp_rrrf_create(rate, 60.0f);

    constexpr unsigned int BLOCK = 4096;
    r.audio.reserve(static_cast<size_t>(baseband.size() * rate) + 64);
    unsigned int n_in = static_cast<unsigned int>(baseband.size());
    unsigned int offset = 0;
    while (offset < n_in) {
        unsigned int chunk = std::min(BLOCK, n_in - offset);
        unsigned int n_out = 0;
        std::vector<float> out_buf(static_cast<size_t>(chunk * rate) + 64);
        msresamp_rrrf_execute(resamp, baseband.data() + offset,
                              chunk, out_buf.data(), &n_out);
        r.audio.insert(r.audio.end(), out_buf.begin(),
                       out_buf.begin() + n_out);
        offset += chunk;
    }
    msresamp_rrrf_destroy(resamp);

    spdlog::info("AmDemod: {:.3f} MHz {} → {} audio samples @ {:.0f} Hz",
                 center_freq_hz / 1e6, mod_, r.audio.size(), out_sr_hz);
    return r;
}

} // namespace demod
