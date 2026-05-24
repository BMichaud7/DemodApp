/**
 * @file IDemod.hpp
 * @brief Core demodulation interface, result types, and parameter structures.
 */
#pragma once
#include <complex>
#include <cstdint>
#include <vector>
#include <string>

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
    DemodClass  type;             ///< Which output field is valid.
    std::string modulation;       ///< Modulation identifier (e.g. "FM_WB").
    double      center_freq_hz;   ///< Centre frequency of the captured signal.
    double      sample_rate_hz;   ///< Output PCM rate (Audio) or symbol rate (Bits).
    int64_t     timestamp_ms;     ///< Capture start time (UNIX milliseconds).
    int64_t     duration_ms;      ///< Duration of the captured block.
    int         bits_per_symbol = 1; ///< Bits per symbol for digital modes.
    std::string stream_id;          ///< Non-empty when produced by a streaming session.

    // Exactly one of these is populated:
    std::vector<float>               audio;  ///< PCM float32, typically 48 kHz mono.
    std::vector<uint8_t>             bits;   ///< Packed bytes, MSB-first.
    std::vector<std::complex<float>> raw_iq; ///< Unprocessed IQ samples.
};

/**
 * @brief Parameters used by DemodRouter to request IQ samples and route them.
 */
struct DemodParams {
    double     sample_rate_sps;       ///< Requested IQ sample rate in sps.
    double     bandwidth_hz;          ///< Requested capture bandwidth in Hz.
    int64_t    duration_ms;           ///< Duration of IQ capture in milliseconds.
    DemodClass out_class;             ///< Expected output class.
    int        m_ary = 2;             ///< FSK/PSK/QAM modulation order.
    double     fm_deviation_hz = 0;   ///< FM peak deviation (FM modes only).
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
     * @param iq             Baseband IQ samples.
     * @param sr_sps         Sample rate of the IQ data in samples/second.
     * @param center_freq_hz Centre frequency of the capture in Hz.
     * @param timestamp_ms   Capture start timestamp in milliseconds.
     * @return DemodResult   Populated with the appropriate output field.
     */
    virtual DemodResult process(const std::vector<std::complex<float>>& iq,
                                double sr_sps,
                                double center_freq_hz,
                                int64_t timestamp_ms) = 0;
};

} // namespace demod
