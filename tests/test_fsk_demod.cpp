#include <gtest/gtest.h>
#include "TestSignals.hpp"
#include "demod/FskDemod.hpp"
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

using namespace demod;

TEST(FskDemod, TwofskProducesBits) {
    auto iq = TestSignals::makeFsk(50000, 0.1, 5000, 1200);
    FskDemod d(2);
    auto r = d.process(iq, au::hertz(50000.0), au::hertz(162.4e6), au::seconds(0.0));
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_GT(r.bits.size(), 0u);
}

TEST(FskDemod, OokProducesBits) {
    auto iq = TestSignals::makeFsk(50000, 0.05, 5000, 2400);
    FskDemod d(2, true);
    auto r = d.process(iq, au::hertz(50000.0), au::hertz(433.9e6), au::seconds(0.0));
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_GT(r.bits.size(), 0u);
}
