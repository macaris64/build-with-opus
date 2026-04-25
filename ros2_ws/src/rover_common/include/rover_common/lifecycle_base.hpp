#pragma once

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

namespace rover_common
{

/* LifecycleBase — header-only base class for all rover lifecycle nodes.
 *
 * Provides default SUCCESS implementations of all five lifecycle callbacks so
 * Phase 37 node classes only override the transitions they actually use.
 * Inherit and override as needed:
 *
 *   class NavNode : public rover_common::LifecycleBase { ... };
 */
class LifecycleBase : public rclcpp_lifecycle::LifecycleNode
{
public:
    explicit LifecycleBase(
        const std::string & node_name,
        const rclcpp::NodeOptions & options = rclcpp::NodeOptions())
    : rclcpp_lifecycle::LifecycleNode(node_name, options)
    {
        RCLCPP_INFO(get_logger(), "LifecycleBase '%s' created", node_name.c_str());
    }

    CallbackReturn on_configure(const rclcpp_lifecycle::State & /*state*/) override
    {
        RCLCPP_INFO(get_logger(), "on_configure");
        return CallbackReturn::SUCCESS;
    }

    CallbackReturn on_activate(const rclcpp_lifecycle::State & /*state*/) override
    {
        RCLCPP_INFO(get_logger(), "on_activate");
        return CallbackReturn::SUCCESS;
    }

    CallbackReturn on_deactivate(const rclcpp_lifecycle::State & /*state*/) override
    {
        RCLCPP_INFO(get_logger(), "on_deactivate");
        return CallbackReturn::SUCCESS;
    }

    CallbackReturn on_cleanup(const rclcpp_lifecycle::State & /*state*/) override
    {
        RCLCPP_INFO(get_logger(), "on_cleanup");
        return CallbackReturn::SUCCESS;
    }

    CallbackReturn on_shutdown(const rclcpp_lifecycle::State & /*state*/) override
    {
        RCLCPP_INFO(get_logger(), "on_shutdown");
        return CallbackReturn::SUCCESS;
    }
};

}  // namespace rover_common
