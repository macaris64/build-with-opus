#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>

#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include "rover_common/lifecycle_base.hpp"
#include "rover_common/msg/hk.hpp"
#include "rover_common/tm_bridge.hpp"

namespace fleet_monitor
{

/* APID 0x160 — intra-fleet DDS heartbeat (mids.h: FLEET_MONITOR_HK_MID). */
static constexpr uint16_t APID_FLEET_MONITOR = 0x160U;

/* A rover heartbeat record — updated on each subscription callback. */
struct RoverHeartbeat {
    rclcpp::Time last_seen{0, 0, RCL_ROS_TIME};
    bool         ever_seen{false};
};

/* FleetMonitorNode — aggregates DDS heartbeats from all three rovers and
 * emits a 13-byte CCSDS SPP at 1 Hz via UDP:5800 to ros2_bridge.
 *
 * Payload layout (big-endian multi-byte fields):
 *   byte 0:    health_mask  (bit 0=land, bit 1=uav, bit 2=cryo)
 *   bytes 1-4: land_age_ms  (uint32, BE)
 *   bytes 5-8: uav_age_ms   (uint32, BE)
 *   bytes 9-12: cryo_age_ms (uint32, BE)
 *
 * A rover is considered alive when its last heartbeat is within STALE_THRESHOLD.
 */
class FleetMonitorNode : public rover_common::LifecycleBase
{
public:
    explicit FleetMonitorNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

    CallbackReturn on_configure(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_activate(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_deactivate(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_cleanup(const rclcpp_lifecycle::State & state) override;
    CallbackReturn on_shutdown(const rclcpp_lifecycle::State & state) override;

    /* Exposed for testing. */
    const std::array<RoverHeartbeat, 3> & heartbeats() const noexcept
    {
        return heartbeats_;
    }

private:
    void hk_timer_callback();
    void land_hk_callback(const rover_common::msg::Hk::SharedPtr msg);
    void uav_state_callback(const nav_msgs::msg::Odometry::SharedPtr msg);
    void cryo_hk_callback(const rover_common::msg::Hk::SharedPtr msg);

    /* Stale threshold: rover is "dead" if no HB within this window. */
    static constexpr std::chrono::seconds STALE_THRESHOLD{3};

    rclcpp::Subscription<rover_common::msg::Hk>::SharedPtr       land_sub_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr      uav_sub_;
    rclcpp::Subscription<rover_common::msg::Hk>::SharedPtr        cryo_sub_;
    rclcpp::TimerBase::SharedPtr                                   hk_timer_;
    std::unique_ptr<rover_common::TmBridge>                       tm_bridge_;
    std::unique_ptr<rover_common::UdpGateway>                     udp_gw_;

    /* Index: 0 = land, 1 = uav, 2 = cryo. */
    std::array<RoverHeartbeat, 3> heartbeats_{};
};

}  // namespace fleet_monitor
