#include "rover_common/tm_bridge.hpp"

#include <cstddef>

namespace rover_common
{

/* Maximum user-data length that fits in the 16-bit CCSDS data_length field.
 *
 * data_length = 9 + payload.size()
 * max data_length = 0xFFFF → max payload = 0xFFFF - 9 = 65526 bytes
 */
static constexpr size_t MAX_PAYLOAD_BYTES = 65526U;

/* CCSDS primary header word0 base for TM (type=0, secondary-header flag=1).
 * Bits [15:13] = 000 (version), bit [12] = 0 (TM), bit [11] = 1 (sec hdr). */
static constexpr uint16_t CCSDS_TM_WORD0_BASE = 0x0800U;

/* Sequence flags: 0b11 = standalone packet (no segmentation). */
static constexpr uint16_t SEQ_FLAGS_STANDALONE = 0xC000U;

/* 14-bit sequence counter mask per CCSDS 133.0-B-2 §4.1.3.4 */
static constexpr uint16_t SEQ_COUNT_MASK = 0x3FFFU;

TmBridge::TmBridge(uint16_t apid) noexcept
: apid_(static_cast<uint16_t>(apid & 0x07FFU))
, seq_count_(0U)
{}

std::vector<uint8_t> TmBridge::pack_hk(
    const std::vector<uint8_t> & payload,
    uint32_t tai_coarse,
    uint32_t tai_fine) noexcept
{
    if (payload.size() > MAX_PAYLOAD_BYTES) {
        return {};
    }

    const uint16_t data_length =
        static_cast<uint16_t>(9U + payload.size());

    /* Primary header word 0: version|type|sec_hdr|APID */
    const uint16_t word0 =
        static_cast<uint16_t>(CCSDS_TM_WORD0_BASE | apid_);

    /* Primary header word 1: seq_flags|seq_count — capture then advance */
    const uint16_t word1 =
        static_cast<uint16_t>(SEQ_FLAGS_STANDALONE | (seq_count_ & SEQ_COUNT_MASK));
    seq_count_ =
        static_cast<uint16_t>((seq_count_ + 1U) & SEQ_COUNT_MASK);

    std::vector<uint8_t> pkt;
    pkt.reserve(16U + payload.size());

    /* ── Primary header (6 B, big-endian) ────────────────────────────────── */
    pkt.push_back(static_cast<uint8_t>(word0 >> 8U));
    pkt.push_back(static_cast<uint8_t>(word0 & 0xFFU));
    pkt.push_back(static_cast<uint8_t>(word1 >> 8U));
    pkt.push_back(static_cast<uint8_t>(word1 & 0xFFU));
    pkt.push_back(static_cast<uint8_t>(data_length >> 8U));
    pkt.push_back(static_cast<uint8_t>(data_length & 0xFFU));

    /* ── Secondary header (10 B, Q-C6): CUC 7B + func_code 2B + inst_id 1B ── */
    /* TAI coarse — 4 bytes big-endian */
    pkt.push_back(static_cast<uint8_t>(tai_coarse >> 24U));
    pkt.push_back(static_cast<uint8_t>((tai_coarse >> 16U) & 0xFFU));
    pkt.push_back(static_cast<uint8_t>((tai_coarse >> 8U) & 0xFFU));
    pkt.push_back(static_cast<uint8_t>(tai_coarse & 0xFFU));

    /* TAI fine — 3 bytes big-endian (CUC sub-second field) */
    pkt.push_back(static_cast<uint8_t>((tai_fine >> 16U) & 0xFFU));
    pkt.push_back(static_cast<uint8_t>((tai_fine >> 8U) & 0xFFU));
    pkt.push_back(static_cast<uint8_t>(tai_fine & 0xFFU));

    /* func_code (2 B BE) and instance_id (1 B): 0x0000 / 0x00 for HK path */
    pkt.push_back(0x00U);
    pkt.push_back(0x00U);
    pkt.push_back(0x00U);

    /* ── User data ────────────────────────────────────────────────────────── */
    pkt.insert(pkt.end(), payload.begin(), payload.end());

    return pkt;
}

void TmBridge::reset_seq(uint16_t to) noexcept
{
    seq_count_ = static_cast<uint16_t>(to & SEQ_COUNT_MASK);
}

}  // namespace rover_common
