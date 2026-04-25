#include <rclcpp/rclcpp.hpp>

#include "rover_examples/hdlc_example.hpp"
#include "rover_examples/tether_pipeline_example.hpp"
#include "rover_examples/tm_bridge_example.hpp"

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    auto logger = rclcpp::get_logger("rover_examples");

    /* ── HdlcLite examples ──────────────────────────────────────────────────── */
    const std::vector<uint8_t> raw_payload = {0x01, 0x7E, 0x7D, 0x02};
    const auto frame = rover_examples::build_nominal_frame(raw_payload);
    RCLCPP_INFO(logger, "HDLC nominal frame: %zu bytes", frame.size());

    const auto back = rover_examples::decode_frame(frame);
    RCLCPP_INFO(logger, "Decoded payload: %zu bytes (match=%s)",
        back.size(), back == raw_payload ? "yes" : "no");

    const uint16_t crc = rover_examples::compute_crc({'1','2','3','4','5','6','7','8','9'});
    RCLCPP_INFO(logger, "CRC-16/CCITT-FALSE KAT: 0x%04X (expect 0x29B1)", crc);

    /* ── TmBridge examples ──────────────────────────────────────────────────── */
    const auto pkt = rover_examples::pack_single_hk(0x300U, {}, 0U, 0U);
    RCLCPP_INFO(logger, "Single HK packet: %zu bytes", pkt.size());

    const std::vector<std::vector<uint8_t>> payloads = {{0x01}, {0x02}, {0x03}};
    const auto seq = rover_examples::pack_hk_sequence(0x300U, payloads, 0U, 0U);
    RCLCPP_INFO(logger, "HK sequence: %zu packets", seq.size());

    /* ── Tether pipeline examples ───────────────────────────────────────────── */
    const auto wire = rover_examples::hk_to_wire(0x400U, {}, 0U, 0U);
    RCLCPP_INFO(logger, "Tether wire frame (NOMINAL): %zu bytes", wire.size());

    const auto wire_c = rover_examples::hk_to_wire_collapse(0x400U, {}, 0U, 0U);
    RCLCPP_INFO(logger, "Tether wire frame (COLLAPSE): %zu bytes", wire_c.size());

    rclcpp::shutdown();
    return 0;
}
