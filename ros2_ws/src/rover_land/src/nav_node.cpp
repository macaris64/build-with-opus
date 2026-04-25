#include "rover_land/nav_node.hpp"

#include <chrono>
#include <functional>

#include "rover_common/qos.hpp"

using namespace std::chrono_literals;

namespace rover_land
{

static constexpr auto HK_PERIOD = 100ms;

NavNode::NavNode(const rclcpp::NodeOptions & options)
: rover_common::LifecycleBase("nav_node", options)
{
    RCLCPP_INFO(get_logger(), "NavNode created (APID 0x%03X)", APID_LAND_TM);
}

NavNode::CallbackReturn
NavNode::on_configure(const rclcpp_lifecycle::State & /*state*/)
{
    cmd_vel_pub_ = create_publisher<geometry_msgs::msg::Twist>(
        "cmd_vel", rover_common::CMD_QOS);

    odom_sub_ = create_subscription<nav_msgs::msg::Odometry>(
        "odom", rover_common::SENSOR_QOS,
        std::bind(&NavNode::odom_callback, this, std::placeholders::_1));

    tm_bridge_ = std::make_unique<rover_common::TmBridge>(APID_LAND_TM);

    RCLCPP_INFO(get_logger(), "NavNode configured");
    return CallbackReturn::SUCCESS;
}

NavNode::CallbackReturn
NavNode::on_activate(const rclcpp_lifecycle::State & /*state*/)
{
    cmd_vel_pub_->on_activate();

    hk_timer_ = create_wall_timer(
        HK_PERIOD,
        std::bind(&NavNode::hk_timer_callback, this));

    RCLCPP_INFO(get_logger(), "NavNode activated");
    return CallbackReturn::SUCCESS;
}

NavNode::CallbackReturn
NavNode::on_deactivate(const rclcpp_lifecycle::State & /*state*/)
{
    hk_timer_->cancel();
    hk_timer_.reset();
    cmd_vel_pub_->on_deactivate();

    RCLCPP_INFO(get_logger(), "NavNode deactivated");
    return CallbackReturn::SUCCESS;
}

NavNode::CallbackReturn
NavNode::on_cleanup(const rclcpp_lifecycle::State & /*state*/)
{
    odom_sub_.reset();
    cmd_vel_pub_.reset();
    tm_bridge_.reset();

    RCLCPP_INFO(get_logger(), "NavNode cleaned up");
    return CallbackReturn::SUCCESS;
}

NavNode::CallbackReturn
NavNode::on_shutdown(const rclcpp_lifecycle::State & /*state*/)
{
    RCLCPP_INFO(get_logger(), "NavNode shutting down");
    return CallbackReturn::SUCCESS;
}

void NavNode::hk_timer_callback()
{
    if (!tm_bridge_) {
        return;
    }
    auto pkt = tm_bridge_->pack_hk({}, 0U, 0U);
    RCLCPP_DEBUG(get_logger(), "NavNode HK packed: %zu bytes", pkt.size());
}

void NavNode::odom_callback(const nav_msgs::msg::Odometry::SharedPtr /*msg*/)
{
    /* Non-blocking: store latest odometry for HK packing in future phases. */
}

}  // namespace rover_land

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(rover_land::NavNode)
