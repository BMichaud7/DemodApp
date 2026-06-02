/**
 * @file P25Types.hpp
 * @brief P25 APCO-25 protocol constants and data structures.
 *
 * Covers Phase 1 (C4FM) control channel only.
 * Reference: TIA-102 standard series.
 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace demod::p25 {

// P25 frame synchronisation word (48 bits)
static constexpr uint64_t FRAME_SYNC = 0x5575F5FF77FFull;

// Data Unit Identifiers (DUID) — carried in the NID field
enum class Duid : uint8_t {
    HDU  = 0x00,  ///< Header Data Unit
    TDU  = 0x03,  ///< Terminator Data Unit (no LCW)
    TDULC= 0x0F,  ///< Terminator Data Unit with Link Control Word
    TSBK = 0x0A,  ///< Trunking System Block
    LDU1 = 0x05,  ///< Logical Data Unit 1 (voice, IMBE frames 1-9)
    LDU2 = 0x0A,  ///< Logical Data Unit 2 (voice, IMBE frames 10-18) — same value as TSBK; distinguished by context
    PDU  = 0x0C,  ///< Packet Data Unit
    UNKN = 0xFF,
};

// TSBK Opcodes
enum class TsbkOpcode : uint8_t {
    GRP_V_CH_GRANT     = 0x00,  ///< Group Voice Channel Grant
    GRP_V_CH_GRANT_UPD = 0x02,  ///< Group Voice Channel Grant Update
    IND_V_CH_GRANT     = 0x04,  ///< Individual Voice Channel Grant
    UU_V_CH_GRANT      = 0x06,  ///< Unit-to-Unit Voice Channel Grant
    NET_STATUS_BCAST   = 0x3B,  ///< Network Status Broadcast
    RFSS_STATUS_BCAST  = 0x3A,  ///< RFSS Status Broadcast (channel map)
    ADJ_STS_BCAST      = 0x3C,  ///< Adjacent Site Status Broadcast
    IDEN_UP            = 0x3D,  ///< Identifier Update (band/channel info)
    UNKN               = 0xFF,
};

/// Channel identifier → frequency mapping (from IDEN_UP TSBK)
struct ChannelId {
    uint8_t  iden;          ///< Channel identifier 0–15
    uint32_t base_freq_hz;  ///< Base frequency in Hz
    uint32_t channel_spacing_hz; ///< Channel spacing in Hz
    uint32_t tx_offset_hz;  ///< TX offset from RX (typically 0 for RX only)
};

/// A channel grant decoded from a TSBK
struct ChannelGrant {
    TsbkOpcode      opcode;
    uint32_t        talk_group;   ///< Talk group ID (0 = individual call)
    uint32_t        source_id;    ///< Source unit ID
    uint8_t         channel_iden; ///< Channel identifier
    uint16_t        channel_num;  ///< Channel number within band
    double          freq_hz;      ///< Resolved frequency (0 if channel map not yet received)
    bool            encrypted;
    bool            emergency;
};

/// P25 Network/Site identifiers (from RFSS_STATUS_BCAST)
struct SiteInfo {
    uint16_t wacn;   ///< Wideband Area Communications Network ID
    uint16_t sys_id; ///< System ID
    uint8_t  rfss_id;
    uint8_t  site_id;
};

} // namespace demod::p25
