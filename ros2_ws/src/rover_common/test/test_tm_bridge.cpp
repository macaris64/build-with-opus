/* RED → GREEN: TmBridge unit tests.
 *
 * Verifies CCSDS TM packet encoding per Q-C6 (secondary header layout) and
 * Q-C8 (big-endian wire, no LE conversion).  Does NOT spin a DDS stack.
 */

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <lifecycle_msgs/msg/state.hpp>

#include "rover_common/tm_bridge.hpp"
#include "rover_common/lifecycle_base.hpp"

/* ── helpers ─────────────────────────────────────────────────────────────── */

static uint16_t be16(const std::vector<uint8_t> & pkt, size_t off)
{
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(pkt[off]) << 8U) | pkt[off + 1U]);
}

static uint32_t be32(const std::vector<uint8_t> & pkt, size_t off)
{
    return (static_cast<uint32_t>(pkt[off]) << 24U)
         | (static_cast<uint32_t>(pkt[off + 1U]) << 16U)
         | (static_cast<uint32_t>(pkt[off + 2U]) << 8U)
         |  static_cast<uint32_t>(pkt[off + 3U]);
}

static uint32_t be24(const std::vector<uint8_t> & pkt, size_t off)
{
    return (static_cast<uint32_t>(pkt[off]) << 16U)
         | (static_cast<uint32_t>(pkt[off + 1U]) << 8U)
         |  static_cast<uint32_t>(pkt[off + 2U]);
}

/* ── TmBridge tests ──────────────────────────────────────────────────────── */

class TmBridgeTest : public ::testing::Test
{
protected:
    /* APID 0x300 = rover_land base, per arch §4 */
    rover_common::TmBridge bridge_{0x300U};
};

/*
 * Given  TmBridge with APID 0x300
 * When   pack_hk called with a 4-byte payload
 * Then   returned packet is 20 bytes with correct primary header fields
 */
TEST_F(TmBridgeTest, pack_hk_four_byte_payload_returns_correct_size_and_header)
{
    const std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
    auto pkt = bridge_.pack_hk(payload, 0U, 0U);

    ASSERT_EQ(pkt.size(), 20U);

    /* Version (bits [15:13] of word0) must be 0b000 */
    EXPECT_EQ(pkt[0] >> 5U, 0x00U);

    /* Type bit (bit 12) must be 0 (TM) — mask 0x10 in byte[0] */
    EXPECT_EQ(pkt[0] & 0x10U, 0x00U);

    /* Secondary-header flag (bit 11) must be 1 — mask 0x08 in byte[0] */
    EXPECT_NE(pkt[0] & 0x08U, 0x00U);

    /* APID field extracted from word0 must equal 0x300 */
    const uint16_t apid_decoded =
        static_cast<uint16_t>((static_cast<uint16_t>(pkt[0] & 0x07U) << 8U) | pkt[1]);
    EXPECT_EQ(apid_decoded, 0x300U);

    /* data_length = 9 + payload.size() = 13 = 0x000D */
    EXPECT_EQ(be16(pkt, 4U), 0x000DU);

    /* Sequence flags (top 2 bits of word1) must be 0b11 */
    EXPECT_EQ((pkt[2] >> 6U) & 0x03U, 0x03U);

    /* First call: sequence count must be 0 */
    const uint16_t seq = static_cast<uint16_t>(
        (static_cast<uint16_t>(pkt[2] & 0x3FU) << 8U) | pkt[3]);
    EXPECT_EQ(seq, 0x0000U);

    /* User data bytes must be preserved verbatim */
    EXPECT_EQ(pkt[16], 0x01U);
    EXPECT_EQ(pkt[17], 0x02U);
    EXPECT_EQ(pkt[18], 0x03U);
    EXPECT_EQ(pkt[19], 0x04U);
}

/*
 * Given  TmBridge with empty payload
 * When   pack_hk called
 * Then   packet is 16 bytes and data_length == 9
 */
TEST_F(TmBridgeTest, pack_hk_empty_payload_returns_16_bytes)
{
    auto pkt = bridge_.pack_hk({}, 0U, 0U);

    ASSERT_EQ(pkt.size(), 16U);
    EXPECT_EQ(be16(pkt, 4U), 0x0009U);  /* data_length = 9 + 0 = 9 */
}

/*
 * Given  TmBridge
 * When   pack_hk called with TAI coarse=0x01020304 and fine=0x050607
 * Then   secondary header encodes timestamps big-endian
 */
TEST_F(TmBridgeTest, pack_hk_encodes_tai_big_endian)
{
    auto pkt = bridge_.pack_hk({}, 0x01020304U, 0x050607U);

    ASSERT_GE(pkt.size(), 16U);

    /* Bytes 6–9: TAI coarse (4B BE) */
    EXPECT_EQ(be32(pkt, 6U), 0x01020304U);

    /* Bytes 10–12: TAI fine (3B BE) */
    EXPECT_EQ(be24(pkt, 10U), 0x050607U);

    /* Bytes 13–14: func_code = 0x0000 (HK default) */
    EXPECT_EQ(be16(pkt, 13U), 0x0000U);

    /* Byte 15: instance_id = 0x00 (HK default) */
    EXPECT_EQ(pkt[15], 0x00U);
}

/*
 * Given  TmBridge
 * When   pack_hk called twice in succession
 * Then   sequence counter increments
 */
