#include <gtest/gtest.h>
#include "TestSignals.hpp"
#include "demod/FmDemod.hpp"
#include <algorithm>

using namespace demod;

TEST(FmDemod, RecoversSineAudio) {
    auto iq = TestSignals::makeFm(250000, 0.1, 1000, 75000);
    FmDemod d(75000, 48000);
    auto r = d.process(iq, 250000, 96.3e6, 0);
    EXPECT_EQ(r.type, DemodClass::Audio);
    EXPECT_GT(r.audio.size(), 0u);
    // Peak should be in reasonable range
    float peak = *std::max_element(r.audio.begin(), r.audio.end());
    EXPECT_GT(peak, 0.01f);
}

TEST(FmDemod, NbfmProducesAudio) {
    auto iq = TestSignals::makeFm(50000, 0.1, 1000, 5000);
    FmDemod d(5000, 48000);
    auto r = d.process(iq, 50000, 145.5e6, 0);
    EXPECT_EQ(r.type, DemodClass::Audio);
    EXPECT_GT(r.audio.size(), 0u);
}
