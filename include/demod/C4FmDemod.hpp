/**
 * @file C4FmDemod.hpp
 * @brief C4FM (Continuous 4-Level FM) demodulator for P25 Phase 1.
 *
 * C4FM is 4-level FSK with deviations ±600 Hz and ±1800 Hz,
 * symbol rate 4800 sym/s, yielding 9600 bps.
 *
 * Input:  baseband IQ samples at any sample rate
 * Output: dibit stream (2 bits per symbol, packed as uint8)
 */
#pragma once
#include <complex>
#include <vector>
#include <memory>

namespace demod::p25 {

class C4FmDemod {
public:
    /**
     * @brief Construct C4FM demodulator.
     * @param sample_rate    Input IQ sample rate (Hz).
     * @param symbol_rate    P25 symbol rate (default 4800).
     */
    explicit C4FmDemod(double sample_rate, double symbol_rate = 4800.0);
    ~C4FmDemod();

    C4FmDemod(const C4FmDemod&) = delete;

    /**
     * @brief Process a block of IQ samples.
     * @param samples   CF32 input.
     * @return          Sequence of dibits (0–3), one per recovered symbol.
     */
    std::vector<uint8_t> process(const std::vector<std::complex<float>>& samples);

    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace demod::p25
