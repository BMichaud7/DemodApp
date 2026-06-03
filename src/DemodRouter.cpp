#include "DemodRouter.hpp"
#include "demod/FmDemod.hpp"
#include "demod/AmDemod.hpp"
#include "demod/FskDemod.hpp"
#include "demod/PskQamDemod.hpp"
#include "demod/CwDemod.hpp"
#include "demod/AfskDemod.hpp"
#include "demod/DmrDemod.hpp"
#include "demod/NxdnDemod.hpp"
#include "demod/DstarDemod.hpp"
#include "demod/AisDemod.hpp"
#include "demod/PocsagDemod.hpp"
#include "demod/AcarsDemod.hpp"
#include "demod/FlexDemod.hpp"
#include "demod/Mdc1200Demod.hpp"
#include "demod/DtmfDemod.hpp"
#include "demod/EasSameDemod.hpp"
#include "demod/RttyDemod.hpp"
#include "demod/P25Phase2Demod.hpp"
#include "demod/AdsbDemod.hpp"
#include "demod/DscDemod.hpp"
#include "demod/NavtexDemod.hpp"
#include "demod/Vdl2Demod.hpp"
#include "demod/Psk31Demod.hpp"
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
                         ResultCallback on_result, AlertCallback on_alert)
    : cfg_(cfg), fetcher_(fetcher), on_result_(std::move(on_result)),
      on_alert_(std::move(on_alert)) {}

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
    // ── Protocol-specific demodulators ────────────────────────────────────────
    if (mod == "DMR") {
        return {au::hertz(12'500.0), au::hertz(12'500.0),
                cfg_.engine.digital_duration,
                DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "NXDN") {
        return {au::hertz(12'500.0), au::hertz(12'500.0),
                cfg_.engine.digital_duration,
                DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "DSTAR" || mod == "D-STAR") {
        return {au::hertz(12'500.0), au::hertz(6'250.0),
                cfg_.engine.digital_duration,
                DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "AIS") {
        return {au::hertz(25'000.0), au::hertz(25'000.0),
                cfg_.engine.digital_duration,
                DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "POCSAG") {
        return {au::hertz(25'000.0), au::hertz(12'500.0),
                cfg_.engine.digital_duration,
                DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "ACARS") {
        return {au::hertz(25'000.0), au::hertz(8'330.0),
                cfg_.engine.digital_duration,
                DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "P25" || mod == "P25_C4FM") {
        return {au::hertz(12'500.0), au::hertz(6'250.0),
                cfg_.engine.audio_duration, DemodClass::Audio, 2, au::hertz(2'500.0)};
    }
    if (mod == "P25_PHASE2") {
        return {au::hertz(12'500.0), au::hertz(6'250.0),
                cfg_.engine.digital_duration, DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "FLEX") {
        return {au::hertz(25'000.0), au::hertz(12'500.0),
                cfg_.engine.digital_duration, DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "MDC-1200" || mod == "MDC1200") {
        return {au::hertz(12'500.0), au::hertz(6'250.0),
                cfg_.engine.digital_duration, DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "DTMF") {
        return {bw * 2.0, bw, cfg_.engine.audio_duration, DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "EAS" || mod == "EAS_SAME" || mod == "SAME") {
        return {au::hertz(25'000.0), au::hertz(8'000.0),
                cfg_.engine.digital_duration, DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "RTTY") {
        return {au::hertz(1'000.0), au::hertz(500.0),
                cfg_.engine.digital_duration, DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "ADS_B" || mod == "ADSB") {
        return {au::hertz(2'000'000.0), au::hertz(1'500'000.0),
                cfg_.engine.digital_duration, DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "DSC") {
        return {au::hertz(6'250.0), au::hertz(3'000.0),
                cfg_.engine.digital_duration, DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "NAVTEX") {
        return {au::hertz(1'000.0), au::hertz(500.0),
                cfg_.engine.digital_duration, DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "VDL2" || mod == "VDL_MODE2") {
        return {au::hertz(25'000.0), au::hertz(12'500.0),
                cfg_.engine.digital_duration, DemodClass::Bits, 2, au::hertz(0.0)};
    }
    if (mod == "PSK31" || mod == "PSK63") {
        return {au::hertz(500.0), au::hertz(100.0),
                cfg_.engine.digital_duration, DemodClass::Bits, 2, au::hertz(0.0)};
    }
    // OFDM / CSS / LFM → raw IQ dump
    return {std::max(bw * 2.0, au::hertz(250'000.0)), bw,
            cfg_.engine.digital_duration,
            DemodClass::RawIq, 2, au::hertz(0.0)};
}

bool DemodRouter::route(const std::string&         modulation,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Hertz>   bandwidth,
                        au::QuantityD<au::Hertz>   symbol_rate,
                        float                      /*confidence*/,
                        au::QuantityD<au::Seconds> timestamp,
                        const std::string&         request_id,
                        const std::string&         stream_id)
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

    // Resolve actual sample rate as Au quantity
    auto actual_sr_q = fetcher_.lastSampleRate();
    auto actual_sr   = actual_sr_q > au::hertz(0.0) ? actual_sr_q : p.sample_rate;
    auto out_rate    = cfg_.engine.audio_sample_rate;

    DemodResult result;

    if (p.out_class == DemodClass::Audio) {
        if (modulation == "FM_WB" || modulation == "FM_NB") {
            FmDemod d(p.fm_deviation, out_rate);
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else {
            AmDemod d(modulation, out_rate);
            result = d.process(iq, actual_sr, center_freq, timestamp);
        }
    } else if (p.out_class == DemodClass::Bits) {
        if (modulation == "OOK") {
            FskDemod d(2, /*is_ook=*/true);
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "FSK" || modulation == "GFSK" ||
                   modulation == "GMSK" || modulation == "MSK") {
            FskDemod d(2);
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "4FSK") {
            FskDemod d(4);
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "8FSK") {
            FskDemod d(8);
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "CW") {
            CwDemod d;
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "AFSK") {
            AfskDemod d;
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "DMR") {
            DmrDemod d;
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "NXDN") {
            NxdnDemod d;
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "DSTAR" || modulation == "D-STAR") {
            DstarDemod d;
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "AIS") {
            AisDemod d;
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "POCSAG") {
            PocsagDemod d;
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "ACARS") {
            AcarsDemod d;
            result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "P25" || modulation == "P25_C4FM") {
            FmDemod d(au::hertz(2500.0), cfg_.engine.audio_sample_rate);
            result = d.process(iq, actual_sr, center_freq, timestamp);
            result.modulation = "P25";
        } else if (modulation == "P25_PHASE2") {
            P25Phase2Demod d; result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "FLEX") {
            FlexDemod d; result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "MDC-1200" || modulation == "MDC1200") {
            Mdc1200Demod d; result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "DTMF") {
            DtmfDemod d; result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "EAS" || modulation == "EAS_SAME" || modulation == "SAME") {
            EasSameDemod d; result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "RTTY") {
            RttyDemod d; result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "ADS_B" || modulation == "ADSB") {
            AdsbDemod d; result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "DSC") {
            DscDemod d; result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "NAVTEX") {
            NavtexDemod d; result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "VDL2" || modulation == "VDL_MODE2") {
            Vdl2Demod d; result = d.process(iq, actual_sr, center_freq, timestamp);
        } else if (modulation == "PSK31" || modulation == "PSK63") {
            Psk31Demod d; result = d.process(iq, actual_sr, center_freq, timestamp);
        } else {
            PskQamDemod d(modulation, symbol_rate);
            result = d.process(iq, actual_sr, center_freq, timestamp);
        }
    } else {
        // Raw IQ dump
        result.type        = DemodClass::RawIq;
        result.modulation  = modulation;
        result.center_freq = center_freq;
        result.sample_rate = actual_sr;
        result.timestamp_ms = static_cast<int64_t>(timestamp.in(au::seconds) * 1000.0);
        result.duration    = p.duration;
        result.raw_iq      = std::move(iq);
    }

    result.stream_id = stream_id;

    // Suppress alert if threat detection is disabled globally or per-validator.
    if (!result.alert_json.empty()) {
        bool emit = cfg_.threat.enabled;
        if (emit) {
            // Per-validator flag check based on alert type in JSON
            const auto& j = result.alert_json;
            if      (j.find("ADSB")     != std::string::npos) emit = cfg_.threat.adsb_enabled;
            else if (j.find("AIS")      != std::string::npos) emit = cfg_.threat.ais_enabled;
            else if (j.find("EAS")      != std::string::npos) emit = cfg_.threat.eas_enabled;
            else if (j.find("DSC")      != std::string::npos) emit = cfg_.threat.dsc_enabled;
            else if (j.find("P25_ROGUE")!= std::string::npos) emit = cfg_.threat.p25_rogue_enabled;
        }
        if (emit && on_alert_)
            on_alert_(result.alert_json, center_freq.in(au::hertz));
        else
            result.alert_json.clear();  // don't propagate suppressed alerts
    }
    on_result_(result);
    return true;
}

} // namespace demod
