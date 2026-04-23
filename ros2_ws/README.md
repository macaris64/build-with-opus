# ros2_ws/ — Space ROS 2 Colcon Workspace

Surface rovers: wheeled (`rover_land`), aerial (`rover_uav`), subsurface (`rover_cryobot`). C++17 lifecycle-node compositions.

## Layout

```
ros2_ws/
└── src/
    ├── rover_teleop/       — reference lifecycle-node teleop (canonical template)
    ├── rover_bringup/      — composite launch files & parameter YAML
    ├── rover_common/       — (planned) msg types, shared QoS, tm_bridge helper
    ├── rover_land/         — (planned) wheeled-rover nodes
    ├── rover_uav/          — (planned) aerial-rover nodes
    └── rover_cryobot/      — (planned) subsurface-rover nodes
```

Per-class packages, per-instance via launch-file parameters. Five `rover_land` instances in Scale-5 use the same package with different namespaces.

## Build & Test

From inside `ros2_ws/`:

```bash
colcon build --symlink-install
colcon test
```

Space ROS 2 (Humble-compatible) must be sourced first (`source /opt/ros/humble/setup.bash` or equivalent).

## Where to read more

- **Architecture** — [`../docs/architecture/04-rovers-spaceros.md`](../docs/architecture/04-rovers-spaceros.md) (node compositions, QoS profiles, lifecycle rules).
- **Coding rules** — [`../.claude/rules/ros2-nodes.md`](../.claude/rules/ros2-nodes.md) (lifecycle-node requirements), [`../.claude/rules/general.md`](../.claude/rules/general.md) (C++ universal), [`../.claude/rules/testing.md`](../.claude/rules/testing.md).
- **Canonical template** — [`src/rover_teleop/src/teleop_node.cpp`](src/rover_teleop/src/teleop_node.cpp) demonstrates every required pattern.

## Hard rules (non-exhaustive — see `.claude/rules/ros2-nodes.md`)

- Every node subclasses `rclcpp_lifecycle::LifecycleNode`; plain `rclcpp::Node` is banned.
- All five lifecycle callbacks (`on_configure`, `on_activate`, `on_deactivate`, `on_cleanup`, `on_shutdown`) must be implemented.
- Callbacks must not block — offload heavy work to a separate thread or callback group.
- QoS profiles are named constants at file scope; no inline `rclcpp::QoS(...)` at publisher/subscriber creation.
- Log via `RCLCPP_INFO(get_logger(), ...)` only; `std::cout`/`printf`/`fprintf` banned.
- Configure → activate → deactivate → cleanup round-trip test is the minimum required per package.
