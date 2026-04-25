#include "rover_uav/flight_ctrl_node.hpp"

#include <chrono>
#include <functional>

#include "rover_common/qos.hpp"

using namespace std::chrono_literals;

namespace rover_uav
{

static constexpr auto INNER_LOOP_PERIOD = 10ms;  /* 100 Hz inner-loop */

FlightCtrlNode::FlightCtrlNode(const rclcpp::NodeOptions & options)
: rover_common::LifecycleBase("flight_ctrl_node", options)
{
    RCLCPP_INFO(get_logger(), "FlightCtrlNode created (APID 0x%03X)", APID_UAV_TM);
}

FlightCtrlNode::CallbackReturn
FlightCtrlNode::on_configure(const rclcpp_lifecycle::State & /*state*/)
{
    state_est_pub_ = create_publisher<nav_msgs::msg::Odometry>(
        "state_est", rover_common::SENSOR_QOS);

    imu_sub_ = create_subscription<sensor_msgs::msg::Imu>(
        "imu", rover_common::SENSOR_QOS,
        std::bind(&FlightCtrlNode::imu_callback, this, std::placeholders::_1));

    tm_bridge_ = std::make_unique<rover_common::TmBridge>(APID_UAV_TM);

    RCLCPP_INFO(get_logger(), "FlightCtrlNode configured");
    return CallbackReturn::SUCCESS;
}

FlightCtrlNode::CallbackReturn
FlightCtrlNode::on_activate(const rclcpp_lifecycle::State & /*state*/)
{
    state_est_pub_->on_activate();

    inner_loop_timer_ = create_wall_timer(
        INNER_LOOP_PERIOD,
        std::bind(&FlightCtrlNode::inner_loop_callback, this));

    RCLCPP_INFO(get_logger(), "FlightCtrlNode activated");
    return CallbackReturn::SUCCESS;
}

FlightCtrlNode::CallbackReturn
FlightCtrlNode::on_deactivate(const rclcpp_lifecycle::State & /*state*/)
{
    inner_loop_timer_->cancel();
    inner_loop_timer_.reset();
    state_est_pub_->on_deactivate();

    RCLCPP_INFO(get_logger(), "FlightCtrlNode deactivated");
    return CallbackReturn::SUCCESS;
}

FlightCtrlNode::CallbackReturn
FlightCtrlNode::on_cleanup(const rclcpp_lifecycle::State & /*state*/)
{
    imu_sub_.reset();
    state_est_pub_.reset();
    tm_bridge_.reset();

    RCLCPP_INFO(get_logger(), "FlightCtrlNode cleaned up");
    return CallbackReturn::SUCCESS;
}

FlightCtrlNode::CallbackReturn
FlightCtrlNode::on_shutdown(const rclcpp_lifecycle::State & /*state*/)
{
    RCLCPP_INFO(get_logger(), "FlightCtrlNode shutting down");
    return CallbackReturn::SUCCESS;
}

void FlightCtrlNode::inner_loop_callback()
{
    /* Non-blocking 100 Hz stub: pack HK and log size.  Attitude computation
     * and state_est_pub_ population are deferred to Phase 38 (Gazebo sensor
     * fusion). */
    if (!tm_bridge_) {
        return;
    }
    auto pkt = tm_bridge_->pack_hk({}, 0U, 0U);
    RCLCPP_DEBUG(get_logger(), "FlightCtrlNode HK packed: %zu bytes", pkt.size());
}

void FlightCtrlNode::imu_callback(const sensor_msgs::msg::Imu::SharedPtr /*msg*/)
{
    /* Non-blocking: store latest IMU sample for state estimation stub. */
}

}  // namespace rover_uav

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(rover_uav::FlightCtrlNode)