TEST_F(TmBridgeTest, pack_hk_increments_sequence_counter)
{
    auto pkt0 = bridge_.pack_hk({}, 0U, 0U);
    auto pkt1 = bridge_.pack_hk({}, 0U, 0U);

    const uint16_t seq0 = static_cast<uint16_t>(
        (static_cast<uint16_t>(pkt0[2] & 0x3FU) << 8U) | pkt0[3]);
    const uint16_t seq1 = static_cast<uint16_t>(
        (static_cast<uint16_t>(pkt1[2] & 0x3FU) << 8U) | pkt1[3]);

    EXPECT_EQ(seq0, 0x0000U);
    EXPECT_EQ(seq1, 0x0001U);
}

/*
 * Given  TmBridge with sequence counter at 0x3FFF
 * When   pack_hk called
 * Then   next packet carries seq 0 (14-bit CCSDS wrap)
 */
TEST_F(TmBridgeTest, pack_hk_sequence_counter_wraps_at_14_bits)
{
    bridge_.reset_seq(0x3FFFU);

    auto pkt_last = bridge_.pack_hk({}, 0U, 0U);
    auto pkt_wrap = bridge_.pack_hk({}, 0U, 0U);

    const uint16_t seq_last = static_cast<uint16_t>(
        (static_cast<uint16_t>(pkt_last[2] & 0x3FU) << 8U) | pkt_last[3]);
    const uint16_t seq_wrap = static_cast<uint16_t>(
        (static_cast<uint16_t>(pkt_wrap[2] & 0x3FU) << 8U) | pkt_wrap[3]);

    EXPECT_EQ(seq_last, 0x3FFFU);
    EXPECT_EQ(seq_wrap, 0x0000U);
}

/*
 * Given  TmBridge with APID at 11-bit maximum (0x7FF)
 * When   pack_hk called
 * Then   all 11 APID bits are preserved in the packet
 */
TEST_F(TmBridgeTest, pack_hk_preserves_max_apid_boundary)
{
    rover_common::TmBridge max_bridge(0x7FFU);
    auto pkt = max_bridge.pack_hk({}, 0U, 0U);

    ASSERT_GE(pkt.size(), 6U);

    const uint16_t apid_decoded =
        static_cast<uint16_t>((static_cast<uint16_t>(pkt[0] & 0x07U) << 8U) | pkt[1]);
    EXPECT_EQ(apid_decoded, 0x07FFU);
}

/*
 * Given  TmBridge
 * When   reset_seq called and then pack_hk called
 * Then   sequence counter restarts from 0
 */
TEST_F(TmBridgeTest, reset_seq_restarts_counter)
{
    /* Advance counter first */
    bridge_.pack_hk({}, 0U, 0U);
    bridge_.pack_hk({}, 0U, 0U);

    bridge_.reset_seq();

    auto pkt = bridge_.pack_hk({}, 0U, 0U);
    const uint16_t seq = static_cast<uint16_t>(
        (static_cast<uint16_t>(pkt[2] & 0x3FU) << 8U) | pkt[3]);
    EXPECT_EQ(seq, 0x0000U);
}

/*
 * Given  TmBridge
 * When   pack_hk called with oversized payload (> 65526 bytes)
 * Then   returns empty vector (failure path — caller must chunk)
 */
TEST_F(TmBridgeTest, pack_hk_rejects_oversized_payload)
{
    /* 65527 bytes exceeds the 16-bit data_length field capacity */
    const std::vector<uint8_t> oversized(65527U, 0x00U);
    auto result = bridge_.pack_hk(oversized, 0U, 0U);

    EXPECT_TRUE(result.empty());
}

/* ── LifecycleBase tests ─────────────────────────────────────────────────── */

/* Minimal concrete subclass used to test LifecycleBase defaults */
class MinimalNode : public rover_common::LifecycleBase
{
public:
    MinimalNode() : rover_common::LifecycleBase("minimal_test_node") {}
};

class LifecycleBaseTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        node_ = std::make_shared<MinimalNode>();
    }
    void TearDown() override
    {
        node_.reset();
    }
    std::shared_ptr<MinimalNode> node_;
};

/*
 * Given  a LifecycleBase node
 * When   configure → activate → deactivate → cleanup round trip executed
 * Then   all transitions return SUCCESS
 */
TEST_F(LifecycleBaseTest, lifecycle_round_trip_returns_success)
{
    using rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface;

    auto unconfigured = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_UNCONFIGURED, "unconfigured");
    auto inactive = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE, "inactive");
    auto active = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE, "active");

    EXPECT_EQ(node_->on_configure(unconfigured),
        LifecycleNodeInterface::CallbackReturn::SUCCESS);
    EXPECT_EQ(node_->on_activate(inactive),
        LifecycleNodeInterface::CallbackReturn::SUCCESS);
    EXPECT_EQ(node_->on_deactivate(active),
        LifecycleNodeInterface::CallbackReturn::SUCCESS);
    EXPECT_EQ(node_->on_cleanup(inactive),
        LifecycleNodeInterface::CallbackReturn::SUCCESS);
}

/*
 * Given  a LifecycleBase node
 * When   on_shutdown called
 * Then   returns SUCCESS without throwing
 */
TEST_F(LifecycleBaseTest, on_shutdown_returns_success)
{
    using rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface;

    auto active = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE, "active");

    EXPECT_EQ(node_->on_shutdown(active),
        LifecycleNodeInterface::CallbackReturn::SUCCESS);
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return result;
}
