#include "rover_cryobot/tether_client.hpp"

namespace rover_cryobot
{

TetherClient::TetherClient(uint16_t apid)
: tm_bridge_(apid)
{
}

std::vector<uint8_t> TetherClient::pack_and_frame(
    const std::vector<uint8_t> & hk_payload,
    uint32_t tai_coarse,
    uint32_t tai_fine,
    uint8_t mode)
{
    auto spp = tm_bridge_.pack_hk(hk_payload, tai_coarse, tai_fine);
    if (spp.empty()) {
        return {};
    }

    try {
        return HdlcLite::encode(mode, spp);
    } catch (const std::invalid_argument &) {
        /* SPP exceeds the tether frame budget — caller must chunk. */
        return {};
    }
}

}  // namespace rover_cryobot
