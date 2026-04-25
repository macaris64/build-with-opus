#include "rover_examples/tether_pipeline_example.hpp"

#include "rover_cryobot/hdlc_lite.hpp"
#include "rover_cryobot/tether_client.hpp"

namespace rover_examples
{

std::vector<uint8_t> hk_to_wire(
    uint16_t apid,
    const std::vector<uint8_t> & hk_payload,
    uint32_t tai_coarse,
    uint32_t tai_fine)
{
    rover_cryobot::TetherClient client(apid);
    auto frame = client.pack_and_frame(
        hk_payload, tai_coarse, tai_fine,
        rover_cryobot::HdlcLite::MODE_NOMINAL);
    if (frame.empty()) {
        return {};
    }
    return frame;
}

std::vector<uint8_t> hk_to_wire_collapse(
    uint16_t apid,
    const std::vector<uint8_t> & hk_payload,
    uint32_t tai_coarse,
    uint32_t tai_fine)
{
    rover_cryobot::TetherClient client(apid);
    auto frame = client.pack_and_frame(
        hk_payload, tai_coarse, tai_fine,
        rover_cryobot::HdlcLite::MODE_COLLAPSE);
    if (frame.empty()) {
        return {};
    }
    return frame;
}

}  // namespace rover_examples
