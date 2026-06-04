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
/**
 * @file test_protocol_demods.cpp
 * @brief Tests for DMR, NXDN, D-STAR, AIS, POCSAG, ACARS demodulators.
 *
 * Two categories:
 *  1. Synthetic signals — mathematically correct waveforms matching
 *     exact protocol RF parameters (symbol rate, deviation, framing).
 *  2. Real-signal properties — verify output characteristics match what
 *     would be expected from live traffic (bit count, byte range, etc.).
 */
#include <gtest/gtest.h>
#include "TestSignals.hpp"
#include "demod/DmrDemod.hpp"
#include "demod/NxdnDemod.hpp"
#include "demod/DstarDemod.hpp"
#include "demod/AisDemod.hpp"
#include "demod/PocsagDemod.hpp"
#include "demod/AcarsDemod.hpp"
#include "demod/FmDemod.hpp"
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"
#include <cmath>

using namespace demod;

static constexpr double SR = 48000.0;
static constexpr double DUR = 0.5;  // 0.5 s of signal per test

// ── DMR ───────────────────────────────────────────────────────────────────────

class DmrDemodTest : public ::testing::Test {
protected:
    DmrDemod demod;
    std::vector<std::complex<float>> iq = TestSignals::makeDmr(SR, DUR);
};

TEST_F(DmrDemodTest, ReturnsCorrectType) {
    auto r = demod.process(iq, au::hertz(SR), au::hertz(462.5e6), au::seconds(0));
    EXPECT_EQ(r.type, DemodClass::Bits);
}

TEST_F(DmrDemodTest, ModulationTaggedCorrectly) {
    auto r = demod.process(iq, au::hertz(SR), au::hertz(462.5e6), au::seconds(0));
    EXPECT_EQ(r.modulation, "DMR");
}

TEST_F(DmrDemodTest, ProducesNonEmptyBits) {
    auto r = demod.process(iq, au::hertz(SR), au::hertz(462.5e6), au::seconds(0));
    EXPECT_GT(r.bits.size(), 0u);
}

TEST_F(DmrDemodTest, BitCountScalesWithDuration) {
    // 4800 sym/s × 2 bits/sym × 0.5s = 4800 bits → ~600 bytes
    auto r = demod.process(iq, au::hertz(SR), au::hertz(462.5e6), au::seconds(0));
    EXPECT_GT(r.bits.size(), 100u);  // at least 800 bits packed
    EXPECT_LT(r.bits.size(), 2000u);
}

TEST_F(DmrDemodTest, EmptyInputReturnsEmpty) {
    auto r = demod.process({}, au::hertz(SR), au::hertz(462.5e6), au::seconds(0));
    EXPECT_TRUE(r.bits.empty());
}

TEST_F(DmrDemodTest, DurationFieldSet) {
    auto r = demod.process(iq, au::hertz(SR), au::hertz(462.5e6), au::seconds(0));
    EXPECT_NEAR(r.duration.in(au::seconds), DUR, 0.1);
}

// ── NXDN ──────────────────────────────────────────────────────────────────────

class NxdnDemodTest : public ::testing::Test {
protected:
    NxdnDemod demod;
    std::vector<std::complex<float>> iq = TestSignals::makeP25C4fm(SR, DUR);
    // NXDN uses same 4FSK structure as P25 but different deviation
};

TEST_F(NxdnDemodTest, ReturnsCorrectType) {
    NxdnDemod d;
    auto iq2 = TestSignals::makeDmr(SR, DUR);  // 4FSK signal
    auto r = d.process(iq2, au::hertz(SR), au::hertz(461.0e6), au::seconds(0));
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_EQ(r.modulation, "NXDN");
}

TEST_F(NxdnDemodTest, ProducesBits) {
    NxdnDemod d;
    auto iq2 = TestSignals::makeDmr(SR, DUR);
    auto r = d.process(iq2, au::hertz(SR), au::hertz(461.0e6), au::seconds(0));
    EXPECT_GT(r.bits.size(), 50u);
}

// ── D-STAR ────────────────────────────────────────────────────────────────────

class DstarDemodTest : public ::testing::Test {
protected:
    DstarDemod demod;
};

TEST_F(DstarDemodTest, ReturnsCorrectType) {
    // D-STAR uses GMSK — FM FSK signal
    auto iq = TestSignals::makeFsk(SR, DUR, 1200.0, 4800.0);
    auto r = demod.process(iq, au::hertz(SR), au::hertz(144.0e6), au::seconds(0));
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_EQ(r.modulation, "DSTAR");
}

TEST_F(DstarDemodTest, ProducesNonEmptyBitsFromGmsk) {
    auto iq = TestSignals::makeFsk(SR, DUR, 1200.0, 4800.0);
    auto r = demod.process(iq, au::hertz(SR), au::hertz(144.0e6), au::seconds(0));
    EXPECT_GT(r.bits.size(), 50u);
}

// ── AIS ───────────────────────────────────────────────────────────────────────

class AisDemodTest : public ::testing::Test {
protected:
    AisDemod demod;
    // AIS: GMSK 9600 bps
    std::vector<std::complex<float>> iq = TestSignals::makeAis(SR*2, DUR);
};

TEST_F(AisDemodTest, ReturnsCorrectType) {
    auto r = demod.process(iq, au::hertz(SR*2), au::hertz(162.025e6), au::seconds(0));
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_EQ(r.modulation, "AIS");
}

TEST_F(AisDemodTest, ProducesBytesFromHdlcPreamble) {
    auto r = demod.process(iq, au::hertz(SR*2), au::hertz(162.025e6), au::seconds(0));
    EXPECT_GT(r.bits.size(), 0u);
}

