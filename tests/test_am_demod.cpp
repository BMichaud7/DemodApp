#include <gtest/gtest.h>
#include "TestSignals.hpp"
#include "demod/AmDemod.hpp"

using namespace demod;

TEST(AmDemod, DsbRecoversTone) {
    auto iq = TestSignals::makeAmDsb(30000, 0.1, 1000);
    AmDemod d("AM_DSB", 48000);
    auto r = d.process(iq, 30000, 1.0e6, 0);
    EXPECT_EQ(r.type, DemodClass::Audio);
    EXPECT_GT(r.audio.size(), 0u);
}

TEST(AmDemod, ToneProducesAudio) {
    auto iq = TestSignals::makeAmDsb(30000, 0.1, 800, 1.0);
    AmDemod d("TONE", 48000);
    auto r = d.process(iq, 30000, 1.0e6, 0);
    EXPECT_EQ(r.type, DemodClass::Audio);
    EXPECT_GT(r.audio.size(), 0u);
}
