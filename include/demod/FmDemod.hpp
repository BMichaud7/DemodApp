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
     * @param deviation    Peak FM carrier deviation.
     *                     Use hertz(75000) for Wide-band FM, hertz(2500–5000) for NBFM.
     * @param output_rate  Desired output PCM sample rate (default 48 kHz).
     */
    explicit FmDemod(au::QuantityD<au::Hertz> deviation,
                     au::QuantityD<au::Hertz> output_rate = au::hertz(48000.0));

    /// @brief Destructor.
    ~FmDemod() override;

    /**
     * @brief FM-demodulate an IQ block to PCM audio.
     *
     * @param iq          Baseband IQ samples centred at the signal carrier.
     * @param sr          IQ sample rate.
     * @param center_freq Centre frequency of the capture.
     * @param timestamp   Capture start timestamp.
     * @return DemodResult with type == DemodClass::Audio and audio samples
     *                     at @p output_rate.
     */
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;

private:
    au::QuantityD<au::Hertz> deviation_; ///< Peak FM deviation.
    au::QuantityD<au::Hertz> out_rate_;  ///< Output PCM sample rate.
};

} // namespace demod
