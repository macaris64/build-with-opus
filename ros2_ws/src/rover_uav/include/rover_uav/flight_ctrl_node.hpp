#pragma once

#include <cstdint>
#include <memory>

#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <sensor_msgs/msg/imu.hpp>

#include "rover_common/lifecycle_base.hpp"
#include "rover_common/tm_bridge.hpp"

namespace rover_uav
{

static constexpr uint16_t APID_UAV_TM = 0x3C0U;

class FlightCtrlNode : public rover_common::LifecycleBase
{
public:
    explicit FlightCtrlNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

    CallbackReturn on_configure(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_activate(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_deactivate(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_cleanup(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_shutdown(const rclcpp_lifecycle::State & state) override;

private:
    void inner_loop_callback();
    void hk_timer_callback();
    void imu_callback(const sensor_msgs::msg::Imu::SharedPtr msg);

    rclcpp_lifecycle::LifecyclePublisher<nav_msgs::msg::Odometry>::SharedPtr state_est_pub_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
    rclcpp::TimerBase::SharedPtr inner_loop_timer_;
    rclcpp::TimerBase::SharedPtr hk_timer_;
    std::unique_ptr<rover_common::TmBridge>   tm_bridge_;
    std::unique_ptr<rover_common::UdpGateway> udp_gw_;
    sensor_msgs::msg::Imu::SharedPtr          latest_imu_;
};

}  // namespace rover_uav
