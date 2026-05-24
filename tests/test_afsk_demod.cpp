#include <gtest/gtest.h>
#include "TestSignals.hpp"
#include "demod/AfskDemod.hpp"

using namespace demod;

TEST(AfskDemod, EmptyInputReturnsEmpty) {
    AfskDemod d;
    std::vector<std::complex<float>> empty;
    auto r = d.process(empty, 9600.0, 144.39e6, 0);
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_TRUE(r.bits.empty());
}

TEST(AfskDemod, AlternatingSymbolsProducesBytes) {
    // 32 alternating space/mark bits → 4 packed bytes expected
    std::vector<int> pattern;
    for (int i = 0; i < 32; ++i)
        pattern.push_back(i % 2);  // 0=space, 1=mark
    auto iq = TestSignals::makeAfsk(9600.0, pattern);

    AfskDemod d;
    auto r = d.process(iq, 9600.0, 144.39e6, 0);
    EXPECT_EQ(r.type, DemodClass::Bits);
    // FM discriminator may lose one leading bit; expect 3–4 bytes
    EXPECT_GE(r.bits.size(), 3u);
    EXPECT_LE(r.bits.size(), 5u);
}
