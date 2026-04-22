#include "rover_teleop/teleop_node.hpp"

#include <chrono>
#include <functional>

using namespace std::chrono_literals;

namespace rover_teleop
{

/* QoS profile for command-velocity messages — defined as a named constant
 * rather than an inline literal so the profile is easy to audit and change. */
static const rclcpp::QoS CMD_VEL_QOS = rclcpp::QoS(rclcpp::KeepLast(10));

static constexpr auto TIMER_PERIOD = 100ms;  /* 10 Hz publish rate */

TeleopNode::TeleopNode(const rclcpp::NodeOptions & options)
: rclcpp_lifecycle::LifecycleNode("teleop_node", options)
{
    RCLCPP_INFO(get_logger(), "TeleopNode created");
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
TeleopNode::on_configure(const rclcpp_lifecycle::State & /*state*/)
{
    cmd_vel_pub_ = create_lifecycle_publisher<geometry_msgs::msg::Twist>(
        "cmd_vel", CMD_VEL_QOS);

    RCLCPP_INFO(get_logger(), "TeleopNode configured");
    return CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
TeleopNode::on_activate(const rclcpp_lifecycle::State & /*state*/)
{
    cmd_vel_pub_->on_activate();

    timer_ = create_wall_timer(TIMER_PERIOD, std::bind(&TeleopNode::timer_callback, this));

    RCLCPP_INFO(get_logger(), "TeleopNode activated");
    return CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
TeleopNode::on_deactivate(const rclcpp_lifecycle::State & /*state*/)
{
    timer_->cancel();
    timer_.reset();
    cmd_vel_pub_->on_deactivate();

    RCLCPP_INFO(get_logger(), "TeleopNode deactivated");
    return CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
TeleopNode::on_cleanup(const rclcpp_lifecycle::State & /*state*/)
{
    cmd_vel_pub_.reset();

    RCLCPP_INFO(get_logger(), "TeleopNode cleaned up");
    return CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
TeleopNode::on_shutdown(const rclcpp_lifecycle::State & /*state*/)
{
    RCLCPP_INFO(get_logger(), "TeleopNode shutting down");
    return CallbackReturn::SUCCESS;
}

void TeleopNode::timer_callback()
{
    /* Callback must not block — zero-duration publish only */
    auto msg = geometry_msgs::msg::Twist();
    msg.linear.x  = 0.0;
    msg.angular.z = 0.0;
    cmd_vel_pub_->publish(msg);
}

}  // namespace rover_teleop

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(rover_teleop::TeleopNode)
