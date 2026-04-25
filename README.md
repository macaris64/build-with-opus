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

## Quick Start — Docker (recommended)

Requires Docker ≥ 24 with the Compose v2 plugin. No other tools needed.

```bash
make run    # build all images and start every service (cFS, Gazebo, ROS 2, ground station, fault injector)
make logs   # stream combined logs from all services  (Ctrl-C to exit)
make stop   # stop and remove containers (volumes preserved)
make clean  # stop containers and remove named volumes
```

Once the stack is up:

| Service | URL / Address | Protocol | Notes |
|---|---|---|---|
| Ground station operator UI | `http://localhost:8080` | HTTP | `/api/time`, `/api/tc`, telemetry surfaces |
| Ground station telemetry API | `http://localhost:8080/api/time` | HTTP | Liveness probe |
| UDP telemetry ingress | `localhost:10000` | UDP | CCSDS AOS frames from cFS / fault injector |

> First run pulls/builds all images — expect 10–20 minutes. Subsequent runs use the Docker layer cache and start in seconds.

## Quick Start — Native (no Docker)

```bash
# Build C/cFS + simulation plugins + fault injector
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

# Run C unit tests
ctest --test-dir build --output-on-failure

# Build and test Rust
cargo build && cargo test && cargo clippy -- -D warnings

# ROS 2 workspace (requires Space ROS 2 install)
cd ros2_ws && colcon build --symlink-install && colcon test
```

Copy `.env.example` to `.env` before using the Postgres MCP server.

## Using This as a Boilerplate

Forking to start a new mission? See [docs/dev/fork-bootstrap.md](docs/dev/fork-bootstrap.md) for the runbook (rename codename, reset mission config, trim rules/skills, re-run quickstart).

## Documentation

- [docs/README.md](docs/README.md) — full documentation index by audience.
- [docs/dev/quickstart.md](docs/dev/quickstart.md) — 10-minute bring-up from a fresh clone.
- [docs/dev/mcp-setup.md](docs/dev/mcp-setup.md) — how to approve and configure the GitHub / Sentry / Postgres MCP servers.
