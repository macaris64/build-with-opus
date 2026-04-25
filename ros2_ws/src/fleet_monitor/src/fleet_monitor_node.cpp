#include "fleet_monitor/fleet_monitor_node.hpp"

#include <chrono>
#include <functional>

#include "rover_common/qos.hpp"

using namespace std::chrono_literals;

namespace fleet_monitor
{

static constexpr auto HK_PERIOD = 1000ms;

FleetMonitorNode::FleetMonitorNode(const rclcpp::NodeOptions & options)
: rover_common::LifecycleBase("fleet_monitor_node", options)
{
    RCLCPP_INFO(get_logger(), "FleetMonitorNode created (APID 0x%03X)", APID_FLEET_MONITOR);
}

FleetMonitorNode::CallbackReturn
FleetMonitorNode::on_configure(const rclcpp_lifecycle::State & /*state*/)
{
    land_sub_ = create_subscription<rover_common::msg::Hk>(
        "land_hk", rover_common::HK_QOS,
        std::bind(&FleetMonitorNode::land_hk_callback, this, std::placeholders::_1));

    uav_sub_ = create_subscription<nav_msgs::msg::Odometry>(
        "state_est", rover_common::SENSOR_QOS,
        std::bind(&FleetMonitorNode::uav_state_callback, this, std::placeholders::_1));

    cryo_sub_ = create_subscription<rover_common::msg::Hk>(
        "cryo_hk", rover_common::HK_QOS,
        std::bind(&FleetMonitorNode::cryo_hk_callback, this, std::placeholders::_1));

    tm_bridge_ = std::make_unique<rover_common::TmBridge>(APID_FLEET_MONITOR);
    udp_gw_    = std::make_unique<rover_common::UdpGateway>("127.0.0.1", 5800U);

    RCLCPP_INFO(get_logger(), "FleetMonitorNode configured");
    return CallbackReturn::SUCCESS;
}

FleetMonitorNode::CallbackReturn
FleetMonitorNode::on_activate(const rclcpp_lifecycle::State & /*state*/)
{
    hk_timer_ = create_wall_timer(
        HK_PERIOD,
        std::bind(&FleetMonitorNode::hk_timer_callback, this));

    RCLCPP_INFO(get_logger(), "FleetMonitorNode activated");
    return CallbackReturn::SUCCESS;
}

FleetMonitorNode::CallbackReturn
FleetMonitorNode::on_deactivate(const rclcpp_lifecycle::State & /*state*/)
{
    hk_timer_->cancel();
    hk_timer_.reset();

    RCLCPP_INFO(get_logger(), "FleetMonitorNode deactivated");
    return CallbackReturn::SUCCESS;
}

FleetMonitorNode::CallbackReturn
FleetMonitorNode::on_cleanup(const rclcpp_lifecycle::State & /*state*/)
{
    land_sub_.reset();
    uav_sub_.reset();
    cryo_sub_.reset();
    tm_bridge_.reset();
    udp_gw_.reset();

    RCLCPP_INFO(get_logger(), "FleetMonitorNode cleaned up");
    return CallbackReturn::SUCCESS;
}

FleetMonitorNode::CallbackReturn
FleetMonitorNode::on_shutdown(const rclcpp_lifecycle::State & /*state*/)
{
    RCLCPP_INFO(get_logger(), "FleetMonitorNode shutting down");
    return CallbackReturn::SUCCESS;
}

void FleetMonitorNode::hk_timer_callback()
{
    if (!tm_bridge_ || !udp_gw_) {
        return;
    }

    const rclcpp::Time now = this->now();

    auto age_ms = [&](size_t idx) -> uint32_t {
        if (!heartbeats_[idx].ever_seen) {
            return UINT32_MAX;
        }
        const auto diff_ns = (now - heartbeats_[idx].last_seen).nanoseconds();
        if (diff_ns < 0) {
            return 0U;
        }
        const uint64_t ms = static_cast<uint64_t>(diff_ns) / 1'000'000ULL;
        return ms > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(ms);
    };

    auto is_alive = [&](size_t idx) -> bool {
        if (!heartbeats_[idx].ever_seen) { return false; }
        const auto diff = now - heartbeats_[idx].last_seen;
        return diff.seconds() < static_cast<double>(STALE_THRESHOLD.count());
    };

    const uint8_t mask = static_cast<uint8_t>(
        (is_alive(0U) ? 0x01U : 0x00U) |
        (is_alive(1U) ? 0x02U : 0x00U) |
        (is_alive(2U) ? 0x04U : 0x00U));

    const uint32_t land_age = age_ms(0U);
    const uint32_t uav_age  = age_ms(1U);
    const uint32_t cryo_age = age_ms(2U);

    /* 13-byte payload packed big-endian. */
    std::vector<uint8_t> payload(13U, 0U);
    payload[0] = mask;
    payload[1] = static_cast<uint8_t>((land_age >> 24U) & 0xFFU);
    payload[2] = static_cast<uint8_t>((land_age >> 16U) & 0xFFU);
    payload[3] = static_cast<uint8_t>((land_age >> 8U)  & 0xFFU);
    payload[4] = static_cast<uint8_t>(land_age           & 0xFFU);
    payload[5] = static_cast<uint8_t>((uav_age  >> 24U) & 0xFFU);
    payload[6] = static_cast<uint8_t>((uav_age  >> 16U) & 0xFFU);
    payload[7] = static_cast<uint8_t>((uav_age  >> 8U)  & 0xFFU);
    payload[8] = static_cast<uint8_t>(uav_age            & 0xFFU);
    payload[9]  = static_cast<uint8_t>((cryo_age >> 24U) & 0xFFU);
    payload[10] = static_cast<uint8_t>((cryo_age >> 16U) & 0xFFU);
    payload[11] = static_cast<uint8_t>((cryo_age >> 8U)  & 0xFFU);
    payload[12] = static_cast<uint8_t>(cryo_age           & 0xFFU);

    const auto now_ns     = now.nanoseconds();
    const auto tai_coarse = static_cast<uint32_t>(now_ns / 1'000'000'000LL);
    const auto tai_fine   = static_cast<uint32_t>(
        (now_ns % 1'000'000'000LL) * 0x1000000LL / 1'000'000'000LL);

    const auto pkt = tm_bridge_->pack_hk(payload, tai_coarse, tai_fine);
    if (!pkt.empty()) {
        udp_gw_->send(pkt);
    }

    RCLCPP_DEBUG(get_logger(), "Fleet health mask=0x%02X land=%ums uav=%ums cryo=%ums",
                 mask, land_age, uav_age, cryo_age);
}

void FleetMonitorNode::land_hk_callback(const rover_common::msg::Hk::SharedPtr /*msg*/)
{
    heartbeats_[0].last_seen  = this->now();
    heartbeats_[0].ever_seen  = true;
}

void FleetMonitorNode::uav_state_callback(const nav_msgs::msg::Odometry::SharedPtr /*msg*/)
{
    heartbeats_[1].last_seen = this->now();
    heartbeats_[1].ever_seen = true;
}

void FleetMonitorNode::cryo_hk_callback(const rover_common::msg::Hk::SharedPtr /*msg*/)
{
    heartbeats_[2].last_seen = this->now();
    heartbeats_[2].ever_seen = true;
}

}  // namespace fleet_monitor

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(fleet_monitor::FleetMonitorNode)
