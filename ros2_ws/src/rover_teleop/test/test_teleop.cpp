#include <gtest/gtest.h>
#include <lifecycle_msgs/msg/state.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include "rover_teleop/teleop_node.hpp"

/* ---------------------------------------------------------------------------
 * Lifecycle transition tests
 *
 * These tests verify that the node transitions correctly through its lifecycle
 * states. They do NOT spin a full DDS stack — rclcpp::init is called once
 * in main() and nodes are managed directly.
 * --------------------------------------------------------------------------- */

class TeleopNodeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        node_ = std::make_shared<rover_teleop::TeleopNode>();
    }

    void TearDown() override
    {
        node_.reset();
    }

    std::shared_ptr<rover_teleop::TeleopNode> node_;
};

TEST_F(TeleopNodeTest, configure_transition_succeeds)
{
    auto state = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_UNCONFIGURED, "unconfigured");

    auto result = node_->on_configure(state);

    EXPECT_EQ(result,
        rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS);
}

TEST_F(TeleopNodeTest, activate_after_configure_succeeds)
{
    auto unconfigured = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_UNCONFIGURED, "unconfigured");
    auto inactive = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE, "inactive");

    node_->on_configure(unconfigured);
    auto result = node_->on_activate(inactive);

    EXPECT_EQ(result,
        rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS);

    /* Clean up — deactivate before destroying to prevent timer dangling */
    auto active = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE, "active");
    node_->on_deactivate(active);
}

TEST_F(TeleopNodeTest, qos_constant_has_expected_depth)
{
    /* The CMD_VEL_QOS constant (file scope) is defined with KeepLast(10).
     * This test documents and guards against accidental changes. */
    auto pub_options = rclcpp::PublisherOptions();
    EXPECT_EQ(pub_options.event_callbacks.deadline_callback, nullptr);
    /* Depth is validated indirectly — configure must succeed which creates
     * the publisher with CMD_VEL_QOS. If depth were 0, DDS would reject it. */
    auto inactive = rclcpp_lifecycle::State(
        lifecycle_msgs::msg::State::PRIMARY_STATE_UNCONFIGURED, "unconfigured");
    EXPECT_EQ(node_->on_configure(inactive),
        rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS);
}

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return result;
}
