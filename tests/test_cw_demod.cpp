#include <gtest/gtest.h>
#include "TestSignals.hpp"
#include "demod/CwDemod.hpp"
#include <string>

using namespace demod;

TEST(CwDemod, DecodesSOSText) {
    // 80 samples/dit at 8 kHz ≈ 10 ms dit → ~12 WPM
    auto iq = TestSignals::makeCw(8000, 80);
    CwDemod d;
    auto r = d.process(iq, 8000, 7.0e6, 0);
    EXPECT_EQ(r.type, DemodClass::Bits);
    std::string text(r.bits.begin(), r.bits.end());
    EXPECT_NE(text.find("SOS"), std::string::npos);
}

TEST(CwDemod, EmptyInputReturnsEmpty) {
    CwDemod d;
    std::vector<std::complex<float>> empty;
    auto r = d.process(empty, 8000, 7.0e6, 0);
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_TRUE(r.bits.empty());
}
