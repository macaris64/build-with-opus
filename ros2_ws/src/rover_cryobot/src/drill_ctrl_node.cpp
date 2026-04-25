#include "rover_cryobot/drill_ctrl_node.hpp"

#include <chrono>
#include <functional>

#include "rover_common/qos.hpp"

using namespace std::chrono_literals;

namespace rover_cryobot
{

static constexpr auto HK_PERIOD = 1000ms;  /* 1 Hz tether HK cadence */

DrillCtrlNode::DrillCtrlNode(const rclcpp::NodeOptions & options)
: rover_common::LifecycleBase("drill_ctrl_node", options)
{
    RCLCPP_INFO(get_logger(), "DrillCtrlNode created (APID 0x%03X)", APID_CRYOBOT_TM);
}

DrillCtrlNode::CallbackReturn
DrillCtrlNode::on_configure(const rclcpp_lifecycle::State & /*state*/)
{
    drill_cmd_sub_ = create_subscription<std_msgs::msg::UInt8MultiArray>(
        "drill_cmd", rover_common::CMD_QOS,
        std::bind(&DrillCtrlNode::drill_cmd_callback, this, std::placeholders::_1));

    tether_client_ = std::make_unique<TetherClient>(APID_CRYOBOT_TM);

    RCLCPP_INFO(get_logger(), "DrillCtrlNode configured");
    return CallbackReturn::SUCCESS;
}

DrillCtrlNode::CallbackReturn
DrillCtrlNode::on_activate(const rclcpp_lifecycle::State & /*state*/)
{
    hk_timer_ = create_wall_timer(
        HK_PERIOD,
        std::bind(&DrillCtrlNode::hk_timer_callback, this));

    RCLCPP_INFO(get_logger(), "DrillCtrlNode activated");
    return CallbackReturn::SUCCESS;
}

DrillCtrlNode::CallbackReturn
DrillCtrlNode::on_deactivate(const rclcpp_lifecycle::State & /*state*/)
{
    hk_timer_->cancel();
    hk_timer_.reset();

    RCLCPP_INFO(get_logger(), "DrillCtrlNode deactivated");
    return CallbackReturn::SUCCESS;
}

DrillCtrlNode::CallbackReturn
DrillCtrlNode::on_cleanup(const rclcpp_lifecycle::State & /*state*/)
{
    drill_cmd_sub_.reset();
    tether_client_.reset();

    RCLCPP_INFO(get_logger(), "DrillCtrlNode cleaned up");
    return CallbackReturn::SUCCESS;
}

DrillCtrlNode::CallbackReturn
DrillCtrlNode::on_shutdown(const rclcpp_lifecycle::State & /*state*/)
{
    RCLCPP_INFO(get_logger(), "DrillCtrlNode shutting down");
    return CallbackReturn::SUCCESS;
}

void DrillCtrlNode::hk_timer_callback()
{
    /* Non-blocking: pack HK into HDLC-lite frame and log size.
     * Physical TCP tether socket is wired in Phase 40 (SITL integration). */
    if (!tether_client_) {
        return;
    }
    auto frame = tether_client_->pack_and_frame({}, 0U, 0U, HdlcLite::MODE_NOMINAL);
    RCLCPP_DEBUG(get_logger(), "Tether frame packed: %zu bytes", frame.size());
}

void DrillCtrlNode::drill_cmd_callback(
    const std_msgs::msg::UInt8MultiArray::SharedPtr /*msg*/)
{
    /* Non-blocking: decode and queue TC for drill actuation stub. */
}

}  // namespace rover_cryobot

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(rover_cryobot::DrillCtrlNode)
