#pragma once

#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include "rover_common/lifecycle_base.hpp"
#include "rover_common/msg/link_state.hpp"
#include "rover_common/tm_bridge.hpp"

namespace rover_common
{

/* APID 0x129 — Proximity-1 session-state TM (mids.h: ROS_GND_LINK_STATE_MID). */
static constexpr uint16_t APID_LINK_STATE = 0x129U;

/* LinkStateBridge — forwards ROS 2 LinkState messages to cFS as CCSDS SPP.
 *
 * Subscribes to /prx1/link_state, serialises each message into a 10-byte
 * payload (1 B session_active + 1 B signal_strength + 8 B last_contact as
 * IEEE-754 double packed big-endian), wraps it in a CCSDS TM packet with
 * APID 0x129, and sends it via UDP:5800 to ros2_bridge for SB forwarding.
 *
 * Lifecycle: configure → activate → publish (callback-driven).
 */
class LinkStateBridge : public LifecycleBase
{
public:
    explicit LinkStateBridge(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

    CallbackReturn on_configure(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_activate(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_deactivate(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_cleanup(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_shutdown(const rclcpp_lifecycle::State & state) override;

private:
    void link_state_callback(const rover_common::msg::LinkState::SharedPtr msg);

    rclcpp::Subscription<rover_common::msg::LinkState>::SharedPtr link_state_sub_;
    std::unique_ptr<TmBridge>    tm_bridge_;
    std::unique_ptr<UdpGateway>  udp_gw_;
};

}  // namespace rover_common
