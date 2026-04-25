# rover_land

Wheeled locomotion navigation node for the Europa surface rover.

## Node: `NavNode`

**APID**: `0x300` (TM block `0x300–0x37F` per `apid-registry.md`)
**Lifecycle base**: `rover_common::LifecycleBase` → `rclcpp_lifecycle::LifecycleNode`
**Package version**: 0.1.0

### Interfaces

| Direction | Topic | Message type | QoS constant |
|---|---|---|---|
| Publish | `/cmd_vel` | `geometry_msgs/Twist` | `rover_common::CMD_QOS` |
| Subscribe | `/odom` | `nav_msgs/Odometry` | `rover_common::SENSOR_QOS` |

### Lifecycle Callbacks

| Transition | Action |
|---|---|
| `on_configure` | Creates `/cmd_vel` lifecycle publisher, `/odom` subscriber, `TmBridge(0x300U)` |
| `on_activate` | Activates publisher, starts 100 ms HK timer |
| `on_deactivate` | Cancels timer, deactivates publisher |
| `on_cleanup` | Resets publisher, subscriber, bridge |
| `on_shutdown` | Delegates to `on_cleanup` |

### HK Timer

Fires every 100 ms in ACTIVE state. Packs a zero-payload HK packet via `TmBridge(0x300U)` and
logs the resulting packet size with `RCLCPP_DEBUG`. The packed bytes contain the CCSDS primary
header (6 B) + secondary header (10 B) with APID `0x300` encoded in bits [10:0].

## Build and Test

```bash
cd ros2_ws
colcon build --symlink-install --packages-select rover_land
colcon test --packages-select rover_land
colcon test-result --verbose
```

## Test Coverage (`test/test_rover_land.cpp`)

| Test | Description |
|---|---|
| `LifecycleRoundTrip` | `configure → activate → deactivate → cleanup` each returns `CallbackReturn::SUCCESS` |
| `ApidEncoding` | `TmBridge(0x300U).pack_hk({}, 0, 0)` → 16 B packet with APID bits [10:0] == `0x300` |

## Dependencies

| Package | Role |
|---|---|
| `rover_common` | `LifecycleBase`, `TmBridge`, `CMD_QOS`, `SENSOR_QOS` |
| `rclcpp_lifecycle` | Lifecycle node framework |
| `geometry_msgs` | `/cmd_vel` message type |
| `nav_msgs` | `/odom` message type |
