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

    cryo_hk_pub_ = create_publisher<rover_common::msg::Hk>(
        "cryo_hk", rover_common::HK_QOS);

    tether_client_ = std::make_unique<TetherClient>(APID_CRYOBOT_TM);
    tm_bridge_     = std::make_unique<rover_common::TmBridge>(APID_CRYOBOT_TM);
    udp_gw_        = std::make_unique<rover_common::UdpGateway>("127.0.0.1", 5800U);

    RCLCPP_INFO(get_logger(), "DrillCtrlNode configured");
    return CallbackReturn::SUCCESS;
}

DrillCtrlNode::CallbackReturn
DrillCtrlNode::on_activate(const rclcpp_lifecycle::State & /*state*/)
{
    cryo_hk_pub_->on_activate();

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
    cryo_hk_pub_->on_deactivate();

    RCLCPP_INFO(get_logger(), "DrillCtrlNode deactivated");
    return CallbackReturn::SUCCESS;
}

DrillCtrlNode::CallbackReturn
DrillCtrlNode::on_cleanup(const rclcpp_lifecycle::State & /*state*/)
{
    drill_cmd_sub_.reset();
    cryo_hk_pub_.reset();
    tether_client_.reset();
    tm_bridge_.reset();
    udp_gw_.reset();

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
    /* Tether path (HDLC-lite framing) — kept for physical TCP tether in Phase 40. */
    if (tether_client_) {
        auto frame = tether_client_->pack_and_frame({}, 0U, 0U, HdlcLite::MODE_NOMINAL);
        RCLCPP_DEBUG(get_logger(), "Tether frame packed: %zu bytes", frame.size());
    }

    if (!tm_bridge_ || !udp_gw_) {
        return;
    }

    /* 32-byte payload: depth stub (float32 = 0.0) + reserved. */
    std::vector<uint8_t> payload(32U, 0U);

    const auto now_ns     = this->now().nanoseconds();
    const auto tai_coarse = static_cast<uint32_t>(now_ns / 1'000'000'000LL);
    const auto tai_fine   = static_cast<uint32_t>(
        (now_ns % 1'000'000'000LL) * 0x1000000LL / 1'000'000'000LL);

    const auto pkt = tm_bridge_->pack_hk(payload, tai_coarse, tai_fine);
    if (pkt.empty()) {
        return;
    }

    /* Forward to cFS ros2_bridge via UDP:5800. */
    udp_gw_->send(pkt);

    /* Publish on DDS for fleet_monitor subscription. */
    if (cryo_hk_pub_ && cryo_hk_pub_->is_activated()) {
        rover_common::msg::Hk hk_msg;
        hk_msg.seq       = hk_seq_++;
        hk_msg.timestamp = this->now().seconds();
        for (size_t i = 0U; i < 32U && (i + 16U) < pkt.size(); ++i) {
            hk_msg.payload[i] = pkt[i + 16U];
        }
        cryo_hk_pub_->publish(hk_msg);
    }
}

void DrillCtrlNode::drill_cmd_callback(
    const std_msgs::msg::UInt8MultiArray::SharedPtr /*msg*/)
{
    /* Non-blocking: decode and queue TC for drill actuation stub. */
}

}  // namespace rover_cryobot

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(rover_cryobot::DrillCtrlNode)
