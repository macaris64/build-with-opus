#pragma once

/* TetherClient — packs HK data into a CCSDS SPP then wraps it in an
 * HDLC-lite frame ready for tether transmission (ICD-cryobot-tether.md §3).
 *
 * In Phase 37 no physical socket is opened; the encoded frame is returned to
 * the caller for logging / testing.  The TCP transport layer is wired in
 * Phase 40 (SITL integration).
 */

#include <cstdint>
#include <vector>

#include "rover_common/tm_bridge.hpp"
#include "rover_cryobot/hdlc_lite.hpp"

namespace rover_cryobot
{

class TetherClient
{
public:
    explicit TetherClient(uint16_t apid);

    /* Pack HK payload into a CCSDS SPP, then wrap in an HDLC-lite frame.
     *
     * Returns the wire frame bytes, or an empty vector if the SPP cannot be
     * built (oversized payload) or the HDLC frame would exceed mode limits. */
    std::vector<uint8_t> pack_and_frame(
        const std::vector<uint8_t> & hk_payload,
        uint32_t tai_coarse,
        uint32_t tai_fine,
        uint8_t mode = HdlcLite::MODE_NOMINAL);

private:
    rover_common::TmBridge tm_bridge_;
};

}  // namespace rover_cryobot
