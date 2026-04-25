#pragma once

#include <memory>

#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include "rover_common/lifecycle_base.hpp"
#include "rover_common/tm_bridge.hpp"

namespace rover_land
{

static constexpr uint16_t APID_LAND_TM = 0x300U;

class NavNode : public rover_common::LifecycleBase
{
public:
    explicit NavNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

    CallbackReturn on_configure(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_activate(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_deactivate(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_cleanup(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_shutdown(const rclcpp_lifecycle::State & state) override;

private:
    void hk_timer_callback();
    void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg);

    rclcpp_lifecycle::LifecyclePublisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
    rclcpp::TimerBase::SharedPtr hk_timer_;
    std::unique_ptr<rover_common::TmBridge> tm_bridge_;
};

}  // namespace rover_land
