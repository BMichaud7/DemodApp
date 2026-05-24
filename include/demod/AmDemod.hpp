/**
 * @file AmDemod.hpp
 * @brief Amplitude-modulation demodulator (DSB, SSB, SC, TONE).
 */
#pragma once
#include "IDemod.hpp"
#include <string>

namespace demod {

/**
 * @class AmDemod
 * @brief AM demodulator supporting DSB, DSB-SC, USB, LSB, and TONE.
 *
 * Handles the following modulation strings:
 *  - @c AM_DSB   — Double-sideband AM with carrier (envelope detection).
 *  - @c AM_DSB_SC — Double-sideband suppressed-carrier (product detection).
 *  - @c AM_SSB_USB — Upper sideband SSB.
 *  - @c AM_SSB_LSB — Lower sideband SSB.
 *  - @c TONE     — Single-tone CW carrier (envelope detection).
 *
 * Output is resampled to the specified @p output_sample_rate.
 */
class AmDemod : public IDemod {
public:
    /**
     * @brief Construct an AmDemod.
     *
     * @param modulation        Modulation type string (see class description).
     * @param output_sample_rate Desired output PCM sample rate in Hz (default 48000).
     */
    explicit AmDemod(const std::string& modulation, int output_sample_rate = 48000);

    /// @brief Destructor.
    ~AmDemod() override;

    /**
     * @brief AM-demodulate an IQ block to PCM audio.
     *
     * @param iq             Baseband IQ samples.
     * @param sr_sps         IQ sample rate in samples/second.
     * @param center_freq_hz Centre frequency of the capture in Hz.
     * @param timestamp_ms   Capture start timestamp in milliseconds.
     * @return DemodResult   with type == DemodClass::Audio.
     */
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        double sr_sps, double center_freq_hz,
                        int64_t timestamp_ms) override;

private:
    std::string mod_;   ///< Modulation type string.
    int         out_sr_; ///< Output PCM sample rate in Hz.
};

} // namespace demod
