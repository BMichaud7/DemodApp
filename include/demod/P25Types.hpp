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

/**
 * @brief P25 encryption algorithm identifiers (TIA-102.AABF).
 *
 * Carried in the Header Data Unit (HDU) of every encrypted voice channel.
 * The HDU is the first frame on the voice channel and contains ALGID + KID.
 */
enum class AlgId : uint8_t {
    CLEAR    = 0x00,  ///< Unencrypted — no algorithm
    DES_OFB  = 0x41,  ///< DVP (DES-OFB) — legacy Motorola
    DES_XL   = 0x42,  ///< DES-XL — legacy
    DES      = 0x01,  ///< DES (56-bit) — obsolete
    TDEA_2   = 0x02,  ///< 2-key Triple DES
    TDEA_3   = 0x03,  ///< 3-key Triple DES (112/168-bit)
    AES_256  = 0x04,  ///< AES-256 — most common modern standard
    ARC4     = 0x1F,  ///< ARC4 / RC4
    MOTOROLA = 0x80,  ///< Motorola vendor-specific
    HARRIS   = 0x21,  ///< Harris/L3 vendor-specific
    UNKN     = 0xFF,
};

/// @brief Return a human-readable name for an ALGID.
inline const char* algid_name(AlgId id) {
    switch (id) {
        case AlgId::CLEAR:    return "Clear";
        case AlgId::DES_OFB:  return "DVP (DES-OFB)";
        case AlgId::DES_XL:   return "DES-XL";
        case AlgId::DES:      return "DES";
        case AlgId::TDEA_2:   return "2-key TDEA";
        case AlgId::TDEA_3:   return "3-key TDEA";
        case AlgId::AES_256:  return "AES-256";
        case AlgId::ARC4:     return "ARC4";
        case AlgId::MOTOROLA: return "Motorola Vendor";
        case AlgId::HARRIS:   return "Harris Vendor";
        default: {
            uint8_t v = static_cast<uint8_t>(id);
            if (v >= 0x80) return "Vendor-specific";
            return "Unknown";
        }
    }
}

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
    // Encryption details — populated from voice channel HDU when followed
    AlgId           alg_id  = AlgId::UNKN;  ///< Encryption algorithm (from HDU)
    uint16_t        key_id  = 0;            ///< Key ID (from HDU, 0 if unknown)
    std::string     alg_name() const { return algid_name(alg_id); }
};

/// P25 Network/Site identifiers (from RFSS_STATUS_BCAST)
struct SiteInfo {
    uint16_t wacn;   ///< Wideband Area Communications Network ID
    uint16_t sys_id; ///< System ID
    uint8_t  rfss_id;
    uint8_t  site_id;
};

} // namespace demod::p25
