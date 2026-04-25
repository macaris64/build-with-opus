#pragma once

#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <std_msgs/msg/u_int8_multi_array.hpp>

#include "rover_common/lifecycle_base.hpp"
#include "rover_cryobot/tether_client.hpp"

namespace rover_cryobot
{

static constexpr uint16_t APID_CRYOBOT_TM = 0x400U;

class DrillCtrlNode : public rover_common::LifecycleBase
{
public:
    explicit DrillCtrlNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

    CallbackReturn on_configure(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_activate(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_deactivate(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_cleanup(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_shutdown(const rclcpp_lifecycle::State & state) override;

private:
    void hk_timer_callback();
    void drill_cmd_callback(const std_msgs::msg::UInt8MultiArray::SharedPtr msg);

    rclcpp::Subscription<std_msgs::msg::UInt8MultiArray>::SharedPtr drill_cmd_sub_;
    rclcpp::TimerBase::SharedPtr hk_timer_;
    std::unique_ptr<TetherClient> tether_client_;
};

}  // namespace rover_cryobot
