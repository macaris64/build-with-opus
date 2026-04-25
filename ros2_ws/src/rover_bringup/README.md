# rover_bringup

Top-level launch files and parameter configuration for the SAKURA-II surface rover software stack.

## Purpose

Composes rover packages into a runnable system. Parameters are kept here (not in source packages) so that velocity limits, namespace assignments, and other tunable values can be changed without recompiling any C++ code.

## Contents

```
rover_bringup/
├── launch/
│   └── rover.launch.py     — launches teleop_node as a managed lifecycle node
└── config/
    └── rover_params.yaml   — runtime parameters (velocities, etc.)
```

## Launch

```bash
# Source Space ROS and the workspace install
source /opt/ros/spaceros/setup.bash
source install/setup.bash

# Launch with defaults
ros2 launch rover_bringup rover.launch.py

# Override the parameter file
ros2 launch rover_bringup rover.launch.py \
  params_file:=/path/to/custom_params.yaml
```

## Parameters (`rover_params.yaml`)

| Parameter | Default | Description |
|---|---|---|
| `teleop_node.max_linear_vel` | `1.0` m/s | Hardware velocity ceiling — do not exceed chassis limit |
| `teleop_node.max_angular_vel` | `0.5` rad/s | Hardware angular velocity ceiling |

## Dependencies

| Package | Role |
|---|---|
| `rover_teleop` | Lifecycle teleop node launched by this bringup |
| `launch` / `launch_ros` | ROS 2 launch framework |

## Conventions

- One parameter file per rover class; per-instance variation via launch-argument override (not separate files).
- All nodes are launched as `LifecycleNode` — plain `Node` is banned per `.claude/rules/ros2-nodes.md`.
- Namespace is set at launch time; the package itself is namespace-agnostic.
