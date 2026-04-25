# HOW_TO_RUN

Quick-reference: what each stack can do right now.

| Stack | What you can do today | What is not yet possible | Jump to |
|---|---|---|---|
| C / cFS (`apps/`) | Build + run unit tests (CMocka / ctest) | Run apps as services — no cFS runtime vendored | [§2](#2-c--cfs-apps) |
| ROS 2 (`ros2_ws/`) | Build workspace; run `teleop_node` via launch file | Multi-rover bringup; Gazebo bridge | [§3](#3-ros-2-workspace) |
| Gazebo (`simulation/`) | Compile the rover drive plugin | Launch any simulation — no world `.sdf` files | [§4](#4-gazebo-simulation-plugin) |
| Rust ground station (`rust/`) | Run binary (scaffold) + 5 self-contained examples | Full pipeline — spawning deferred to Phase 22+ | [§5](#5-rust-ground-station) |
| Combined | ROS 2 + Rust side-by-side (no data exchange yet) | Full integrated stack (cFS + ROS 2 + Gazebo + Rust) | [§6](#6-running-stacks-together) |

---

## 1. Prerequisites

### C / cFS

- CMake ≥ 3.20
- GCC or Clang with C17 support
- `libcmocka-dev` — CMocka unit-test framework
- Optional: `cppcheck` for static analysis

```bash
sudo apt install cmake gcc libcmocka-dev cppcheck
```

### ROS 2

Space ROS 2 (Humble-compatible) with `colcon`. The CI reference environment is the `osrf/space-ros:latest` Docker image, which ships all dependencies pre-installed.

```bash
# Local install (Humble):
source /opt/ros/humble/setup.bash

# Or via Docker (matches CI exactly):
docker pull osrf/space-ros:latest
docker run --rm -it -v "$(pwd)/ros2_ws:/workspace/ros2_ws" osrf/space-ros:latest bash
# inside container:
source /opt/ros/spaceros/setup.bash
```

### Gazebo

`libgz-sim8-dev` (Gazebo Harmonic development headers). If absent, CMake prints a warning and skips the plugin — all other targets still build.

```bash
sudo apt install libgz-sim8-dev
```

### Rust

`rustup` with the stable toolchain. The repo pins the toolchain via `rust-toolchain.toml`; `rustup` picks it up automatically.

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
# Optional extras:
cargo install cargo-audit --locked       # vulnerability scanning
cargo install cargo-tarpaulin --locked   # coverage (Linux x86-64 only)
```

---

## 2. C / cFS Apps

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The top-level `CMakeLists.txt` also attempts to build `simulation/gazebo_rover_plugin`. If Gazebo headers are absent, CMake prints a warning and continues — the cFS unit-test targets are unaffected.

### Run Unit Tests

```bash
ctest --test-dir build --output-on-failure
```

Runs all 9 CMocka test executables: `sample_app`, `orbiter_cdh`, `orbiter_adcs`, `orbiter_comm`, `orbiter_power`, `orbiter_payload`, `mcu_payload_gw`, `mcu_rwa_gw`, `mcu_eps_gw`.

### Static Analysis

```bash
cppcheck --enable=all --std=c17 apps/
```

### Coverage Report

```bash
cmake -B build_cov -DCMAKE_BUILD_TYPE=Debug -DSAKURA_COVERAGE=ON
cmake --build build_cov
ctest --test-dir build_cov --output-on-failure
bash scripts/coverage-gate.sh
```

> **Limitation.** The cFS apps are compiled as `OBJECT` libraries, not standalone executables. Running them as live flight-software services requires a full NASA cFS runtime (cFE + OSAL + PSP), which is not vendored in this repository. Only the CMocka unit tests are runnable today. See [§7 Phase C](#phase-c--add-cfs-runtime-independent-of-ab) for what is needed.

---

## 3. ROS 2 Workspace

### First-Time Setup

```bash
source /opt/ros/humble/setup.bash   # adjust path for spaceros or custom install
```

### Build

```bash
cd ros2_ws
colcon build --symlink-install
source install/setup.bash
```

### Run Tests

```bash
cd ros2_ws
colcon test
colcon test-result --verbose
```

### Launch the Teleop Node

```bash
cd ros2_ws
source install/setup.bash
ros2 launch rover_bringup rover.launch.py
```

`rover.launch.py` starts `teleop_node` as a lifecycle node with parameters from `config/rover_params.yaml` (`max_linear_vel: 1.0 m/s`, `max_angular_vel: 0.5 rad/s`). The node starts in `unconfigured` state.

### Lifecycle Transitions (manual)

```bash
# In a second terminal (ROS 2 sourced):
ros2 lifecycle set /teleop_node configure
ros2 lifecycle set /teleop_node activate
```

### Custom Parameter File

```bash
ros2 launch rover_bringup rover.launch.py params_file:=/path/to/my_params.yaml
```

> **Limitation.** `rover_land`, `rover_uav`, and `rover_cryobot` exist as packages but are not included in `rover.launch.py`. Only `teleop_node` is launched today. A multi-rover bringup launch file and the Gazebo bridge are both planned but not yet implemented. See [§7 Phase B](#phase-b--connect-gazebo--ros-2-depends-on-phase-a).

---

## 4. Gazebo Simulation Plugin

### Build

The rover drive plugin is included in the top-level CMake build (same command as [§2](#build)):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

If `libgz-sim8-dev` is not installed, CMake prints:

```
WARNING: Gazebo not found — rover drive plugin will not be built. Install Gazebo Harmonic to enable.
```

and continues without error. The plugin target is skipped; all cFS unit tests still build and run normally.

> **Limitation — no simulation can be launched today.** The `gazebo_rover_plugin` is a reference stub for wheeled-rover drive dynamics. The following are all still planned and not yet present in the repository:
>
> - World files (`simulation/worlds/` — directory does not exist)
> - UAV, cryobot, world-environment, and fault-injector plugins (`gazebo_uav_plugin`, `gazebo_cryobot_plugin`, `gazebo_world_plugin`, `fault_injector/`)
> - A combined Gazebo + ROS 2 launch file
>
> Attempting to run Gazebo with only this plugin will fail. See [§7 Phase A](#phase-a--enable-gazebo-simulation-independent) for the development plan.

---

## 5. Rust Ground Station

### Build

```bash
cargo build --workspace
```

### Run the Binary

```bash
# Default listen address: 127.0.0.1:10000
cargo run -p ground_station

# Custom address:
cargo run -p ground_station -- 0.0.0.0:10000

# With verbose logging:
RUST_LOG=debug cargo run -p ground_station -- 127.0.0.1:10000
```

Set `RUST_LOG` to `error`, `warn`, `info`, `debug`, or `trace`.

> **Note.** The binary starts, logs the listen address, declares pipeline channel stubs, yields once to the tokio runtime, and exits. This is expected — the pipeline task-spawning logic is scaffolded in `rust/ground_station/src/main.rs:27–41` and lands in Phase 22+. Use the examples below for meaningful output today.

### Run the Five Examples

Each example is self-contained and requires no external services.

```bash
# Full telemetry pipeline: AOS → VcDemux → SppDecoder → ApidRouter (9 packets)
cargo run -p ground_station --example pipeline_demo

# TC uplink session: TcBuilder → COP-1 FOP-1 state machine → SDLP framing
cargo run -p ground_station --example uplink_session

# CFDP Class 1 downlink: Metadata + FileData + EOF PDUs, CRC-32 verify
cargo run -p ground_station --example cfdp_session

# M-File chunked reassembly: in-order, reordered, duplicate, and corrupt scenarios
cargo run -p ground_station --example mfile_reassembly

# cFS ↔ Rust message boundary: MID decode, SB bytes → SpacePacket → routing
cargo run -p ground_station --example cfs_bridge
```

### Test, Lint, Audit

```bash
cargo test --workspace
cargo clippy --workspace -- -D warnings
cargo audit                              # requires cargo-audit
cargo fmt --all -- --check              # formatting check only
cargo tarpaulin --workspace --out Html  # coverage — Linux x86-64 only
```

---

## 6. Running Stacks Together

### What Works Today — ROS 2 + Rust Side-by-Side

Both processes run independently with no data flowing between them yet.

```bash
# Terminal 1 — ROS 2
cd ros2_ws
source install/setup.bash
ros2 launch rover_bringup rover.launch.py

# Terminal 2 — Rust ground station
RUST_LOG=info cargo run -p ground_station
```

### What Does Not Work Yet

| Combination | Blocking requirement |
|---|---|
| Gazebo + any stack | World `.sdf` files must be authored under `simulation/worlds/` |
| Gazebo + ROS 2 | `ros_gz_bridge` integration + a combined bringup launch file |
| cFS apps as services + anything | Full cFS runtime (cFE + OSAL + PSP) vendored or installed |
| Rust ground station receiving live TM | Pipeline task-spawning in `main.rs` (Phase 22+) + cFS AOS output |
| Full integrated stack | All of the above |

---

## 7. Development Roadmap for Full Integration

Phases are sequenced by dependency. Phases A and C are independent and can be started in parallel.

### Phase A — Enable Gazebo Simulation (independent)

1. Author at least one world file in `simulation/worlds/` (e.g., `mars_surrogate.sdf`) with terrain, lighting, and gravity set for Mars.
2. Implement compilable source for `gazebo_uav_plugin` and `gazebo_world_plugin` (headers and stubs do not yet exist).
3. Add a YAML-driven fault-injector scenario runner under `simulation/scenarios/`.
4. Verify: `gazebo simulation/worlds/mars_surrogate.sdf` loads `rover_drive_plugin` without errors.

### Phase B — Connect Gazebo + ROS 2 (depends on Phase A)

1. Add `ros_gz_bridge` (or `gz_ros2_control`) as a workspace dependency in `ros2_ws/`.
2. Add `sim.launch.py` to `rover_bringup/launch/` that starts the Gazebo server, the bridge, and `teleop_node` together.
3. Verify `/cmd_vel` flows from `teleop_node` through the bridge into the rover model joints.

### Phase C — Add cFS Runtime (independent of A/B)

1. Vendor NASA cFS (cFE + OSAL + PSP) — either as a git submodule under `cfs/` or as a system install via `apt`.
2. Update the root `CMakeLists.txt` to use the official cFE `add_cfe_app()` framework instead of the stub `sakura_add_cfs_app()` macro in `_defs/cfs_app_template.cmake`.
3. Create a `cpu1/` mission target directory with startup tables listing which apps load at boot.
4. Verify: all 9 apps register on the cFE Software Bus and send at least one event after startup.

### Phase D — Connect cFS to Rust Ground Station (depends on Phase C)

1. Implement pipeline task-spawning in `rust/ground_station/src/main.rs` (Phase 22+ work already scaffolded at lines 27–41).
2. Configure `orbiter_comm` to write CCSDS AOS frames to a UDP/TCP port matching the ground station default (`127.0.0.1:10000`).
3. Run `cargo run -p ground_station --example pipeline_demo` against the live cFS stream to validate the full AOS → SppDecoder → ApidRouter path.

### Phase E — Full Integrated Stack (depends on A, B, C, D)

1. Write an integration launch script (shell or Python) that starts cFS, the Gazebo world, the ROS 2 bringup, and the Rust ground station in dependency order with readiness checks between each.
2. Validate fault-injection end-to-end: `fault_injector` emits sideband SPPs (APIDs 0x540–0x543); confirm the ground station ApidRouter routes them correctly per the ICD.
3. Add a CI smoke-test job that starts all four stacks inside Docker and waits for health-check signals before reporting success.
