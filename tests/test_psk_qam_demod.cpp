#include <gtest/gtest.h>
#include "TestSignals.hpp"
#include "demod/PskQamDemod.hpp"

using namespace demod;

TEST(PskQamDemod, BpskProducesBits) {
    auto iq = TestSignals::makeBpsk(50000, 0.1, 5000);
    PskQamDemod d("BPSK", 5000);
    auto r = d.process(iq, 50000, 144.0e6, 0);
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_GT(r.bits.size(), 0u);
}
