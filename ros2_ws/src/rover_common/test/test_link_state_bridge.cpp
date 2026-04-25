#include <gtest/gtest.h>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include "rover_common/link_state_bridge.hpp"

// GIVEN  a LinkStateBridge node
// WHEN   configure → activate → deactivate → cleanup lifecycle is executed
// THEN   each transition returns SUCCESS without throwing
TEST(LinkStateBridgeTest, lifecycle_round_trip)
{
    rclcpp::init(0, nullptr);

    auto opts = rclcpp::NodeOptions().use_intra_process_comms(false);
    auto node = std::make_shared<rover_common::LinkStateBridge>(opts);

    using State = rclcpp_lifecycle::State;
    using CbReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

    EXPECT_EQ(
        node->on_configure(State()),
        CbReturn::SUCCESS);
    EXPECT_EQ(
        node->on_activate(State()),
        CbReturn::SUCCESS);
    EXPECT_EQ(
        node->on_deactivate(State()),
        CbReturn::SUCCESS);
    EXPECT_EQ(
        node->on_cleanup(State()),
        CbReturn::SUCCESS);

    rclcpp::shutdown();
}
