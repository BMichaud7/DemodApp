/**
 * @file PskQamDemod.hpp
 * @brief Coherent PSK/QAM/ASK demodulator using liquid-dsp.
 */
#pragma once
#include "IDemod.hpp"
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
                         double symbol_rate_hint_sps = 0.0);

    /// @brief Destructor.
    ~PskQamDemod() override;

    /**
     * @brief Demodulate PSK/QAM/ASK IQ samples into a packed bit stream.
     *
     * @param iq             Baseband IQ samples.
     * @param sr_sps         IQ sample rate in samples/second.
     * @param center_freq_hz Centre frequency of the capture in Hz.
     * @param timestamp_ms   Capture start timestamp in milliseconds.
     * @return DemodResult   with type == DemodClass::Bits.
     */
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        double sr_sps, double center_freq_hz,
                        int64_t timestamp_ms) override;

private:
    std::string mod_;           ///< Modulation type string.
    double      sym_rate_hint_; ///< Symbol rate hint in sps (0 = auto).
};

} // namespace demod
