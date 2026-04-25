# rover_uav

Flight control node for the UAV drone deployed from the Europa surface rover.

## Node: `FlightCtrlNode`

**APID**: `0x3C0` (TM block `0x3C0–0x3FF` per `apid-registry.md`)
**Lifecycle base**: `rover_common::LifecycleBase` → `rclcpp_lifecycle::LifecycleNode`
**Package version**: 0.1.0

### Interfaces

| Direction | Topic | Message type | QoS constant |
|---|---|---|---|
| Subscribe | `/imu` | `sensor_msgs/Imu` | `rover_common::SENSOR_QOS` |
| Publish | `/state_est` | `nav_msgs/Odometry` | `rover_common::SENSOR_QOS` |

### Lifecycle Callbacks

| Transition | Action |
|---|---|
| `on_configure` | Creates `/imu` subscriber, `/state_est` lifecycle publisher, `TmBridge(0x3C0U)` |
| `on_activate` | Activates publisher, starts 10 ms inner-loop timer (100 Hz) |
| `on_deactivate` | Cancels timer, deactivates publisher |
| `on_cleanup` | Resets subscriber, publisher, bridge |
| `on_shutdown` | Delegates to `on_cleanup` |

### Inner-Loop Timer

Fires every 10 ms (100 Hz) in ACTIVE state. Reserved for attitude estimation and control law.
Phase 37 implements a non-blocking stub; the Gazebo plugin sensor feed is wired in Phase 38.
The timer callback logs its sequence number via `RCLCPP_DEBUG` with no blocking operations.

## Build and Test

```bash
cd ros2_ws
colcon build --symlink-install --packages-select rover_uav
colcon test --packages-select rover_uav
colcon test-result --verbose
```

## Test Coverage (`test/test_rover_uav.cpp`)

| Test | Description |
|---|---|
| `LifecycleRoundTrip` | `configure → activate → deactivate → cleanup` each returns `CallbackReturn::SUCCESS` |
| `ApidEncoding` | `TmBridge(0x3C0U).pack_hk({}, 0, 0)` → 16 B packet with APID bits [10:0] == `0x3C0` |

## Dependencies

| Package | Role |
|---|---|
| `rover_common` | `LifecycleBase`, `TmBridge`, `SENSOR_QOS` |
| `rclcpp_lifecycle` | Lifecycle node framework |
| `sensor_msgs` | `/imu` message type |
| `nav_msgs` | `/state_est` message type |
