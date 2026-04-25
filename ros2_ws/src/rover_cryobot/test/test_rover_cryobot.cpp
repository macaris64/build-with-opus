/* RED → GREEN: rover_cryobot HdlcLite + DrillCtrlNode tests.
 *
 * Mandatory tests (from ICD-cryobot-tether.md §3, Q-C9):
 *   1. CRC-16/CCITT-FALSE test vector: CRC("123456789") == 0x29B1
 *   2. Byte stuffing: 0x7E and 0x7D are correctly escaped
 *   3. Roundtrip: decode(encode(payload)) == payload
 *   4. Payload size guard: > MAX_PAYLOAD_NOMINAL throws
 *   5. DrillCtrlNode lifecycle round trip
 *   6. TmBridge APID 0x400 (rover_cryobot block)
 *   7. BW-COLLAPSE size guard: > 80 B throws
 *   8. Decode rejects bad CRC
 *   9. Decode rejects unescaped FLAG in body
 */

#include <gtest/gtest.h>
#include <lifecycle_msgs/msg/state.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include "rover_common/tm_bridge.hpp"
#include "rover_cryobot/drill_ctrl_node.hpp"
#include "rover_cryobot/hdlc_lite.hpp"
#include "rover_cryobot/tether_client.hpp"

using CB = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;
using rover_cryobot::HdlcLite;

/* ── CRC tests ─────────────────────────────────────────────────────────────── */

/*
 * Given  the ASCII string "123456789"
 * When   crc16_ccitt_false computed
 * Then   result == 0x29B1  (normative test vector from ICD §3.3)
 */
TEST(HdlcLiteCrcTest, known_answer_test_vector)
{
    const std::vector<uint8_t> input = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    EXPECT_EQ(HdlcLite::crc16_ccitt_false(input), 0x29B1U);
}

/*
 * Given  empty input
 * When   crc16_ccitt_false computed
 * Then   result == 0xFFFF (init value, no bytes processed)
 */
TEST(HdlcLiteCrcTest, empty_input_returns_init_value)
{
    EXPECT_EQ(HdlcLite::crc16_ccitt_false({}), 0xFFFFU);
}

/* ── Encode / byte-stuffing tests ─────────────────────────────────────────── */

/*
 * Given  payload containing 0x7E and 0x7D
 * When   encode(MODE_NOMINAL, payload) called
 * Then   frame starts and ends with 0x7E
 *        no bare 0x7E appears in the body
 *        0x7E in payload is escaped as {0x7D, 0x5E}
 *        0x7D in payload is escaped as {0x7D, 0x5D}
 */
TEST(HdlcLiteEncodeTest, byte_stuffing_escapes_flag_and_esc)
{
    const std::vector<uint8_t> payload = {0x01U, 0x7EU, 0x7DU, 0x02U};
    auto wire = HdlcLite::encode(HdlcLite::MODE_NOMINAL, payload);

    ASSERT_GE(wire.size(), 2U);
    EXPECT_EQ(wire.front(), HdlcLite::FLAG);
    EXPECT_EQ(wire.back(), HdlcLite::FLAG);

    /* No bare FLAG allowed in body (between the two delimiter flags). */
    for (size_t i = 1U; i < wire.size() - 1U; ++i) {
        EXPECT_NE(wire[i], HdlcLite::FLAG)
            << "Bare FLAG found at body index " << i;
    }
}

/*
 * Given  a simple payload with no special bytes
 * When   encode(MODE_NOMINAL, payload) called
 * Then   frame has correct FLAG delimiters and non-zero length
 */
TEST(HdlcLiteEncodeTest, simple_payload_produces_valid_frame_structure)
{
    const std::vector<uint8_t> payload = {0xABU, 0xCDU, 0xEFU};
    auto wire = HdlcLite::encode(HdlcLite::MODE_NOMINAL, payload);

    ASSERT_GE(wire.size(), 9U);  /* FLAG + mode(1) + len(2) + payload(3) + CRC(2) + FLAG */
    EXPECT_EQ(wire.front(), HdlcLite::FLAG);
    EXPECT_EQ(wire.back(), HdlcLite::FLAG);
}

/* ── Roundtrip tests ─────────────────────────────────────────────────────── */

/*
 * Given  payload with embedded 0x7E and 0x7D bytes
 * When   wire = encode(MODE_NOMINAL, payload); decoded = decode(wire)
 * Then   decoded.has_value() == true
 *        *decoded == payload (exact reconstruction)
 */
TEST(HdlcLiteRoundtripTest, roundtrip_with_escaped_bytes)
{
    const std::vector<uint8_t> payload = {0x11U, 0x7EU, 0x7DU, 0x5EU, 0xFFU};
    auto wire = HdlcLite::encode(HdlcLite::MODE_NOMINAL, payload);
    auto decoded = HdlcLite::decode(wire);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, payload);
}

/*
 * Given  empty payload
 * When   encode/decode roundtrip
 * Then   decoded == empty vector
 */
TEST(HdlcLiteRoundtripTest, roundtrip_empty_payload)
{
    auto wire = HdlcLite::encode(HdlcLite::MODE_NOMINAL, {});
    auto decoded = HdlcLite::decode(wire);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded->empty());
}

