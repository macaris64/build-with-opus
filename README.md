# space-systems-boilerplate

Claude Code boilerplate for mission-critical space software. Demonstrates every Claude Code feature — skills, hooks, subagents, MCP servers, rules, output styles — targeting NASA cFS flight software, Space ROS 2 nodes, Gazebo simulation, and Rust ground-support tools.

## What's Inside

| Layer | Directory | Technology |
|---|---|---|
| Flight Software | `apps/` | C17, NASA cFS, MISRA C:2012 |
| Robotics | `ros2_ws/` | Space ROS 2, ament/colcon, C++17 |
| Simulation | `simulation/` | Gazebo Harmonic, ModelPlugin |
| Ground Tools | `rust/` | Rust stable, Cargo workspace |
| Mission Config | `_defs/` | CMake targets, compile-time constants |

## Quick Start

```bash
# Build C/cFS
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

# Run C unit tests
ctest --test-dir build --output-on-failure

# Build and test Rust
cargo build && cargo test && cargo clippy -- -D warnings

# ROS 2 workspace (requires Space ROS 2 install)
cd ros2_ws && colcon build --symlink-install && colcon test
```

Copy `.env.example` to `.env` before using the Postgres MCP server.
