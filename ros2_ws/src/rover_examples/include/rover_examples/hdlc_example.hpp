#pragma once

/* Demonstrative wrappers around rover_cryobot::HdlcLite.
 *
 * Each function catches exceptions or handles optional returns so callers get
 * a simple vector<uint8_t> result: non-empty on success, empty on error. */

#include <cstdint>
#include <vector>

namespace rover_examples
{

/* Build a NOMINAL HDLC-lite wire frame.
 * Returns empty if payload exceeds MAX_PAYLOAD_NOMINAL (240 B). */
std::vector<uint8_t> build_nominal_frame(const std::vector<uint8_t> & payload);

/* Build a BW-COLLAPSE HDLC-lite wire frame.
 * Returns empty if payload exceeds MAX_PAYLOAD_COLLAPSE (80 B). */
std::vector<uint8_t> build_collapse_frame(const std::vector<uint8_t> & payload);

/* Decode an HDLC-lite wire frame produced by encode().
 * Returns the original payload on success, or empty on CRC/framing error. */
std::vector<uint8_t> decode_frame(const std::vector<uint8_t> & wire);

/* CRC-16/CCITT-FALSE (poly=0x1021, init=0xFFFF, no reflect).
 * KAT: compute_crc({'1'..'9'}) == 0x29B1. */
uint16_t compute_crc(const std::vector<uint8_t> & data);

}  // namespace rover_examples
