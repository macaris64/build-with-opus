#pragma once

/* Demonstrative wrappers around rover_cryobot::TetherClient.
 *
 * Shows the full HK → CCSDS SPP → HDLC-lite wire-frame pipeline used by
 * DrillCtrlNode to transmit telemetry over the cryobot tether. */

#include <cstdint>
#include <vector>

namespace rover_examples
{

/* Pack HK payload → CCSDS SPP → HDLC NOMINAL wire frame.
 * Returns empty if the pipeline fails (SPP oversize or HDLC limit exceeded). */
std::vector<uint8_t> hk_to_wire(
    uint16_t apid,
    const std::vector<uint8_t> & hk_payload,
    uint32_t tai_coarse,
    uint32_t tai_fine);

/* Pack HK payload → CCSDS SPP → HDLC BW-COLLAPSE wire frame.
 * Returns empty if the pipeline fails (SPP oversize or HDLC limit exceeded). */
std::vector<uint8_t> hk_to_wire_collapse(
    uint16_t apid,
    const std::vector<uint8_t> & hk_payload,
    uint32_t tai_coarse,
    uint32_t tai_fine);

}  // namespace rover_examples
