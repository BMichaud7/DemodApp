/**
 * @file AfskDemod.hpp
 * @brief Bell 202 AFSK demodulator (1200 baud, mark=1200 Hz, space=2200 Hz).
 *
 * Intended for APRS / AX.25 packet radio reception.
 */
#pragma once
#include "IDemod.hpp"

namespace demod {

/**
 * @class AfskDemod
 * @brief Demodulates Bell 202 AFSK (Audio Frequency Shift Keying) into a
 *        packed bit stream.
 *
 * Processing pipeline:
 *  1. FM-discriminate the IQ stream to recover the audio baseband.
 *  2. Correlate each symbol window against the mark (1200 Hz) and space
 *     (2200 Hz) tones to decide each bit.
 *  3. Pack bits MSB-first into bytes and return as DemodClass::Bits.
 *
 * @note Request 9600 Hz sample rate and 3000 Hz bandwidth from IqFetcher.
 * @note The FM demodulation step is performed inline (no FmDemod dependency).
 */
class AfskDemod : public IDemod {
public:
    /**
     * @brief Construct an AfskDemod.
     *
     * No parameters are needed; Bell 202 constants (1200/2200 Hz tones,
     * 1200 baud) are hardcoded per the standard.
     */
    AfskDemod();

    /// @brief Destructor.
    ~AfskDemod() override;

    /**
     * @brief Demodulate AFSK IQ data into a packed bit stream.
     *
     * @param iq            Baseband IQ samples at the requested sample rate.
     * @param sr_sps        Actual sample rate of the IQ data in samples/second.
     * @param center_freq_hz Centre frequency used during capture (informational).
     * @param timestamp_ms  Capture start timestamp in milliseconds.
     * @return DemodResult  with type == DemodClass::Bits containing the
     *                      packed bit stream (MSB-first, Bell 202 encoding:
     *                      mark=1, space=0).
     */
    DemodResult process(const std::vector<std::complex<float>>& iq,
                        double sr_sps,
                        double center_freq_hz,
                        int64_t timestamp_ms) override;
};

} // namespace demod
