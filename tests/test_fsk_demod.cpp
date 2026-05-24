#include <gtest/gtest.h>
#include "TestSignals.hpp"
#include "demod/FskDemod.hpp"

using namespace demod;

TEST(FskDemod, TwofskProducesBits) {
    auto iq = TestSignals::makeFsk(50000, 0.1, 5000, 1200);
    FskDemod d(2);
    auto r = d.process(iq, 50000, 162.4e6, 0);
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_GT(r.bits.size(), 0u);
}

TEST(FskDemod, OokProducesBits) {
    auto iq = TestSignals::makeFsk(50000, 0.05, 5000, 2400);
    FskDemod d(2, true);
    auto r = d.process(iq, 50000, 433.9e6, 0);
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_GT(r.bits.size(), 0u);
}
