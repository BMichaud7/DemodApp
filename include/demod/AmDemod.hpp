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
     * @param modulation   Modulation type string (see class description).
     * @param output_rate  Desired output PCM sample rate (default 48 kHz).
     */
    explicit AmDemod(const std::string& modulation,
                     au::QuantityD<au::Hertz> output_rate = au::hertz(48000.0));

    /// @brief Destructor.
    ~AmDemod() override;

    /**
     * @brief AM-demodulate an IQ block to PCM audio.
     *
     * @param iq          Baseband IQ samples.
     * @param sr          IQ sample rate.
     * @param center_freq Centre frequency of the capture.
     * @param timestamp   Capture start timestamp.
     * @return DemodResult with type == DemodClass::Audio.
     */
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;

private:
    std::string              mod_;      ///< Modulation type string.
    au::QuantityD<au::Hertz> out_rate_; ///< Output PCM sample rate.
};

} // namespace demod

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
