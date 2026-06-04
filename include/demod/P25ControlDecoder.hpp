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
 * @file P25ControlDecoder.hpp
 * @brief P25 APCO-25 trunked control channel decoder.
 *
 * Consumes raw C4FM dibits from C4FmDemod, synchronises to P25 frames,
 * parses Trunking System Blocks (TSBKs), and emits ChannelGrant events
 * for talk groups in the configured whitelist.
 *
 * Phase 1 only.  No audio / IMBE decoding (handled by SpeechApp).
 */
#pragma once
#include "P25Types.hpp"
#include <functional>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <string>

namespace demod::p25 {

/// Called when a channel grant matching the TG whitelist is decoded.
using GrantCallback = std::function<void(const ChannelGrant&)>;

class P25ControlDecoder {
public:
    /**
     * @brief Construct decoder.
     * @param on_grant       Fired for every filtered grant.
     * @param tg_whitelist   Talk groups to watch (empty = all).
     */
    explicit P25ControlDecoder(GrantCallback on_grant,
                               std::unordered_set<uint32_t> tg_whitelist = {});

    /**
     * @brief Feed dibits from the C4FM demodulator.
     * @param dibits  Values 0–3, one per symbol.
     */
    void feed(const std::vector<uint8_t>& dibits);

    /// Current site info (populated after RFSS_STATUS_BCAST).
    SiteInfo site_info() const { return site_; }

    /// Channel identifier map (populated after IDEN_UP TSBKs).
    const std::unordered_map<uint8_t, ChannelId>& channel_map() const {
        return channel_map_;
    }

    void set_whitelist(std::unordered_set<uint32_t> tg_ids) {
        tg_whitelist_ = std::move(tg_ids);
    }

    /// Drain any pending rogue-site alert JSON (empty = no alert). Clears on read.
    std::string pop_alert() { return std::exchange(rogue_alert_json_, {}); }

private:
    // Frame synchroniser
    bool try_sync(uint64_t& shift_reg) const;

    // TSBK processing
    void process_frame(const uint8_t* bits, size_t n_bits);
    void decode_tsbk(const uint8_t* payload);
    void decode_hdu(const uint8_t* bits, size_t n_bits);  ///< Extract ALGID+KID from voice channel HDU

    // Channel frequency resolution
    double resolve_freq(uint8_t iden, uint16_t channel_num) const;

    // Golay(23,12) error correction for TSBK
    static int golay_decode(uint32_t codeword, uint32_t& data);

    GrantCallback  on_grant_;
    std::unordered_set<uint32_t>       tg_whitelist_;
    std::unordered_map<uint8_t, ChannelId> channel_map_;
    SiteInfo       site_{};
    std::string    rogue_alert_json_; ///< Set when WACN/site changes unexpectedly.

    // Bit buffer for frame sync hunting
    std::vector<uint8_t> bit_buf_;
    uint64_t             sync_reg_ = 0;
    bool                 synced_   = false;
    int                  bits_since_sync_ = 0;

    // Most-recently-decoded HDU encryption info (updated on each voice channel HDU)
    AlgId                last_hdu_alg_ = AlgId::UNKN;
    uint16_t             last_hdu_kid_ = 0;

    static constexpr int FRAME_BITS = 196;  ///< P25 TSBK frame size in bits
};

} // namespace demod::p25

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
