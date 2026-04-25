#pragma once

/* Demonstrative wrappers around rover_common::TmBridge.
 *
 * Shows how to pack single HK packets and multi-packet sequences using a
 * shared TmBridge instance (sequence counter advances across calls). */

#include <cstdint>
#include <vector>

namespace rover_examples
{

/* Pack a single HK payload into a CCSDS TM Space Packet.
 * Returns empty if payload exceeds 65 526 B (TmBridge hard limit). */
std::vector<uint8_t> pack_single_hk(
    uint16_t apid,
    const std::vector<uint8_t> & payload,
    uint32_t tai_coarse,
    uint32_t tai_fine);

/* Pack each payload in sequence using one TmBridge instance (sequence
 * counter advances per packet).  Entries that produce an empty packet
 * (oversized payload) are silently dropped from the output. */
std::vector<std::vector<uint8_t>> pack_hk_sequence(
    uint16_t apid,
    const std::vector<std::vector<uint8_t>> & payloads,
    uint32_t tai_coarse,
    uint32_t tai_fine);

}  // namespace rover_examples
