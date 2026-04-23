# How-To: Launch a Space ROS 2 Node

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Reference exemplars: [`ros2_ws/src/rover_teleop/`](../../../ros2_ws/src/rover_teleop/), [`ros2_ws/src/rover_bringup/launch/rover.launch.py`](../../../ros2_ws/src/rover_bringup/launch/rover.launch.py). Rules: [`../../../.claude/rules/ros2-nodes.md`](../../../.claude/rules/ros2-nodes.md), [`general.md`](../../../.claude/rules/general.md). Architecture: [`../../architecture/04-rovers-spaceros.md`](../../architecture/04-rovers-spaceros.md).

This guide walks adding and launching a new ROS 2 node under [`ros2_ws/src/`](../../../ros2_ws/src/). It is a signpost — the rules live in `.claude/rules/`, the architecture lives in `04-rovers-spaceros.md`.

## 1. When this applies

- Adding a new rover sub-component (nav, teleop, comm, sensor bridge) as a lifecycle node.
- Launching an existing node with a new parameter file or composition.

Do **not** use this guide for Gazebo plugins ([`writing-a-gazebo-plugin.md`](writing-a-gazebo-plugin.md)) — Gazebo plugins live in-process with Gazebo, not under `ros2_ws/`.

## 2. Prerequisites

- Quickstart green — `colcon build && colcon test` pass ([`../quickstart.md §6`](../quickstart.md)).
- `source /opt/ros/<distro>/setup.bash` (or Space ROS overlay) is in your shell.
- You've read [`rover_teleop.cpp`](../../../ros2_ws/src/rover_teleop/) — it's the canonical lifecycle-node shape.

## 3. Steps

### 3.1 Create the package

```
ros2_ws/src/<package_name>/
├── CMakeLists.txt
├── package.xml
├── include/<package_name>/
├── src/
├── launch/
├── config/           — YAML parameter files
└── test/
```

Choose `ament_cmake` (C++) per [04](../../architecture/04-rovers-spaceros.md). Avoid `ament_python` for mission-critical nodes — C++ lifecycle semantics are stricter.

### 3.2 Subclass `LifecycleNode`, not `Node`

Per [`.claude/rules/ros2-nodes.md`](../../../.claude/rules/ros2-nodes.md) and [CLAUDE.md §ROS 2](../../../CLAUDE.md):

```cpp
class MyNode : public rclcpp_lifecycle::LifecycleNode {
  // ...
  CallbackReturn on_configure(const State &) override;
  CallbackReturn on_activate(const State &) override;
  CallbackReturn on_deactivate(const State &) override;
  CallbackReturn on_cleanup(const State &) override;
};
```

Plain `rclcpp::Node` is banned. If you need ad-hoc-node semantics, that is usually the wrong design — lifecycle states are how SAKURA-II asserts mode authority.

### 3.3 Callbacks must not block

Per [`.claude/rules/general.md`](../../../.claude/rules/general.md): subscription, timer, and service callbacks must return in < 1 ms of wall time. If the work is non-trivial:

- Offload to a worker thread (`std::thread` or `std::async`) and signal completion via a latched topic or an atomic.
- Do not sleep, block on I/O, or acquire a mutex that any other thread can hold for more than a tick.

### 3.4 QoS profiles as named constants

Per [`.claude/rules/ros2-nodes.md`](../../../.claude/rules/ros2-nodes.md): declare QoS at file scope, not inline:

```cpp
static const rclcpp::QoS kHkQos = rclcpp::QoS(10).reliable().durability_volatile();
```

Inline `rclcpp::QoS(...)` at the publisher / subscriber call site is banned. Named constants make compatibility mismatches grep-findable.

### 3.5 Logging

Per [`.claude/rules/general.md`](../../../.claude/rules/general.md): `RCLCPP_INFO(get_logger(), ...)` and `RCLCPP_ERROR(...)` only. `std::cout` and `printf` are banned.

### 3.6 Launch file

See [`rover.launch.py`](../../../ros2_ws/src/rover_bringup/launch/rover.launch.py) as the exemplar. Prefer parameters-driven launches over per-instance launch files — per-instance proliferation violates the "per-class, not per-instance" convention ([`../../README.md`](../../README.md) Conventions).

```python
from launch import LaunchDescription
from launch_ros.actions import LifecycleNode

def generate_launch_description():
    return LaunchDescription([
        LifecycleNode(
            name='my_node',
            namespace='rover',
            package='<package_name>',
            executable='<exe>',
            parameters=['config/my_node.yaml'],
            output='screen',
        ),
    ])
```

### 3.7 Unit tests

Per [`.claude/rules/testing.md`](../../../.claude/rules/testing.md): gtest under `test/`, one failure-path test minimum. `colcon test --packages-select <package_name>` runs them.

### 3.8 `use_sim_time`

For SITL, set `use_sim_time: true` in the parameter YAML — rover nodes consume `/clock` published by the sim per [08 §5.4](../../architecture/08-timing-and-clocks.md). Do not use `std::chrono::system_clock` directly in rover code; `rclcpp::Clock` is the sanctioned interface.

## 4. Checklist before PR

- [ ] Node subclasses `LifecycleNode`
- [ ] All callbacks non-blocking
- [ ] QoS profiles declared as file-scope named constants
- [ ] Launch file does not spawn more than one instance inline (use composition for multi-instance)
- [ ] At least one failure-path test
- [ ] `colcon build` + `colcon test` green
- [ ] `use_sim_time: true` in SITL parameter file

## 5. Troubleshooting

DDS discovery issues: [`../troubleshooting.md §2.2`](../troubleshooting.md). Lifecycle transitions stuck: [`../troubleshooting.md §2.3`](../troubleshooting.md).

## 6. What this guide does NOT cover

- Gazebo plugin authoring — see [`writing-a-gazebo-plugin.md`](writing-a-gazebo-plugin.md).
- Cross-stack integration (ROS 2 ↔ cFS) — see [`../../interfaces/ICD-relay-surface.md`](../../interfaces/ICD-relay-surface.md).
- Hardware-in-the-loop variant of node bring-up — out of scope for Phase B per [05 §11](../../architecture/05-simulation-gazebo.md).
