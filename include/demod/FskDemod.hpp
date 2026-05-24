/**
 * @file FskDemod.hpp
 * @brief Non-coherent M-ary FSK demodulator (FSK, GFSK, GMSK, MSK, OOK).
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

/**
 * @class FskDemod
 * @brief Non-coherent M-ary frequency-shift-keying demodulator.
 *
 * Produces a packed bit stream (DemodClass::Bits) from FSK, GFSK, GMSK,
 * MSK, 4FSK, 8FSK, or OOK signals.
 *
 * For OOK (@p is_ook = true) an envelope threshold replaces the frequency
 * discriminant.
 *
 * @note Supported M-ary values: 2, 4, 8.
 */
class FskDemod : public IDemod {
public:
    /**
     * @brief Construct an FskDemod.
     *
     * @param m_ary  Modulation order: 2 for binary FSK/MSK/GMSK/GFSK/OOK,
     *               4 for 4FSK, 8 for 8FSK.
     * @param is_ook Use amplitude (envelope) thresholding instead of
     *               frequency discriminant.  Only meaningful when @p m_ary == 2.
     */
    explicit FskDemod(unsigned int m_ary, bool is_ook = false);

    /// @brief Destructor.
    ~FskDemod() override;

    /**
     * @brief FSK-demodulate an IQ block to a packed bit stream.
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
    unsigned int m_ary_;  ///< Modulation order.
    bool         is_ook_; ///< True when OOK envelope detection is used.
};

} // namespace demod
