#include "demod/FmDemod.hpp"
#include <liquid/liquid.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <stdexcept>

namespace demod {

FmDemod::FmDemod(double deviation_hz, int output_sample_rate)
    : deviation_hz_(deviation_hz), out_sr_(output_sample_rate) {}

FmDemod::~FmDemod() = default;

DemodResult FmDemod::process(const std::vector<std::complex<float>>& iq,
                              double sr_sps,
                              double center_freq_hz,
                              int64_t timestamp_ms)
{
    DemodResult r;
    r.type           = DemodClass::Audio;
    r.modulation     = deviation_hz_ >= 50000.0 ? "FM_WB" : "FM_NB";
    r.center_freq_hz = center_freq_hz;
    r.sample_rate_hz = out_sr_;
    r.timestamp_ms   = timestamp_ms;
    r.duration_ms    = static_cast<int64_t>(iq.size() / sr_sps * 1000.0);

    // kf = deviation / sample_rate (modulation factor for liquid freqdem)
    float kf = static_cast<float>(deviation_hz_ / sr_sps);
    kf = std::clamp(kf, 0.01f, 0.49f);

    freqdem demod = freqdem_create(kf);
    if (!demod) throw std::runtime_error("freqdem_create failed");

    std::vector<float> discrim(iq.size());
    freqdem_demodulate_block(demod,
        reinterpret_cast<liquid_float_complex*>(
            const_cast<std::complex<float>*>(iq.data())),
        static_cast<unsigned int>(iq.size()),
        discrim.data());
    freqdem_destroy(demod);

    // De-emphasis filter (FM WB only, τ = 75 µs — Americas standard)
    if (deviation_hz_ >= 50000.0) {
        constexpr double tau = 75e-6;
        const double alpha_de = (1.0 / sr_sps) / (tau + 1.0 / sr_sps);
        double y_prev = 0.0;
        for (float& x : discrim) {
            double y = alpha_de * x + (1.0 - alpha_de) * y_prev;
            x = static_cast<float>(y);
            y_prev = y;
        }
    }

    // Resample from sr_sps → out_sr_
    float rate = static_cast<float>(out_sr_) / static_cast<float>(sr_sps);
    msresamp_rrrf resamp = msresamp_rrrf_create(rate, 60.0f);

    // Process in blocks to avoid huge intermediate allocations
    constexpr unsigned int BLOCK = 4096;
    std::vector<float> out_buf(static_cast<size_t>(BLOCK * rate) + 64);
    r.audio.reserve(static_cast<size_t>(iq.size() * rate) + 64);

    unsigned int n_in = static_cast<unsigned int>(discrim.size());
    unsigned int offset = 0;
    while (offset < n_in) {
        unsigned int chunk = std::min(BLOCK, n_in - offset);
        unsigned int n_out = 0;
        out_buf.resize(static_cast<size_t>(chunk * rate) + 64);
        msresamp_rrrf_execute(resamp,
                              discrim.data() + offset,
                              chunk,
                              out_buf.data(),
                              &n_out);
        r.audio.insert(r.audio.end(), out_buf.begin(),
                       out_buf.begin() + n_out);
        offset += chunk;
    }
    msresamp_rrrf_destroy(resamp);

    spdlog::info("FmDemod: {:.3f} MHz → {} audio samples @ {} Hz",
                 center_freq_hz / 1e6, r.audio.size(), out_sr_);
    return r;
}

} // namespace demod
