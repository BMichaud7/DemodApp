#include <gtest/gtest.h>
#include "TestSignals.hpp"
#include "demod/PskQamDemod.hpp"
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

using namespace demod;

TEST(PskQamDemod, BpskProducesBits) {
    auto iq = TestSignals::makeBpsk(50000, 0.1, 5000);
    PskQamDemod d("BPSK", au::hertz(5000.0));
    auto r = d.process(iq, au::hertz(50000.0), au::hertz(144.0e6), au::seconds(0.0));
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_GT(r.bits.size(), 0u);
}
