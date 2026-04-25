#include "rover_common/link_state_bridge.hpp"

#include <cstring>
#include <ctime>

#include "rover_common/qos.hpp"

namespace rover_common
{

LinkStateBridge::LinkStateBridge(const rclcpp::NodeOptions & options)
: LifecycleBase("link_state_bridge", options)
{
    RCLCPP_INFO(get_logger(), "LinkStateBridge created (APID 0x%03X)", APID_LINK_STATE);
}

LinkStateBridge::CallbackReturn
LinkStateBridge::on_configure(const rclcpp_lifecycle::State & /*state*/)
{
    link_state_sub_ = create_subscription<rover_common::msg::LinkState>(
        "/prx1/link_state", rover_common::HK_QOS,
        std::bind(&LinkStateBridge::link_state_callback, this, std::placeholders::_1));

    tm_bridge_ = std::make_unique<TmBridge>(APID_LINK_STATE);
    udp_gw_    = std::make_unique<UdpGateway>("127.0.0.1", 5800U);

    RCLCPP_INFO(get_logger(), "LinkStateBridge configured");
    return CallbackReturn::SUCCESS;
}

LinkStateBridge::CallbackReturn
LinkStateBridge::on_activate(const rclcpp_lifecycle::State & /*state*/)
{
    RCLCPP_INFO(get_logger(), "LinkStateBridge activated");
    return CallbackReturn::SUCCESS;
}

LinkStateBridge::CallbackReturn
LinkStateBridge::on_deactivate(const rclcpp_lifecycle::State & /*state*/)
{
    RCLCPP_INFO(get_logger(), "LinkStateBridge deactivated");
    return CallbackReturn::SUCCESS;
}

LinkStateBridge::CallbackReturn
LinkStateBridge::on_cleanup(const rclcpp_lifecycle::State & /*state*/)
{
    link_state_sub_.reset();
    tm_bridge_.reset();
    udp_gw_.reset();

    RCLCPP_INFO(get_logger(), "LinkStateBridge cleaned up");
    return CallbackReturn::SUCCESS;
}

LinkStateBridge::CallbackReturn
LinkStateBridge::on_shutdown(const rclcpp_lifecycle::State & /*state*/)
{
    RCLCPP_INFO(get_logger(), "LinkStateBridge shutting down");
    return CallbackReturn::SUCCESS;
}

void LinkStateBridge::link_state_callback(
    const rover_common::msg::LinkState::SharedPtr msg)
{
    if (!tm_bridge_ || !udp_gw_) {
        return;
    }

    /* 10-byte payload: 1B session_active + 1B signal_strength + 8B last_contact (BE double). */
    std::vector<uint8_t> payload(10U, 0U);
    payload[0] = msg->session_active ? 1U : 0U;
    payload[1] = msg->signal_strength;

    /* Pack last_contact (double, 8 bytes) big-endian via memcpy + byte-swap. */
    const double lc = msg->last_contact;
    uint64_t     lc_bits = 0U;
    static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64-bit");
    std::memcpy(&lc_bits, &lc, sizeof(lc_bits));
    payload[2] = static_cast<uint8_t>((lc_bits >> 56U) & 0xFFU);
    payload[3] = static_cast<uint8_t>((lc_bits >> 48U) & 0xFFU);
    payload[4] = static_cast<uint8_t>((lc_bits >> 40U) & 0xFFU);
    payload[5] = static_cast<uint8_t>((lc_bits >> 32U) & 0xFFU);
    payload[6] = static_cast<uint8_t>((lc_bits >> 24U) & 0xFFU);
    payload[7] = static_cast<uint8_t>((lc_bits >> 16U) & 0xFFU);
    payload[8] = static_cast<uint8_t>((lc_bits >> 8U) & 0xFFU);
    payload[9] = static_cast<uint8_t>(lc_bits & 0xFFU);

    const auto now_ns  = this->now().nanoseconds();
    const auto tai_coarse = static_cast<uint32_t>(now_ns / 1'000'000'000LL);
    const auto tai_fine   = static_cast<uint32_t>(
        (now_ns % 1'000'000'000LL) * 0x1000000LL / 1'000'000'000LL);

    const auto pkt = tm_bridge_->pack_hk(payload, tai_coarse, tai_fine);
    if (!pkt.empty()) {
        udp_gw_->send(pkt);
    }
}

}  // namespace rover_common

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(rover_common::LinkStateBridge)
