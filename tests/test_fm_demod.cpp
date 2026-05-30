#include <gtest/gtest.h>
#include "TestSignals.hpp"
#include "demod/FmDemod.hpp"
#include <algorithm>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

using namespace demod;

TEST(FmDemod, RecoversSineAudio) {
    auto iq = TestSignals::makeFm(250000, 0.1, 1000, 75000);
    FmDemod d(au::hertz(75000.0), au::hertz(48000.0));
    auto r = d.process(iq, au::hertz(250000.0), au::hertz(96.3e6), au::seconds(0.0));
    EXPECT_EQ(r.type, DemodClass::Audio);
    EXPECT_GT(r.audio.size(), 0u);
    // Peak should be in reasonable range
    float peak = *std::max_element(r.audio.begin(), r.audio.end());
    EXPECT_GT(peak, 0.01f);
}

TEST(FmDemod, NbfmProducesAudio) {
    auto iq = TestSignals::makeFm(50000, 0.1, 1000, 5000);
    FmDemod d(au::hertz(5000.0), au::hertz(48000.0));
    auto r = d.process(iq, au::hertz(50000.0), au::hertz(145.5e6), au::seconds(0.0));
    EXPECT_EQ(r.type, DemodClass::Audio);
    EXPECT_GT(r.audio.size(), 0u);
}
