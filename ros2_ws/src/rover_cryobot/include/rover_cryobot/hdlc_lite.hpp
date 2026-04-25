#pragma once

/* HDLC-lite framing per ICD-cryobot-tether.md §3 (Q-C9).
 *
 * Frame layout on the wire:
 *   0x7E | stuffed(mode[1B] + length[2B BE] + payload[N B] + CRC-16[2B]) | 0x7E
 *
 * CRC-16/CCITT-FALSE (poly=0x1021, init=0xFFFF, no reflect, no final XOR)
 * computed over un-stuffed bytes: mode + length + payload.
 * CRC emitted MSB-first (big-endian) per Q-C8, then byte-stuffed.
 *
 * Byte stuffing inside the frame body (before flags):
 *   0x7E → {0x7D, 0x5E}    (= 0x7E XOR 0x20)
 *   0x7D → {0x7D, 0x5D}    (= 0x7D XOR 0x20)
 */

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace rover_cryobot
{

class HdlcLite
{
public:
    static constexpr uint8_t  FLAG             = 0x7EU;
    static constexpr uint8_t  ESC              = 0x7DU;
    static constexpr uint8_t  MODE_NOMINAL     = 0x00U;
    static constexpr uint8_t  MODE_COLLAPSE    = 0x01U;
    static constexpr uint8_t  MODE_SYNC        = 0x02U;
    static constexpr uint8_t  MODE_KEEPALIVE   = 0x03U;
    static constexpr uint16_t MAX_PAYLOAD_NOMINAL  = 240U;
    static constexpr uint16_t MAX_PAYLOAD_COLLAPSE = 80U;

    /* Encode payload into an HDLC-lite wire frame.
     *
     * Throws std::invalid_argument if payload exceeds the mode's limit
     * (> 240 B for NOMINAL, > 80 B for BW-COLLAPSE).
     * SYNC and KEEPALIVE frames are not size-checked here. */
    static std::vector<uint8_t> encode(
        uint8_t mode,
        const std::vector<uint8_t> & payload);

    /* Decode an HDLC-lite wire frame.
     *
     * Returns the payload on success, or std::nullopt if the frame is
     * malformed (bad flags, invalid stuffing, length mismatch, CRC error). */
    static std::optional<std::vector<uint8_t>> decode(
        const std::vector<uint8_t> & wire);

    /* CRC-16/CCITT-FALSE over the given bytes.
     * Test vector: crc16_ccitt_false({'1'..'9'}) == 0x29B1. */
    static uint16_t crc16_ccitt_false(const std::vector<uint8_t> & data);
};

}  // namespace rover_cryobot
