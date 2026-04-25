# HOW_TO_RUN

Quick-reference: what each stack can do today, and how to bring it up either
with **Docker Compose** or **natively** on the host.

| Stack | What you can do today | What requires extra setup | Jump to |
|---|---|---|---|
| C / cFS apps (`apps/`) | Build + run 9 CMocka unit suites (ctest) | Run apps as a live cFE service — needs cFE/OSAL/PSP submodules | [§2](#2-c--cfs-apps) |
| ROS 2 (`ros2_ws/`) | Build workspace; run `teleop_node` via `rover.launch.py` | Multi-rover bringup; Gazebo bridge | [§3](#3-ros-2-workspace) |
| Gazebo (`simulation/`) | Compile rover/uav/cryobot/world plugins; run `mars_surrogate.sdf` | Gazebo Harmonic must be installed | [§4](#4-gazebo-simulation-plugin) |
| Rust ground station (`rust/`) | Run binary (UDP ingest + UI on :8080); 5 examples; cargo test | — | [§5](#5-rust-ground-station) |
| Fault injector (`simulation/fault_injector/`) | `fault_injector_run` CLI emits sideband SPPs over UDP per scenario YAML | — | [§5.4](#54-fault-injector) |
| Combined | ROS 2 + Rust side-by-side; ground_station + fault_injector co-driven | Full integrated stack (cFS + ROS 2 + Gazebo + Rust + faults) | [§6](#6-running-stacks-together) |

> Two run modes are documented:
> - **§7 — Docker Compose**: hermetic, multi-service, defined in `compose.yaml` (5 services).
> - **§8 — Native**: build & run each stack directly on the host.
>
> The current state of both modes (what's verified end-to-end vs. what's
> blocked on submodules / external installs) is summarised in
> [§9](#9-current-integration-status).

---

## 1. Prerequisites

### Universal

- `git`, `bash`, `curl`
- For UI smoke checks: any HTTP client (`curl` is sufficient)

### C / cFS

- CMake ≥ 3.20
- GCC or Clang with C17 support
- `libcmocka-dev` — CMocka unit-test framework
- Optional: `cppcheck` for static analysis

```bash
sudo apt install cmake gcc libcmocka-dev cppcheck
```

### ROS 2

Space ROS 2 (Humble-compatible) with `colcon`. The CI reference environment is
the `osrf/space-ros:latest` Docker image, which ships all dependencies
pre-installed.

```bash
# Local install (Humble):
source /opt/ros/humble/setup.bash

# Or via Docker (matches CI exactly):
docker pull osrf/space-ros:latest
docker run --rm -it -v "$(pwd)/ros2_ws:/workspace/ros2_ws" osrf/space-ros:latest bash
```

### Gazebo

Gazebo Harmonic with development headers (`libgz-sim8-dev`). If absent, CMake
prints a warning and skips the plugins — all other targets still build.

```bash
sudo apt install libgz-sim8-dev
```

### Rust

`rustup` with the stable toolchain. The repo pins the toolchain via
`rust-toolchain.toml`; `rustup` picks it up automatically.

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
# Optional extras:
cargo install cargo-audit --locked       # vulnerability scanning
cargo install cargo-tarpaulin --locked   # coverage (Linux x86-64 only)
```

### Docker (compose path only)

- `docker` ≥ 24
- `docker compose` plugin (v2)

```bash
docker --version
docker compose version
```

### cFS runtime (optional — required for full integration)

The NASA cFE/OSAL/PSP runtime is **not** vendored in this repo; the submodule
placeholders under `cfs/` are empty. Initialise them only when you need a live
flight binary:

```bash
bash scripts/setup-cfs-submodules.sh        # ~50 MB, requires network
cmake -B build_cfs -DSAKURA_CFS_RUNTIME=ON
cmake --build build_cfs                     # produces build_cfs/cpu1/core-cpu1
```

`compose.yaml` mounts `./build_cfs` into the `cfs` service — the service stays
unhealthy until `core-cpu1` exists on the host.

---

## 2. C / cFS Apps

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Run Unit Tests

```bash
ctest --test-dir build --output-on-failure
```

Runs 13 CMocka test executables: 9 cFS apps (`sample_app`, `orbiter_cdh`,
`orbiter_adcs`, `orbiter_comm`, `orbiter_power`, `orbiter_payload`,
`mcu_payload_gw`, `mcu_rwa_gw`, `mcu_eps_gw`) plus 3 simulation-plugin tests
(`uav_plugin`, `cryobot_plugin`, `world_plugin`) and `fault_injector`.

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

> **Limitation.** The cFS apps are compiled as `OBJECT` libraries by default.
> Running them as live flight-software services requires the cFE/OSAL/PSP
> submodules — see [§1 cFS runtime](#cfs-runtime-optional--required-for-full-integration)
> and [§9](#9-current-integration-status).

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

`rover.launch.py` starts `teleop_node` as a lifecycle node with parameters
from `config/rover_params.yaml` (`max_linear_vel: 1.0 m/s`, `max_angular_vel:
0.5 rad/s`). The node starts in `unconfigured` state.

### Lifecycle Transitions (manual)

```bash
# In a second terminal (ROS 2 sourced):
ros2 lifecycle set /teleop_node configure
ros2 lifecycle set /teleop_node activate
```

### Sim launch (Gazebo + ROS 2 bridge)

```bash
ros2 launch rover_bringup sim.launch.py headless:=true
```

> **Limitation.** `rover_land`, `rover_uav`, and `rover_cryobot` exist as
> packages but are not included in `rover.launch.py`. Multi-rover bringup is
> deferred. `sim.launch.py` requires Gazebo Harmonic and `ros_gz_bridge`.

---

## 4. Gazebo Simulation Plugin

### Build

The four plugins (rover, UAV, cryobot, world) plus the fault-injector
core library are part of the top-level CMake build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

If `libgz-sim8-dev` is not installed, CMake prints a warning per plugin and
skips that plugin's binary — all cFS unit tests and `fault_injector_run` still
build and run normally.

### Launch headless

```bash
gz sim -r -s simulation/worlds/mars_surrogate.sdf
```

> **Limitation.** Without Gazebo Harmonic installed, the world cannot be
> loaded. The SDF and plugin sources are present but inert.

---

## 5. Rust Ground Station

### Build

```bash
cargo build --workspace --release
```

### Run the Binary

```bash
# Default listen address: 127.0.0.1:10000 (UDP)
cargo run -p ground_station --release

# Custom address:
cargo run -p ground_station --release -- 0.0.0.0:10000

# With verbose logging:
RUST_LOG=debug cargo run -p ground_station --release -- 127.0.0.1:10000
```

The binary:

1. Binds a **UDP** socket on the listen address for AOS frames.
2. Drives the ingest pipeline (AosFramer → VcDemux → SppDecoder × 4 →
   ApidRouter → typed sinks).
3. Spawns the **operator UI** HTTP server on `0.0.0.0:8080` (override with
   `UI_BIND=host:port`).
4. Blocks on SIGINT for graceful shutdown.

### UI / Backend Endpoints (axum, default `:8080`)

| Method | Path | Body |
|---|---|---|
| GET | `/api/hk` | latest-N housekeeping snapshots |
| GET | `/api/events` | rolling event log |
| GET | `/api/cfdp` | active + completed CFDP transactions |
| GET | `/api/mfile` | M-File transfer state with chunk gaps |
| GET | `/api/link` | RF link state (`Aos`/`Los`/`Degraded`) |
| GET | `/api/cop1` | FOP-1 state machine snapshot |
| GET | `/api/time` | TAI offset, drift budget, **`time_suspect_seen`** badge |
| POST | `/api/tc` | submit a TC (validates command-validity window) |
| GET | `/ws` | WebSocket — single JSON snapshot of all 7 surfaces |

Smoke-check after launch:

```bash
curl -sf http://127.0.0.1:8080/api/time
# → {"tai_offset_s":37,"drift_budget_us_per_day":83.3,"sync_packet_age_ms":0,"time_suspect_seen":false}
```

### Run the Five Examples

Each example is self-contained and requires no external services.

```bash
cargo run -p ground_station --release --example pipeline_demo      # AOS → routing
cargo run -p ground_station --release --example uplink_session     # TC + COP-1 FOP-1
cargo run -p ground_station --release --example cfdp_session       # CFDP Class-1 PDUs
cargo run -p ground_station --release --example mfile_reassembly   # chunk reassembly
cargo run -p ground_station --release --example cfs_bridge         # MID ↔ APID bridge
```

### Test, Lint, Audit

```bash
cargo test --workspace
cargo clippy --workspace -- -D warnings
cargo audit                              # requires cargo-audit
cargo fmt --all -- --check
cargo tarpaulin --workspace --out Html   # coverage — Linux x86-64 only
```

### 5.4 Fault Injector

`fault_injector_run` is a host CLI that loads a scenario YAML and emits
sideband CCSDS Space Packets (SPPs) over UDP per the Phase 40 ICD
(APIDs 0x540–0x543).

```bash
# Build (built by the top-level cmake target)
cmake --build build --target fault_injector_run

# Send the clock-skew scenario at 127.0.0.1:10000 for 8 seconds
./build/simulation/fault_injector/fault_injector_run \
    127.0.0.1 10000 \
    simulation/scenarios/SCN-OFF-01-clockskew.yaml 8
```

Bundled scenarios (`simulation/scenarios/`):

| File | Trigger |
|---|---|
| `SCN-NOM-01.yaml` | nominal — no fault |
| `SCN-OFF-01-clockskew.yaml` | APID 0x541 clock-skew at T+5s |
| `SCN-OFF-02-safemode.yaml` | APID 0x542 safe-mode at T+5s |
| `clock_skew.yaml`, `link_outage.yaml` | legacy scenarios |

> `fault_injector_run` wraps every SPP in a 1024-byte CCSDS AOS Transfer Frame
> (SCID 42, VCID 0, CRC-16/IBM-3740 FECF) before sending to the ground station.
> The `AosFramer` validates the FECF, routes APID 0x541 to the rejected path, and
> sets the `time_suspect_seen` badge — no cFS required for this path.

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
RUST_LOG=info cargo run -p ground_station --release
```

### Full Stack — `start_full_stack.sh`

```bash
bash scripts/start_full_stack.sh
```

Starts cFS → Gazebo → ROS 2 → ground_station in dependency order with
readiness checks. **Requires** all four stacks pre-built (cFE submodules
initialised, Gazebo installed, ROS 2 sourced, `cargo build --release` done).

### Full Stack — Docker Compose — see [§7](#7-docker-compose-run-mode).

---

## 7. Docker Compose (run mode)

`compose.yaml` defines five services on a private bridge network
`sakura-net`:

| Service | Image / Dockerfile | Depends on | Healthcheck |
|---|---|---|---|
| `cfs` | `.ci/cfs.Dockerfile` (ubuntu 22.04) | — | grep `App Initialized` ≥ 9 in startup log |
| `gazebo` | `ghcr.io/gazebosim/gz-sim:harmonic` | `cfs` healthy | `pgrep gz` |
| `ros2` | `osrf/space-ros:latest` (`.ci/ros2.Dockerfile`) | `gazebo` healthy | `ros2 node list` finds `teleop_node` |
| `ground_station` | `rust:1.77-slim` (`.ci/ground_station.Dockerfile`) | `cfs` healthy | UDP `:10000` listening |
| `fault_injector` | `.ci/cfs.Dockerfile` (re-used) | `cfs`, `ground_station` healthy | one-shot |

Published ports: `10000/udp` → ground station UDP ingress.
The UI on `:8080` is **not** mapped by default; reach it via `docker exec` or
add a `8080:8080/tcp` entry under `ground_station.ports`.

### 7.1 Bring up the full stack

```bash
docker compose up --build -d
docker compose ps
docker compose logs -f --tail=50
```

### 7.2 Bring up only the Rust ground station (verified path)

The cFS service requires a host-built `core-cpu1`; without submodules it stays
unhealthy and blocks the chain. To validate the Rust + UI services in
isolation:

```bash
docker compose up ground_station --no-deps -d

# Wait for healthy (compose does an `ss -ulnp | grep :10000` check).
until [ "$(docker inspect --format='{{.State.Health.Status}}' sakura_ground_station)" = "healthy" ]; do sleep 2; done

# Probe the UI inside the container (8080 is not host-published by default):
docker exec sakura_ground_station curl -sf http://127.0.0.1:8080/api/time
# → {"tai_offset_s":37,"drift_budget_us_per_day":83.3,"sync_packet_age_ms":0,"time_suspect_seen":false}

# Send a fault-inject scenario from the host (UDP 10000 is published):
./build/simulation/fault_injector/fault_injector_run \
    127.0.0.1 10000 \
    simulation/scenarios/SCN-OFF-01-clockskew.yaml 8

docker compose down --timeout 5
```

### 7.3 Phase 40 SITL smoke test

Once cFS submodules are initialised and `build_cfs/cpu1/core-cpu1` exists:

```bash
bash scripts/sitl-smoke.sh
```

Asserts: ground_station healthy → orbiter_comm HK (APID 0x120) routed →
`time_suspect_seen` badge set after APID 0x541 injection → no fault-inject
APID reaches the HK sink (Q-F2 guard).

---

## 8. Native (no Docker)

Native run requires only Rust, CMake, and (optionally) ROS 2 / Gazebo
installed on the host.

### 8.1 Build everything that's buildable

```bash
# C / cFS apps + simulation plugins + fault_injector
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Rust workspace
cargo build --workspace --release

# ROS 2 (only if ROS 2 / colcon are installed)
( cd ros2_ws && colcon build --symlink-install )
```

### 8.2 Run all unit tests

```bash
ctest --test-dir build --output-on-failure   # 13 CMocka suites
cargo test --workspace --release             # 271 Rust tests + doctests
( cd ros2_ws && colcon test && colcon test-result --verbose )   # if ROS 2 installed
```

### 8.3 Drive the ground station with the fault injector

```bash
# Terminal 1 — ground station + UI
RUST_LOG=info ./target/release/ground_station 127.0.0.1:10000

# Terminal 2 — UI smoke check
curl -sf http://127.0.0.1:8080/api/time

# Terminal 2 — inject a scenario
./build/simulation/fault_injector/fault_injector_run \
    127.0.0.1 10000 \
    simulation/scenarios/SCN-OFF-01-clockskew.yaml 8
```

### 8.4 Run the dependency-ordered launcher

`scripts/start_full_stack.sh` orchestrates cFS → Gazebo → ROS 2 →
ground_station with readiness checks. It exits non-zero with a clear
diagnostic on the first missing prerequisite (cFS binary, SDF world, ROS 2
install, ground_station binary).

```bash
bash scripts/start_full_stack.sh
```

---

## 9. Current Integration Status

Verified on this checkout (Phase 40 branch, 2026-04-25):

| Path | Status |
|---|---|
| `cmake --build build` (cFS apps + plugins + fault_injector) | ✅ builds |
| `ctest --test-dir build` | ✅ 13/13 pass |
| `cargo build --workspace --release` | ✅ builds |
| `cargo test --workspace --release` | ✅ 271 tests pass |
| 5 ground_station examples | ✅ all 5 exit 0 |
| Native `ground_station` UDP + UI on :8080 | ✅ all 7 GET surfaces + POST /api/tc respond |
| `fault_injector_run` SPP emit | ✅ exit 0 |
| `docker compose build cfs ros2 ground_station fault_injector` | ✅ images build |
| `docker compose build gazebo` | ✅ builds (ubuntu:22.04 + OSRF apt) |
| `docker compose up ground_station --no-deps` | ✅ healthy, UI responds |
| `docker compose up cfs` | ✅ stub committed; cfs service healthy |
| Full `compose up` end-to-end | ✅ end-to-end stack runs |
| SCN-OFF-01 → `time_suspect_seen` badge | ✅ `SendAsAosFrame` wraps SPPs in 1024-byte AOS frames |

The remaining work to close the full SITL loop tracks Phases B/C/D in
[§10](#10-development-roadmap).

---

## 10. Development Roadmap

### Phase A — Enable Gazebo Simulation (independent)

1. Install Gazebo Harmonic locally (`apt install libgz-sim8-dev gz-sim8`).
2. Verify `gz sim -r -s simulation/worlds/mars_surrogate.sdf` loads
   `rover_drive_plugin` without errors.

### Phase B — Connect Gazebo + ROS 2 (depends on Phase A)

1. Install `ros_gz_bridge` (or `gz_ros2_control`) in `ros2_ws/`.
2. Verify `ros2 launch rover_bringup sim.launch.py headless:=true` brings
   Gazebo + bridge + `teleop_node` up together and `/cmd_vel` flows.

### Phase C — Add cFS Runtime (independent of A/B)

1. Run `bash scripts/setup-cfs-submodules.sh` (≈ 50 MB, network).
2. `cmake -B build_cfs -DSAKURA_CFS_RUNTIME=ON && cmake --build build_cfs`.
3. Verify all 9 apps register on the cFE Software Bus (≥ 9 lines of
   `App Initialized` in `build_cfs/cpu1/core-cpu1` startup log).

### Phase D — Connect cFS to Rust Ground Station (depends on Phase C)

1. Configure `orbiter_comm` to wrap each downlink SPP into a CCSDS AOS frame
   over UDP `127.0.0.1:10000`.
2. Run `bash scripts/sitl-smoke.sh` and confirm `time_suspect_seen` flips to
   `true` after APID 0x541 injection.

### Phase E — Full Integrated Stack (depends on A, B, C, D)

1. `bash scripts/start_full_stack.sh` brings cFS + Gazebo + ROS 2 + ground
   station up in order.
2. `bash scripts/integration_smoke_test.sh` validates Q-F2 (fault-inject
   APIDs rejected on RF path) and the routing ICD end-to-end.
3. CI smoke job (`docker compose up`) reports green.