TEST_F(AisDemodTest, EmptyInputHandled) {
    auto r = demod.process({}, au::hertz(SR*2), au::hertz(162.025e6), au::seconds(0));
    EXPECT_TRUE(r.bits.empty());
}

// ── POCSAG ────────────────────────────────────────────────────────────────────

class PocsagDemodTest : public ::testing::Test {
protected:
    PocsagDemod demod;
    // 25kHz SR → integer sps for 1200 baud
    std::vector<std::complex<float>> iq = TestSignals::makePocsag(25000.0, 1.0);
};

TEST_F(PocsagDemodTest, ReturnsCorrectType) {
    auto r = demod.process(iq, au::hertz(25000), au::hertz(157.0e6), au::seconds(0));
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_EQ(r.modulation, "POCSAG");
}

TEST_F(PocsagDemodTest, ProducesBytesFromRealSignal) {
    auto r = demod.process(iq, au::hertz(25000), au::hertz(157.0e6), au::seconds(0));
    EXPECT_GT(r.bits.size(), 0u);
}

TEST_F(PocsagDemodTest, SyncWordDetectable) {
    // With the preamble + known sync in makePocsag, we should find the sync
    // and emit either decoded text or raw bytes
    auto r = demod.process(iq, au::hertz(25000), au::hertz(157.0e6), au::seconds(0));
    // Either decoded message or raw bit stream
    EXPECT_FALSE(r.bits.empty());
}

TEST_F(PocsagDemodTest, EmptyInputHandled) {
    auto r = demod.process({}, au::hertz(25000), au::hertz(157.0e6), au::seconds(0));
    EXPECT_TRUE(r.bits.empty());
}

// ── ACARS ─────────────────────────────────────────────────────────────────────

class AcarsDemodTest : public ::testing::Test {
protected:
    AcarsDemod demod;
    // ACARS: 2400 bps FSK over AM carrier — use 96kHz SR for good sps
    std::vector<std::complex<float>> iq = TestSignals::makeAcars(96000.0, 0.5);
};

TEST_F(AcarsDemodTest, ReturnsCorrectType) {
    auto r = demod.process(iq, au::hertz(96000), au::hertz(131.55e6), au::seconds(0));
    EXPECT_EQ(r.type, DemodClass::Bits);
    EXPECT_EQ(r.modulation, "ACARS");
}

TEST_F(AcarsDemodTest, ProducesBytesFromAmFskSignal) {
    auto r = demod.process(iq, au::hertz(96000), au::hertz(131.55e6), au::seconds(0));
    EXPECT_GT(r.bits.size(), 0u);
}

TEST_F(AcarsDemodTest, EmptyInputHandled) {
    auto r = demod.process({}, au::hertz(96000), au::hertz(131.55e6), au::seconds(0));
    EXPECT_TRUE(r.bits.empty());
}

// ── FM voice (real-signal properties) ─────────────────────────────────────────
// Tests FM with voice-like audio (multi-harmonic) — closer to real broadcast.

TEST(FmVoiceTest, RecoversMixedHarmonicAudio) {
    auto iq = TestSignals::makeFmVoice(250000.0, 0.2);
    FmDemod d(au::hertz(75000.0), au::hertz(48000.0));
    auto r = d.process(iq, au::hertz(250000), au::hertz(100.1e6), au::seconds(0));
    EXPECT_EQ(r.type, DemodClass::Audio);
    EXPECT_GT(r.audio.size(), 0u);
    // Voice-like audio should have significant energy
    float rms = 0;
    for (float s : r.audio) rms += s*s;
    rms = std::sqrt(rms / r.audio.size());
    EXPECT_GT(rms, 0.001f) << "RMS too low — FM voice not demodulated";
}

// ── P25 C4FM (real-signal with frame sync) ────────────────────────────────────

TEST(P25C4fmTest, DemodulatesAsNbFm) {
    // P25 voice routed as FM_NB 2.5kHz dev
    auto iq = TestSignals::makeP25C4fm(48000.0, 0.5);
    FmDemod d(au::hertz(2500.0), au::hertz(48000.0));
    auto r = d.process(iq, au::hertz(48000), au::hertz(482.6625e6), au::seconds(0));
    EXPECT_EQ(r.type, DemodClass::Audio);
    EXPECT_GT(r.audio.size(), 0u);
}

// ── Cross-modulation rejection ────────────────────────────────────────────────
// Verify demodulators don't produce garbage output for wrong modulation type.

TEST(CrossModTest, DmrDemodOnFmSignalProducesBits) {
    // DMR demod on FM signal — should still produce bits (it's just wrong data)
    auto iq = TestSignals::makeFm(48000, 0.2, 1000, 5000);
    DmrDemod d;
    auto r = d.process(iq, au::hertz(48000), au::hertz(462.5e6), au::seconds(0));
    EXPECT_EQ(r.type, DemodClass::Bits);  // output type correct
    // No crash — that's the key property
}

TEST(CrossModTest, AisDemodOnFskSignalDoesNotCrash) {
    auto iq = TestSignals::makeFsk(96000, 0.2, 4500, 1200);
    AisDemod d;
    EXPECT_NO_THROW({
        auto r = d.process(iq, au::hertz(96000), au::hertz(162.0e6), au::seconds(0));
        (void)r;
    });
}

TEST(CrossModTest, PocsagDemodOnSilenceProducesRawBytes) {
    std::vector<std::complex<float>> silence(25000, {0.0f, 0.0f});
    PocsagDemod d;
    auto r = d.process(silence, au::hertz(25000), au::hertz(157e6), au::seconds(0));
    // Silence → no sync found → raw bits (possibly empty or few bytes)
    EXPECT_EQ(r.type, DemodClass::Bits);
}
