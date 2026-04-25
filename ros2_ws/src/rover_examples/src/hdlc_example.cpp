#include "rover_examples/hdlc_example.hpp"

#include "rover_cryobot/hdlc_lite.hpp"

namespace rover_examples
{

std::vector<uint8_t> build_nominal_frame(const std::vector<uint8_t> & payload)
{
    try {
        return rover_cryobot::HdlcLite::encode(
            rover_cryobot::HdlcLite::MODE_NOMINAL, payload);
    } catch (const std::invalid_argument &) {
        return {};
    }
}

std::vector<uint8_t> build_collapse_frame(const std::vector<uint8_t> & payload)
{
    try {
        return rover_cryobot::HdlcLite::encode(
            rover_cryobot::HdlcLite::MODE_COLLAPSE, payload);
    } catch (const std::invalid_argument &) {
        return {};
    }
}

std::vector<uint8_t> decode_frame(const std::vector<uint8_t> & wire)
{
    auto result = rover_cryobot::HdlcLite::decode(wire);
    if (result.has_value()) {
        return *result;
    }
    return {};
}

uint16_t compute_crc(const std::vector<uint8_t> & data)
{
    return rover_cryobot::HdlcLite::crc16_ccitt_false(data);
}

}  // namespace rover_examples
