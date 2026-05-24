#include "DemodRouter.hpp"
#include "demod/FmDemod.hpp"
#include "demod/AmDemod.hpp"
#include "demod/FskDemod.hpp"
#include "demod/PskQamDemod.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace demod {

static std::string genId() {
    static std::atomic<uint64_t> counter{0};
    auto t = std::chrono::steady_clock::now().time_since_epoch().count();
    return "demod-" + std::to_string(t) + "-" + std::to_string(++counter);
}

DemodRouter::DemodRouter(const AppConfig& cfg, IqFetcher& fetcher,
                         ResultCallback on_result)
    : cfg_(cfg), fetcher_(fetcher), on_result_(std::move(on_result)) {}

DemodParams DemodRouter::paramsFor(const std::string& mod,
                                   double bandwidth_hz,
                                   double symbol_rate_sps) const
{
    // Use analysis-reported bandwidth; fall back to per-modulation defaults.
    double bw = bandwidth_hz > 0 ? bandwidth_hz : 200'000.0;

    if (mod == "FM_WB") {
        return {250'000, 200'000, cfg_.engine.audio_duration_ms,
                DemodClass::Audio, 2, 75'000};
    }
    if (mod == "FM_NB") {
        return {50'000, std::min(bw, 25'000.0), cfg_.engine.audio_duration_ms,
                DemodClass::Audio, 2, 5'000};
    }
    if (mod == "AM_DSB" || mod == "AM_DSB_SC" || mod == "TONE") {
        double sr = std::max(bw * 3, 30'000.0);
        return {sr, bw, cfg_.engine.audio_duration_ms, DemodClass::Audio};
    }
    if (mod == "AM_SSB_USB" || mod == "AM_SSB_LSB") {
        double sr = std::max(bw * 4, 15'000.0);
        return {sr, bw, cfg_.engine.audio_duration_ms, DemodClass::Audio};
    }
    if (mod == "OOK") {
        return {std::max(bw * 4, 50'000.0), bw, cfg_.engine.digital_duration_ms,
                DemodClass::Bits, 2};
    }
    if (mod == "FSK" || mod == "GFSK" || mod == "GMSK" || mod == "MSK") {
        double sr = std::max(bw * 4, 50'000.0);
        return {sr, bw, cfg_.engine.digital_duration_ms, DemodClass::Bits, 2};
    }
    if (mod == "4FSK") {
        double sr = std::max(bw * 4, 100'000.0);
        return {sr, bw, cfg_.engine.digital_duration_ms, DemodClass::Bits, 4};
    }
    if (mod == "8FSK") {
        double sr = std::max(bw * 4, 200'000.0);
        return {sr, bw, cfg_.engine.digital_duration_ms, DemodClass::Bits, 8};
    }
    // PSK / QAM / ASK: use symbol_rate hint if available
    if (mod == "BPSK" || mod == "QPSK" || mod == "8PSK" ||
        mod == "16PSK" || mod == "32PSK" ||
        mod == "QAM16" || mod == "QAM32" || mod == "QAM64" || mod == "QAM256" ||
        mod == "4ASK" || mod == "16ASK") {
        double sr = std::max(bw * 2, symbol_rate_sps > 0 ? symbol_rate_sps * 4 : 50'000.0);
        return {sr, bw, cfg_.engine.digital_duration_ms, DemodClass::Bits};
    }
    // OFDM / CSS / LFM → raw IQ dump
    return {std::max(bw * 2, 250'000.0), bw, cfg_.engine.digital_duration_ms,
            DemodClass::RawIq};
}

void DemodRouter::route(const std::string& modulation,
                        double center_freq_hz,
                        double bandwidth_hz,
                        double symbol_rate_sps,
                        float  /*confidence*/,
                        int64_t timestamp_ms,
                        const std::string& request_id)
{
    DemodParams p = paramsFor(modulation, bandwidth_hz, symbol_rate_sps);

    spdlog::info("DemodRouter: {:.3f} MHz {} sr={:.0f} bw={:.0f} dur={}ms",
                 center_freq_hz / 1e6, modulation,
                 p.sample_rate_sps, p.bandwidth_hz, p.duration_ms);

    auto iq = fetcher_.collect(center_freq_hz, p.bandwidth_hz,
                               p.sample_rate_sps, p.duration_ms, request_id);
    if (iq.empty()) {
        spdlog::warn("DemodRouter: no IQ for {:.3f} MHz", center_freq_hz / 1e6);
        return;
    }

    double actual_sr = fetcher_.lastSampleRate();
    if (actual_sr <= 0) actual_sr = p.sample_rate_sps;

    DemodResult result;

    if (p.out_class == DemodClass::Audio) {
        if (modulation == "FM_WB" || modulation == "FM_NB") {
            FmDemod d(p.fm_deviation_hz, cfg_.engine.audio_sample_rate);
            result = d.process(iq, actual_sr, center_freq_hz, timestamp_ms);
        } else {
            AmDemod d(modulation, cfg_.engine.audio_sample_rate);
            result = d.process(iq, actual_sr, center_freq_hz, timestamp_ms);
        }
    } else if (p.out_class == DemodClass::Bits) {
        if (modulation == "OOK") {
            FskDemod d(2, /*is_ook=*/true);
            result = d.process(iq, actual_sr, center_freq_hz, timestamp_ms);
        } else if (modulation == "FSK" || modulation == "GFSK" ||
                   modulation == "GMSK" || modulation == "MSK") {
            FskDemod d(2);
            result = d.process(iq, actual_sr, center_freq_hz, timestamp_ms);
        } else if (modulation == "4FSK") {
            FskDemod d(4);
            result = d.process(iq, actual_sr, center_freq_hz, timestamp_ms);
        } else if (modulation == "8FSK") {
            FskDemod d(8);
            result = d.process(iq, actual_sr, center_freq_hz, timestamp_ms);
        } else {
            PskQamDemod d(modulation, symbol_rate_sps);
            result = d.process(iq, actual_sr, center_freq_hz, timestamp_ms);
        }
    } else {
        // Raw IQ dump
        result.type           = DemodClass::RawIq;
        result.modulation     = modulation;
        result.center_freq_hz = center_freq_hz;
        result.sample_rate_hz = actual_sr;
        result.timestamp_ms   = timestamp_ms;
        result.duration_ms    = p.duration_ms;
        result.raw_iq         = std::move(iq);
    }

    on_result_(result);
}

} // namespace demod
