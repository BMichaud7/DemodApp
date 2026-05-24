#pragma once
#include "IDemod.hpp"

namespace demod {

// Non-coherent M-ary FSK demodulator. Handles FSK, GFSK, GMSK, MSK,
// 4FSK, 8FSK, OOK (M=2 with envelope threshold for OOK).
class FskDemod : public IDemod {
public:
    // m_ary: 2 for binary FSK/MSK/GMSK/GFSK/OOK, 4 for 4FSK, 8 for 8FSK
    // is_ook: use envelope thresholding instead of frequency discriminant
    explicit FskDemod(unsigned int m_ary, bool is_ook = false);
    ~FskDemod() override;

    DemodResult process(const std::vector<std::complex<float>>& iq,
                        double sr_sps, double center_freq_hz,
                        int64_t timestamp_ms) override;

private:
    unsigned int m_ary_;
    bool         is_ook_;
};

} // namespace demod
