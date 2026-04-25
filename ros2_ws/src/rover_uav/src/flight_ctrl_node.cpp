#include "rover_uav/flight_ctrl_node.hpp"

#include <chrono>
#include <cstring>
#include <functional>

#include "rover_common/qos.hpp"

using namespace std::chrono_literals;

namespace rover_uav
{

static constexpr auto INNER_LOOP_PERIOD = 10ms;   /* 100 Hz inner-loop */
static constexpr auto HK_PERIOD         = 1000ms; /* 1 Hz HK cadence */

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
    udp_gw_    = std::make_unique<rover_common::UdpGateway>("127.0.0.1", 5800U);

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

    hk_timer_ = create_wall_timer(
        HK_PERIOD,
        std::bind(&FlightCtrlNode::hk_timer_callback, this));

    RCLCPP_INFO(get_logger(), "FlightCtrlNode activated");
    return CallbackReturn::SUCCESS;
}

FlightCtrlNode::CallbackReturn
FlightCtrlNode::on_deactivate(const rclcpp_lifecycle::State & /*state*/)
{
    inner_loop_timer_->cancel();
    inner_loop_timer_.reset();
    hk_timer_->cancel();
    hk_timer_.reset();
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
    udp_gw_.reset();
    latest_imu_.reset();

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
    /* Non-blocking 100 Hz stub: publish state_est so fleet_monitor
     * receives a DDS heartbeat.  Full attitude computation deferred to
     * Phase 38 (Gazebo sensor fusion). */
    if (!state_est_pub_ || !state_est_pub_->is_activated()) {
        return;
    }
    nav_msgs::msg::Odometry odom;
    odom.header.stamp    = this->now();
    odom.header.frame_id = "odom";
    if (latest_imu_) {
        odom.pose.pose.orientation = latest_imu_->orientation;
    }
    state_est_pub_->publish(odom);
}

void FlightCtrlNode::hk_timer_callback()
{
    /* 1 Hz HK pack-and-forward to cFS ros2_bridge via UDP:5800. */
    if (!tm_bridge_ || !udp_gw_) {
        return;
    }

    /* 32-byte payload: linear accel xyz (3 × float32) from latest IMU + reserved. */
    std::vector<uint8_t> payload(32U, 0U);
    if (latest_imu_) {
        const float ax = static_cast<float>(latest_imu_->linear_acceleration.x);
        const float ay = static_cast<float>(latest_imu_->linear_acceleration.y);
        const float az = static_cast<float>(latest_imu_->linear_acceleration.z);
        std::memcpy(payload.data() + 0U, &ax, sizeof(float));
        std::memcpy(payload.data() + 4U, &ay, sizeof(float));
        std::memcpy(payload.data() + 8U, &az, sizeof(float));
    }

    const auto now_ns     = this->now().nanoseconds();
    const auto tai_coarse = static_cast<uint32_t>(now_ns / 1'000'000'000LL);
    const auto tai_fine   = static_cast<uint32_t>(
        (now_ns % 1'000'000'000LL) * 0x1000000LL / 1'000'000'000LL);

    const auto pkt = tm_bridge_->pack_hk(payload, tai_coarse, tai_fine);
    if (!pkt.empty()) {
        udp_gw_->send(pkt);
    }
}

void FlightCtrlNode::imu_callback(const sensor_msgs::msg::Imu::SharedPtr msg)
{
    /* Non-blocking: cache latest IMU sample for inner-loop and HK packing. */
    latest_imu_ = msg;
}

}  // namespace rover_uav

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(rover_uav::FlightCtrlNode)
