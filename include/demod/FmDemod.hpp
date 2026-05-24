/**
 * @file FmDemod.hpp
 * @brief FM (Wide-band and Narrow-band) demodulator.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

/**
 * @class FmDemod
 * @brief Frequency-modulation demodulator producing PCM audio output.
 *
 * Uses the liquid-dsp @c freqdem discriminator followed by polyphase
 * resampling to the requested output sample rate.
 *
 * For FM_WB (deviation >= 50 kHz) a 75 µs de-emphasis IIR filter is applied
 * after discriminator to match the Americas broadcast standard.
 *
 * @note Instantiate with @p deviation_hz = 75000 for FM_WB, 2500–5000 for
 *       FM_NB.
 */
class FmDemod : public IDemod {
public:
    /**
     * @brief Construct an FmDemod.
     *
     * @param deviation_hz      Peak FM carrier deviation in Hz.
     *                          Use 75000 for Wide-band FM, 2500–5000 for NBFM.
     * @param output_sample_rate Desired output PCM sample rate in Hz (default 48000).
     */
    explicit FmDemod(double deviation_hz, int output_sample_rate = 48000);

    /// @brief Destructor.
    ~FmDemod() override;

    /**
     * @brief FM-demodulate an IQ block to PCM audio.
     *
     * @param iq             Baseband IQ samples centred at the signal carrier.
     * @param sr_sps         IQ sample rate in samples/second.
     * @param center_freq_hz Centre frequency of the capture in Hz.
     * @param timestamp_ms   Capture start timestamp in milliseconds.
     * @return DemodResult   with type == DemodClass::Audio and audio samples
     *                       at @p output_sample_rate.
     */
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        double sr_sps, double center_freq_hz,
                        int64_t timestamp_ms) override;

private:
    double deviation_hz_; ///< Peak FM deviation in Hz.
    int    out_sr_;       ///< Output PCM sample rate in Hz.
};

} // namespace demod
