/*
========================================================================
Project: OpenRFStack
Author:  Brendan Michaud
Year:    2026
Part of OpenRFStack (https://github.com/OpenRFStack)

Licensed under the Personal Use License.
Do not use for commercial, organizational, or military purposes.
Contact author for permission: https://github.com/OpenRFStack
========================================================================
*/
#include <gtest/gtest.h>
#include "TestSignals.hpp"
#include "demod/AmDemod.hpp"
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

using namespace demod;

TEST(AmDemod, DsbRecoversTone) {
    auto iq = TestSignals::makeAmDsb(30000, 0.1, 1000);
    AmDemod d("AM_DSB", au::hertz(48000.0));
    auto r = d.process(iq, au::hertz(30000.0), au::hertz(1.0e6), au::seconds(0.0));
    EXPECT_EQ(r.type, DemodClass::Audio);
    EXPECT_GT(r.audio.size(), 0u);
}

TEST(AmDemod, ToneProducesAudio) {
    auto iq = TestSignals::makeAmDsb(30000, 0.1, 800, 1.0);
    AmDemod d("TONE", au::hertz(48000.0));
    auto r = d.process(iq, au::hertz(30000.0), au::hertz(1.0e6), au::seconds(0.0));
    EXPECT_EQ(r.type, DemodClass::Audio);
    EXPECT_GT(r.audio.size(), 0u);
}
