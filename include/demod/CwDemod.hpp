/**
 * @file CwDemod.hpp
 * @brief Morse code (CW) demodulator — IQ → ASCII text.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

/**
 * @class CwDemod
 * @brief Demodulates a keyed CW (Morse code) carrier into ASCII text.
 *
 * The input IQ stream should be centered near DC (the CW tone sits at or
 * close to 0 Hz after any prior frequency correction).  The demodulator
 * performs envelope detection, IIR smoothing, adaptive thresholding, run-
 * length classification into dits/dahs, and finally table-driven Morse
 * decoding.
 *
 * @note Designed for 8 kHz sample rate input (request via IqFetcher).
 *       Acceptable range: 8000 – 48000 sps.
 */
class CwDemod : public IDemod {
public:
    /**
     * @brief Construct a CwDemod.
     *
     * No parameters are required; all timing is estimated from the signal.
     */
    CwDemod();

    /// @brief Destructor.
    ~CwDemod() override;

    /**
     * @brief Decode Morse code from IQ samples.
     *
     * @param iq            Baseband IQ samples (CW carrier near DC).
     * @param sr_sps        Sample rate of the IQ data in samples/second.
     * @param center_freq_hz Centre frequency used during capture (informational).
     * @param timestamp_ms  Capture start timestamp in milliseconds.
     * @return DemodResult  with type == DemodClass::Bits and bits containing
     *                      the decoded ASCII text.  Returns an empty bits
     *                      vector when no marks are detected.
     */
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        double sr_sps,
                        double center_freq_hz,
                        int64_t timestamp_ms) override;
};

} // namespace demod
