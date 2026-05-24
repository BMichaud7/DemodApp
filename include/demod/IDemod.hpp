#pragma once
#include <complex>
#include <cstdint>
#include <vector>
#include <string>

namespace demod {

enum class DemodClass { Audio, Bits, RawIq };

struct DemodResult {
    DemodClass  type;
    std::string modulation;
    double      center_freq_hz;
    double      sample_rate_hz;   // audio: output PCM rate; bits: symbol rate
    int64_t     timestamp_ms;
    int64_t     duration_ms;
    int         bits_per_symbol = 1;

    // Exactly one of these is populated:
    std::vector<float>        audio;   // PCM float32, 48 kHz mono
    std::vector<uint8_t>      bits;    // packed bytes, MSB-first
    std::vector<std::complex<float>> raw_iq;
};

struct DemodParams {
    double   sample_rate_sps;
    double   bandwidth_hz;
    int64_t  duration_ms;
    DemodClass out_class;
    int      m_ary = 2;            // FSK/PSK/QAM order
    double   fm_deviation_hz = 0;  // FM only
};

class IDemod {
public:
    virtual ~IDemod() = default;
    virtual DemodResult process(const std::vector<std::complex<float>>& iq,
                                double sr_sps,
                                double center_freq_hz,
                                int64_t timestamp_ms) = 0;
};

} // namespace demod
