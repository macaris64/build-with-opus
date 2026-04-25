#include "rover_land/nav_node.hpp"

#include <chrono>
#include <cstring>
#include <functional>

#include "rover_common/qos.hpp"

using namespace std::chrono_literals;

namespace rover_land
{

static constexpr auto HK_PERIOD = 100ms;

NavNode::NavNode(const rclcpp::NodeOptions & options)
: rover_common::LifecycleBase("nav_node", options)
{
    RCLCPP_INFO(get_logger(), "NavNode created (APID 0x%03X)", APID_LAND_TM);
}

NavNode::CallbackReturn
NavNode::on_configure(const rclcpp_lifecycle::State & /*state*/)
{
    cmd_vel_pub_ = create_publisher<geometry_msgs::msg::Twist>(
        "cmd_vel", rover_common::CMD_QOS);

    land_hk_pub_ = create_publisher<rover_common::msg::Hk>(
        "land_hk", rover_common::HK_QOS);

    odom_sub_ = create_subscription<nav_msgs::msg::Odometry>(
        "odom", rover_common::SENSOR_QOS,
        std::bind(&NavNode::odom_callback, this, std::placeholders::_1));

    tm_bridge_ = std::make_unique<rover_common::TmBridge>(APID_LAND_TM);
    udp_gw_    = std::make_unique<rover_common::UdpGateway>("127.0.0.1", 5800U);

    RCLCPP_INFO(get_logger(), "NavNode configured");
    return CallbackReturn::SUCCESS;
}

NavNode::CallbackReturn
NavNode::on_activate(const rclcpp_lifecycle::State & /*state*/)
{
    cmd_vel_pub_->on_activate();
    land_hk_pub_->on_activate();

    hk_timer_ = create_wall_timer(
        HK_PERIOD,
        std::bind(&NavNode::hk_timer_callback, this));

    RCLCPP_INFO(get_logger(), "NavNode activated");
    return CallbackReturn::SUCCESS;
}

NavNode::CallbackReturn
NavNode::on_deactivate(const rclcpp_lifecycle::State & /*state*/)
{
    hk_timer_->cancel();
    hk_timer_.reset();
    cmd_vel_pub_->on_deactivate();
    land_hk_pub_->on_deactivate();

    RCLCPP_INFO(get_logger(), "NavNode deactivated");
    return CallbackReturn::SUCCESS;
}

NavNode::CallbackReturn
NavNode::on_cleanup(const rclcpp_lifecycle::State & /*state*/)
{
    odom_sub_.reset();
    cmd_vel_pub_.reset();
    land_hk_pub_.reset();
    tm_bridge_.reset();
    udp_gw_.reset();
    latest_odom_.reset();

    RCLCPP_INFO(get_logger(), "NavNode cleaned up");
    return CallbackReturn::SUCCESS;
}

NavNode::CallbackReturn
NavNode::on_shutdown(const rclcpp_lifecycle::State & /*state*/)
{
    RCLCPP_INFO(get_logger(), "NavNode shutting down");
    return CallbackReturn::SUCCESS;
}

void NavNode::hk_timer_callback()
{
    if (!tm_bridge_ || !udp_gw_) {
        return;
    }

    /* Pack 32-byte payload: position xyz (3 × float32) + reserved. */
    std::vector<uint8_t> payload(32U, 0U);
    if (latest_odom_) {
        const auto & pos = latest_odom_->pose.pose.position;
        const float px = static_cast<float>(pos.x);
        const float py = static_cast<float>(pos.y);
        const float pz = static_cast<float>(pos.z);
        std::memcpy(payload.data() + 0U, &px, sizeof(float));
        std::memcpy(payload.data() + 4U, &py, sizeof(float));
        std::memcpy(payload.data() + 8U, &pz, sizeof(float));
    }

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
    if (land_hk_pub_ && land_hk_pub_->is_activated()) {
        rover_common::msg::Hk hk_msg;
        hk_msg.seq       = hk_seq_++;
        hk_msg.timestamp = this->now().seconds();
        /* Copy user-data portion (bytes 16+) into the fixed 32-byte field. */
        for (size_t i = 0U; i < 32U && (i + 16U) < pkt.size(); ++i) {
            hk_msg.payload[i] = pkt[i + 16U];
        }
        land_hk_pub_->publish(hk_msg);
    }
}

void NavNode::odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
{
    /* Non-blocking: store latest odometry for HK packing in hk_timer_callback. */
    latest_odom_ = msg;
}

}  // namespace rover_land

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(rover_land::NavNode)
