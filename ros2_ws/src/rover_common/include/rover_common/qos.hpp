#pragma once

#include <rclcpp/rclcpp.hpp>

namespace rover_common
{

/* Sensor data: high-rate, best-effort (dropping stale samples is acceptable).
 * Use for /scan, /imu, /odom, and any other sensor stream. */
static const rclcpp::QoS SENSOR_QOS = rclcpp::QoS(rclcpp::KeepLast(5)).best_effort();

/* Commands: reliable delivery required; shallow queue for operator responsiveness.
 * Use for /cmd_vel, /drill_cmd, /motor_cmd. */
static const rclcpp::QoS CMD_QOS = rclcpp::QoS(rclcpp::KeepLast(10));

/* Housekeeping telemetry: reliable, single in-flight (1 Hz cadence).
 * One buffered sample is all that is needed before the next arrives. */
static const rclcpp::QoS HK_QOS = rclcpp::QoS(rclcpp::KeepLast(1));

/* Teleop operator input: reliable, same depth as CMD_QOS.
 * Use for /teleop/twist, /teleop/waypoint. */
static const rclcpp::QoS TELEOP_QOS = rclcpp::QoS(rclcpp::KeepLast(10));

}  // namespace rover_common
