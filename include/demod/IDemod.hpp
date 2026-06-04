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
/**
 * @file IDemod.hpp
 * @brief Core demodulation interface, result types, and parameter structures.
 */
#pragma once
#include <complex>
#include <cstdint>
#include <vector>
#include <string>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

namespace demod {

/**
 * @brief Discriminates the output category of a demodulation result.
 *
 * @note Exactly one of DemodResult::audio, DemodResult::bits, or
 *       DemodResult::raw_iq is populated in a given DemodResult.
 */
enum class DemodClass {
    Audio,  ///< PCM float32 audio samples.
    Bits,   ///< Packed bytes (bit stream or ASCII text).
    RawIq   ///< Raw IQ samples (no demodulation).
};

/**
 * @brief Output of a single demodulation run.
 *
 * Exactly one of @p audio, @p bits, or @p raw_iq will be non-empty,
 * selected by @p type.
 */
struct DemodResult {
    DemodClass               type;            ///< Which output field is valid.
    std::string              modulation;      ///< Modulation identifier (e.g. "FM_WB").
    au::QuantityD<au::Hertz> center_freq;     ///< Centre frequency of the captured signal.
    au::QuantityD<au::Hertz> sample_rate;     ///< Output PCM rate (Audio) or symbol rate (Bits).
    int64_t                  timestamp_ms;    ///< Capture start time (UNIX milliseconds).
    au::QuantityD<au::Seconds> duration;      ///< Duration of the captured block.
    int                      bits_per_symbol = 1; ///< Bits per symbol for digital modes.
    std::string              stream_id;       ///< Non-empty when produced by a streaming session.

    // Exactly one of these is populated:
    std::vector<float>               audio;  ///< PCM float32, typically 48 kHz mono.
    std::vector<uint8_t>             bits;   ///< Packed bytes, MSB-first.
    std::vector<std::complex<float>> raw_iq; ///< Unprocessed IQ samples.

    /// Optional threat alert JSON populated by content-layer validators.
    ///
    /// Non-empty when the demodulator detects an anomaly (impossible physics,
    /// invalid protocol fields, etc.).  Format:
    /// @code{.json}
    /// {"type":"ADSB_SPOOFING","severity":"HIGH","details":"ICAO AB1234 GS=2200 kt"}
    /// @endcode
    /// DemodRouter checks ThreatDetectionConfig flags before forwarding.
    /// Cleared by DemodRouter if the corresponding validator flag is disabled.
    std::string alert_json;
};

/**
 * @brief Parameters used by DemodRouter to request IQ samples and route them.
 */
struct DemodParams {
    au::QuantityD<au::Hertz>  sample_rate;    ///< Requested IQ sample rate.
    au::QuantityD<au::Hertz>  bandwidth;      ///< Requested capture bandwidth.
    au::QuantityD<au::Seconds> duration;      ///< Duration of IQ capture.
    DemodClass                 out_class;     ///< Expected output class.
    int                        m_ary = 2;    ///< FSK/PSK/QAM modulation order.
    au::QuantityD<au::Hertz>  fm_deviation;  ///< FM peak deviation (FM modes only).
};

/**
 * @class IDemod
 * @brief Abstract interface for all demodulators.
 *
 * All demodulators accept a block of baseband IQ samples and produce a
 * DemodResult.  Implementations are stateless between process() calls.
 */
class IDemod {
public:
    /// @brief Virtual destructor.
    virtual ~IDemod() = default;

    /**
     * @brief Demodulate a block of IQ samples.
     *
     * @param iq          Baseband IQ samples.
     * @param sr          Sample rate of the IQ data.
     * @param center_freq Centre frequency of the capture.
     * @param timestamp   Capture start timestamp.
     * @return DemodResult Populated with the appropriate output field.
     */
    virtual DemodResult process(const std::vector<std::complex<float>>& iq,
                                au::QuantityD<au::Hertz>   sr,
                                au::QuantityD<au::Hertz>   center_freq,
                                au::QuantityD<au::Seconds> timestamp) = 0;
};

} // namespace demod

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
