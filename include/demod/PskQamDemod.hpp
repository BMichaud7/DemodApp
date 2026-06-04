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
 * @file PskQamDemod.hpp
 * @brief Coherent PSK/QAM/ASK demodulator using liquid-dsp.
 */
#pragma once
#include "IDemod.hpp"
#include <au/units/hertz.hh>
#include <string>

namespace demod {

/**
 * @class PskQamDemod
 * @brief Coherent PSK, QAM, and ASK demodulator using liquid-dsp symbol
 *        synchroniser and Costas-loop carrier recovery.
 *
 * Supported modulation strings:
 *  - PSK:  BPSK, QPSK, 8PSK, 16PSK, 32PSK
 *  - QAM:  QAM16, QAM32, QAM64, QAM256
 *  - ASK:  4ASK, 16ASK
 *
 * @note When @p symbol_rate_hint_sps == 0 the demodulator infers the symbol
 *       rate from the IQ sample rate and defaults to 1 samp/sym.
 */
class PskQamDemod : public IDemod {
public:
    /**
     * @brief Construct a PskQamDemod.
     *
     * @param modulation          Modulation type string (see class description).
     * @param symbol_rate_hint_sps Optional symbol rate hint in symbols/second.
     *                             Pass 0 to let the demodulator decide.
     */
    explicit PskQamDemod(const std::string& modulation,
                         au::QuantityD<au::Hertz> symbol_rate_hint_sps = au::hertz(0.0));

    /// @brief Destructor.
    ~PskQamDemod() override;

    /**
     * @brief Demodulate PSK/QAM/ASK IQ samples into a packed bit stream.
     *
     * @param iq          Baseband IQ samples.
     * @param sr          IQ sample rate.
     * @param center_freq Centre frequency of the capture.
     * @param timestamp   Capture start timestamp.
     * @return DemodResult with type == DemodClass::Bits.
     */
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        au::QuantityD<au::Hertz>   sr,
                        au::QuantityD<au::Hertz>   center_freq,
                        au::QuantityD<au::Seconds> timestamp) override;

private:
    std::string mod_;           ///< Modulation type string.
    double      sym_rate_hint_; ///< Symbol rate hint in sps (0 = auto).
};

} // namespace demod
