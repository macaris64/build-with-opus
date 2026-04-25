/* RED → GREEN: rover_uav FlightCtrlNode lifecycle tests.
 *
 * Tests the configure → activate → deactivate → cleanup round trip and
 * verifies the TmBridge is initialised with the correct APID (0x3C0).
 * No DDS stack is spun — callbacks are invoked directly.
 */

#include <gtest/gtest.h>
#include <lifecycle_msgs/msg/state.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include "rover_common/tm_bridge.hpp"
#include "rover_uav/flight_ctrl_node.hpp"

using CB = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

class FlightCtrlNodeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        node_ = std::make_shared<rover_uav::FlightCtrlNode>();
    }
    void TearDown() override
    {
        node_.reset();
    }
    std::shared_ptr<rover_uav::FlightCtrlNode> node_;
};

/*
 * Given  FlightCtrlNode in UNCONFIGURED state
 * When   configure → activate → deactivate → cleanup round trip
 * Then   every transition returns CallbackReturn::SUCCESS
 */
TEST_F(FlightCtrlNodeTest, lifecycle_round_trip_returns_success)
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
 * Given  FlightCtrlNode
 * When   on_shutdown called
 * Then   returns SUCCESS
 */
TEST_F(FlightCtrlNodeTest, on_shutdown_returns_success)
{
    auto active = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE, "active");
    EXPECT_EQ(node_->on_shutdown(active), CB::SUCCESS);
}

/*
 * Given  TmBridge constructed with APID 0x3C0 (rover_uav block)
 * When   pack_hk called with empty payload
 * Then   packet is 16 bytes and APID bits [10:0] == 0x3C0
 */
TEST(TmBridgeApidTest, rover_uav_apid_encoded_correctly)
{
    rover_common::TmBridge bridge(rover_uav::APID_UAV_TM);
    auto pkt = bridge.pack_hk({}, 0U, 0U);

    ASSERT_EQ(pkt.size(), 16U);

    const uint16_t apid_decoded =
        static_cast<uint16_t>((static_cast<uint16_t>(pkt[0] & 0x07U) << 8U) | pkt[1]);
    EXPECT_EQ(apid_decoded, 0x3C0U);
}

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return result;
}