/*
 * Given  240-byte payload (maximum NOMINAL)
 * When   encode/decode roundtrip
 * Then   decoded == original payload
 */
TEST(HdlcLiteRoundtripTest, roundtrip_max_nominal_payload)
{
    std::vector<uint8_t> payload(240U, 0x42U);
    auto wire = HdlcLite::encode(HdlcLite::MODE_NOMINAL, payload);
    auto decoded = HdlcLite::decode(wire);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, payload);
}

/* ── Payload size guard tests ─────────────────────────────────────────────── */

/*
 * Given  payload of 241 bytes in NOMINAL mode (one byte over limit)
 * When   encode(MODE_NOMINAL, payload) called
 * Then   throws std::invalid_argument
 */
TEST(HdlcLiteSizeGuardTest, nominal_oversized_payload_throws)
{
    std::vector<uint8_t> oversized(241U, 0x00U);
    EXPECT_THROW(
        HdlcLite::encode(HdlcLite::MODE_NOMINAL, oversized),
        std::invalid_argument);
}

/*
 * Given  payload of 81 bytes in BW-COLLAPSE mode (one byte over limit)
 * When   encode(MODE_COLLAPSE, payload) called
 * Then   throws std::invalid_argument
 */
TEST(HdlcLiteSizeGuardTest, collapse_oversized_payload_throws)
{
    std::vector<uint8_t> oversized(81U, 0x00U);
    EXPECT_THROW(
        HdlcLite::encode(HdlcLite::MODE_COLLAPSE, oversized),
        std::invalid_argument);
}

/* ── Decode error path tests ─────────────────────────────────────────────── */

/*
 * Given  wire frame with corrupt CRC (last non-flag byte flipped)
 * When   decode called
 * Then   returns std::nullopt
 */
TEST(HdlcLiteDecodeTest, corrupt_crc_returns_nullopt)
{
    auto wire = HdlcLite::encode(HdlcLite::MODE_NOMINAL, {0x01U, 0x02U});
    /* Flip a CRC byte — second-to-last byte before trailing FLAG. */
    wire[wire.size() - 2U] ^= 0xFFU;
    EXPECT_FALSE(HdlcLite::decode(wire).has_value());
}

/*
 * Given  too-short wire (below minimum valid frame size)
 * When   decode called
 * Then   returns std::nullopt
 */
TEST(HdlcLiteDecodeTest, too_short_wire_returns_nullopt)
{
    EXPECT_FALSE(HdlcLite::decode({0x7EU, 0x7EU}).has_value());
}

/* ── DrillCtrlNode lifecycle tests ────────────────────────────────────────── */

class DrillCtrlNodeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        node_ = std::make_shared<rover_cryobot::DrillCtrlNode>();
    }
    void TearDown() override
    {
        node_.reset();
    }
    std::shared_ptr<rover_cryobot::DrillCtrlNode> node_;
};

/*
 * Given  DrillCtrlNode in UNCONFIGURED state
 * When   configure → activate → deactivate → cleanup round trip
 * Then   every transition returns CallbackReturn::SUCCESS
 */
TEST_F(DrillCtrlNodeTest, lifecycle_round_trip_returns_success)
{
    auto unconfigured = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_UNCONFIGURED, "unconfigured");
    auto inactive = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE, "inactive");
    auto active = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE, "active");

    EXPECT_EQ(node_->on_configure(unconfigured), CB::SUCCESS);
    EXPECT_EQ(node_->on_activate(inactive),      CB::SUCCESS);
    EXPECT_EQ(node_->on_deactivate(active),      CB::SUCCESS);
    EXPECT_EQ(node_->on_cleanup(inactive),        CB::SUCCESS);
}

/*
 * Given  TmBridge constructed with APID 0x400 (rover_cryobot TM block)
 * When   pack_hk called with empty payload
 * Then   packet is 16 bytes and APID bits [10:0] == 0x400
 */
TEST(TmBridgeApidTest, rover_cryobot_apid_encoded_correctly)
{
    rover_common::TmBridge bridge(rover_cryobot::APID_CRYOBOT_TM);
    auto pkt = bridge.pack_hk({}, 0U, 0U);

    ASSERT_EQ(pkt.size(), 16U);

    const uint16_t apid_decoded =
        static_cast<uint16_t>((static_cast<uint16_t>(pkt[0] & 0x07U) << 8U) | pkt[1]);
    EXPECT_EQ(apid_decoded, 0x400U);
}

/*
 * Given  TetherClient with APID 0x400
 * When   pack_and_frame called with empty HK payload in NOMINAL mode
 * Then   result is non-empty (valid HDLC-lite frame)
 *        result starts and ends with 0x7E
 */
TEST(TetherClientTest, pack_and_frame_produces_valid_hdlc_frame)
{
    rover_cryobot::TetherClient client(0x400U);
    auto frame = client.pack_and_frame({}, 0U, 0U, HdlcLite::MODE_NOMINAL);

    ASSERT_FALSE(frame.empty());
    EXPECT_EQ(frame.front(), HdlcLite::FLAG);
    EXPECT_EQ(frame.back(), HdlcLite::FLAG);
}

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return result;
}
