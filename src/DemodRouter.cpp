#include "DemodRouter.hpp"
#include "demod/FmDemod.hpp"
#include "demod/AmDemod.hpp"
#include "demod/FskDemod.hpp"
#include "demod/PskQamDemod.hpp"
#include "demod/CwDemod.hpp"
#include "demod/AfskDemod.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

namespace demod {

static std::string genId() {
    static std::atomic<uint64_t> counter{0};
    auto t = std::chrono::steady_clock::now().time_since_epoch().count();
    return "demod-" + std::to_string(t) + "-" + std::to_string(++counter);
}

DemodRouter::DemodRouter(const AppConfig& cfg, IqFetcher& fetcher,
                         ResultCallback on_result)
    : cfg_(cfg), fetcher_(fetcher), on_result_(std::move(on_result)) {}

DemodParams DemodRouter::paramsFor(const std::string&       mod,
                                   au::QuantityD<au::Hertz> bandwidth,
                                   au::QuantityD<au::Hertz> symbol_rate) const
{
    // Use analysis-reported bandwidth; fall back to per-modulation default.
    auto bw = bandwidth > au::hertz(0.0) ? bandwidth : au::hertz(200'000.0);

    if (mod == "FM_WB") {
        return {au::hertz(250'000.0), au::hertz(200'000.0),
                cfg_.engine.audio_duration,
                DemodClass::Audio, 2, au::hertz(75'000.0)};
    }
    if (mod == "FM_NB") {
        return {au::hertz(50'000.0),
                std::min(bw, au::hertz(25'000.0)),
                cfg_.engine.audio_duration,
                DemodClass::Audio, 2, au::hertz(5'000.0)};
    }
    if (mod == "AM_DSB" || mod == "AM_DSB_SC" || mod == "TONE") {
        auto sr = std::max(bw * 3.0, au::hertz(30'000.0));
        return {sr, bw, cfg_.engine.audio_duration, DemodClass::Audio,
                2, au::hertz(0.0)};
    }
    if (mod == "AM_SSB_USB" || mod == "AM_SSB_LSB") {
        auto sr = std::max(bw * 4.0, au::hertz(15'000.0));
        return {sr, bw, cfg_.engine.audio_duration, DemodClass::Audio,
                2, au::hertz(0.0)};
    }
    if (mod == "OOK") {
        return {std::max(bw * 4.0, au::hertz(50'000.0)), bw,
                cfg_.engine.digital_duration,
                DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "FSK" || mod == "GFSK" || mod == "GMSK" || mod == "MSK") {
        auto sr = std::max(bw * 4.0, au::hertz(50'000.0));
        return {sr, bw, cfg_.engine.digital_duration, DemodClass::Bits,
                2, au::hertz(0.0)};
    }
    if (mod == "4FSK") {
        auto sr = std::max(bw * 4.0, au::hertz(100'000.0));
        return {sr, bw, cfg_.engine.digital_duration, DemodClass::Bits,
                4, au::hertz(0.0)};
    }
    if (mod == "8FSK") {
        auto sr = std::max(bw * 4.0, au::hertz(200'000.0));
        return {sr, bw, cfg_.engine.digital_duration, DemodClass::Bits,
                8, au::hertz(0.0)};
    }
    // PSK / QAM / ASK: use symbol_rate hint if available
    if (mod == "BPSK" || mod == "QPSK" || mod == "8PSK" ||
        mod == "16PSK" || mod == "32PSK" ||
        mod == "QAM16" || mod == "QAM32" || mod == "QAM64" || mod == "QAM256" ||
        mod == "4ASK" || mod == "16ASK") {
        auto sr = std::max(bw * 2.0,
                           symbol_rate > au::hertz(0.0)
                               ? symbol_rate * 4.0
                               : au::hertz(50'000.0));
        return {sr, bw, cfg_.engine.digital_duration, DemodClass::Bits,
                2, au::hertz(0.0)};
    }
    if (mod == "CW") {
        return {au::hertz(8'000.0), au::hertz(500.0),
                cfg_.engine.digital_duration,
                DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "AFSK") {
        return {au::hertz(9'600.0), au::hertz(3'000.0),
                cfg_.engine.digital_duration,
                DemodClass::Bits, 2, au::hertz(0.0)};
    }
    // OFDM / CSS / LFM → raw IQ dump
    return {std::max(bw * 2.0, au::hertz(250'000.0)), bw,
            cfg_.engine.digital_duration,
            DemodClass::RawIq, 2, au::hertz(0.0)};
}

bool DemodRouter::route(const std::string&       modulation,
                        au::QuantityD<au::Hertz> center_freq,
                        au::QuantityD<au::Hertz> bandwidth,
                        au::QuantityD<au::Hertz> symbol_rate,
                        float                    /*confidence*/,
                        int64_t                  timestamp_ms,
                        const std::string&       request_id,
                        const std::string&       stream_id)
{
    DemodParams p = paramsFor(modulation, bandwidth, symbol_rate);

    spdlog::info("DemodRouter: {:.3f} MHz {} sr={:.0f} bw={:.0f} dur={:.3f}s",
                 center_freq.in(au::hertz) / 1e6, modulation,
                 p.sample_rate.in(au::hertz), p.bandwidth.in(au::hertz),
                 p.duration.in(au::seconds));

    auto iq = fetcher_.collect(center_freq, p.bandwidth, p.sample_rate,
                               p.duration, request_id);
    if (iq.empty()) {
        spdlog::warn("DemodRouter: no IQ for {:.3f} MHz",
                     center_freq.in(au::hertz) / 1e6);
        return false;
    }

    // Extract raw doubles for DSP calls
    auto   actual_sr_q  = fetcher_.lastSampleRate();
    double actual_sr    = actual_sr_q > au::hertz(0.0)
                              ? actual_sr_q.in(au::hertz)
                              : p.sample_rate.in(au::hertz);
    double cf           = center_freq.in(au::hertz);
    int    out_sr_int   = static_cast<int>(cfg_.engine.audio_sample_rate.in(au::hertz));

    DemodResult result;

    if (p.out_class == DemodClass::Audio) {
        if (modulation == "FM_WB" || modulation == "FM_NB") {
            FmDemod d(p.fm_deviation.in(au::hertz), out_sr_int);
            result = d.process(iq, actual_sr, cf, timestamp_ms);
        } else {
            AmDemod d(modulation, out_sr_int);
            result = d.process(iq, actual_sr, cf, timestamp_ms);
        }
    } else if (p.out_class == DemodClass::Bits) {
        if (modulation == "OOK") {
            FskDemod d(2, /*is_ook=*/true);
            result = d.process(iq, actual_sr, cf, timestamp_ms);
        } else if (modulation == "FSK" || modulation == "GFSK" ||
                   modulation == "GMSK" || modulation == "MSK") {
            FskDemod d(2);
            result = d.process(iq, actual_sr, cf, timestamp_ms);
        } else if (modulation == "4FSK") {
            FskDemod d(4);
            result = d.process(iq, actual_sr, cf, timestamp_ms);
        } else if (modulation == "8FSK") {
            FskDemod d(8);
            result = d.process(iq, actual_sr, cf, timestamp_ms);
        } else if (modulation == "CW") {
            CwDemod d;
            result = d.process(iq, actual_sr, cf, timestamp_ms);
        } else if (modulation == "AFSK") {
            AfskDemod d;
            result = d.process(iq, actual_sr, cf, timestamp_ms);
        } else {
            PskQamDemod d(modulation, symbol_rate.in(au::hertz));
            result = d.process(iq, actual_sr, cf, timestamp_ms);
        }
    } else {
        // Raw IQ dump
        result.type        = DemodClass::RawIq;
        result.modulation  = modulation;
        result.center_freq = center_freq;
        result.sample_rate = au::hertz(actual_sr);
        result.timestamp_ms = timestamp_ms;
        result.duration    = p.duration;
        result.raw_iq      = std::move(iq);
    }

    result.stream_id = stream_id;
    on_result_(result);
    return true;
}

} // namespace demod
