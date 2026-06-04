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
#include "demod/CwDemod.hpp"
#include <string>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

using namespace demod;

TEST(CwDemod, DecodesSOSText) {
    // 80 samples/dit at 8 kHz ≈ 10 ms dit → ~12 WPM
    auto iq = TestSignals::makeCw(8000, 80);
    CwDemod d;
    auto r = d.process(iq, au::hertz(8000.0), au::hertz(7.0e6), au::seconds(0.0));
    EXPECT_EQ(r.type, DemodClass::Bits);
    std::string text(r.bits.begin(), r.bits.end());
    EXPECT_NE(text.find("SOS"), std::string::npos);
}

TEST(CwDemod, EmptyInputReturnsEmpty) {
    CwDemod d;
    std::vector<std::complex<float>> empty;
    auto r = d.process(empty, au::hertz(8000.0), au::hertz(7.0e6), au::seconds(0.0));
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_TRUE(r.bits.empty());
}

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
