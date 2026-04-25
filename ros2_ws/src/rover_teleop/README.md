# rover_teleop

Lifecycle teleop node for the SAKURA-II surface rover. Publishes zero-velocity `geometry_msgs/Twist` messages at 10 Hz and serves as the canonical reference implementation for all rover lifecycle nodes.

## Purpose

- Provides the operator command-velocity interface (`/cmd_vel`).
- Acts as the template that Phase 37 rover class nodes (`rover_land`, `rover_uav`, `rover_cryobot`) follow for structure, QoS usage, and lifecycle management.

## Contents

```
rover_teleop/
├── include/rover_teleop/
│   └── teleop_node.hpp     — TeleopNode class declaration
├── src/
│   └── teleop_node.cpp     — implementation + component registration
└── test/
    └── test_teleop.cpp     — lifecycle transition tests (3 gtest cases)
```

## Node: `TeleopNode`

| Property | Value |
|---|---|
| Class | `rover_teleop::TeleopNode` |
| Base | `rclcpp_lifecycle::LifecycleNode` |
| Publish topic | `/cmd_vel` (`geometry_msgs/Twist`) |
| QoS | `CMD_VEL_QOS` — reliable, keep-last 10 |
| Publish rate | 10 Hz (100 ms wall timer, active only) |

### Lifecycle callbacks

| Callback | Action |
|---|---|
| `on_configure` | Creates lifecycle publisher on `/cmd_vel` |
| `on_activate` | Activates publisher; starts 10 Hz timer |
| `on_deactivate` | Cancels timer; deactivates publisher |
| `on_cleanup` | Resets publisher |
| `on_shutdown` | Logs shutdown; returns SUCCESS |

## Build & Test

```bash
# From ros2_ws/
colcon build --packages-select rover_teleop --symlink-install
colcon test  --packages-select rover_teleop
colcon test-result --verbose
# Expected: 3 tests, 0 failures
```

## Running

Launch via `rover_bringup` (recommended — loads parameters from YAML):

```bash
source /opt/ros/spaceros/setup.bash && source install/setup.bash
ros2 launch rover_bringup rover.launch.py
```

Or directly (no parameters):

```bash
ros2 run rover_teleop teleop_node
```

Then manage the lifecycle:

```bash
ros2 lifecycle set /teleop_node configure
ros2 lifecycle set /teleop_node activate
ros2 topic echo /cmd_vel
```

## Conventions

- QoS defined as a file-scope named constant (`CMD_VEL_QOS`) — never inline per `.claude/rules/ros2-nodes.md`.
- Timer callback is non-blocking (zero-duration publish only).
- Registered as a composable component via `RCLCPP_COMPONENTS_REGISTER_NODE`.
