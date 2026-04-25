#include <gtest/gtest.h>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include "fleet_monitor/fleet_monitor_node.hpp"

using CbReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;
using State    = rclcpp_lifecycle::State;

class FleetMonitorTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()    { rclcpp::init(0, nullptr); }
    static void TearDownTestSuite() { rclcpp::shutdown(); }
};

// GIVEN  a FleetMonitorNode
// WHEN   configure → activate → deactivate → cleanup lifecycle is executed
// THEN   each transition returns SUCCESS without throwing
TEST_F(FleetMonitorTest, lifecycle_round_trip)
{
    auto opts = rclcpp::NodeOptions().use_intra_process_comms(false);
    auto node = std::make_shared<fleet_monitor::FleetMonitorNode>(opts);

    EXPECT_EQ(node->on_configure(State()),  CbReturn::SUCCESS);
    EXPECT_EQ(node->on_activate(State()),   CbReturn::SUCCESS);
    EXPECT_EQ(node->on_deactivate(State()), CbReturn::SUCCESS);
    EXPECT_EQ(node->on_cleanup(State()),    CbReturn::SUCCESS);
}

// GIVEN  a freshly constructed FleetMonitorNode
// WHEN   heartbeats() is read before any callback fires
// THEN   all three entries report ever_seen = false (no heartbeat has arrived yet)
TEST_F(FleetMonitorTest, heartbeats_initially_unseen)
{
    auto opts = rclcpp::NodeOptions().use_intra_process_comms(false);
    auto node = std::make_shared<fleet_monitor::FleetMonitorNode>(opts);

    const auto & hb = node->heartbeats();
    for (std::size_t i = 0; i < hb.size(); ++i) {
        EXPECT_FALSE(hb[i].ever_seen) << "heartbeat index " << i << " should be unseen";
    }
}

// GIVEN  a FleetMonitorNode with heartbeats array of size 3
// WHEN   heartbeats() is read
// THEN   the array has exactly three entries (land=0, uav=1, cryo=2)
TEST_F(FleetMonitorTest, heartbeats_array_size_is_three)
{
    auto opts = rclcpp::NodeOptions().use_intra_process_comms(false);
    auto node = std::make_shared<fleet_monitor::FleetMonitorNode>(opts);

    EXPECT_EQ(node->heartbeats().size(), 3U);
}
