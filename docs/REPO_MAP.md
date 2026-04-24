# Repository Map

> Terminology: [GLOSSARY.md](GLOSSARY.md). Coding conventions: [.claude/rules/](../.claude/rules/) (normative — this doc does not restate them).

Per-directory mandate for `/home/aero/Code/build-with-opus`. Mirrors [`CMakeLists.txt`](../CMakeLists.txt) and [`_defs/targets.cmake`](../_defs/targets.cmake). Whenever a new top-level directory is added, update this file and the nav hub in [`README.md`](README.md).

## Top-Level Layout

```
build-with-opus/
├── _defs/                 — Mission-wide compile-time constants and CMake targets
├── apps/                  — cFS / cFE flight-software applications (one dir per app)
├── ros2_ws/               — Space ROS 2 colcon workspace (rovers: land / UAV / cryobot)
├── simulation/            — Gazebo Harmonic plugins (physics, sensors, environment)
├── rust/                  — Cargo workspace: ground segment + FFI bindings
├── docs/                  — This documentation tree (you are here)
├── .claude/               — Claude Code harness: skills, agents, hooks, rules
├── .mcp.json              — MCP server registry (github / sentry / postgres)
├── CMakeLists.txt         — Root C/C++ build; includes _defs/targets.cmake
├── Cargo.toml             — Rust workspace root
├── CLAUDE.md              — Authoritative project instructions for Claude Code
├── README.md              — Repo entry point (quickstart and layer table)
└── .clang-format          — C/C++ style (applied by format-on-save hook)
```

## Segment Ownership

Each top-level code directory maps onto exactly one SAKURA-II segment. This keeps docs per-class (see [architecture/10-scaling-and-config.md](architecture/10-scaling-and-config.md) — planned) so the doc set does not explode when fleet size grows.

| Directory | Segment | Language / Framework | Primary audience |
|---|---|---|---|
| `apps/` | Orbiter flight software + cFS-based subsystem gateways | C17, NASA cFS / cFE, MISRA C:2012 | FSW engineers |
| `ros2_ws/src/` | Surface rovers (land, UAV, cryobot) | C++17, Space ROS 2, `rclcpp_lifecycle::LifecycleNode` | Robotics engineers |
| `simulation/` | Gazebo physics and environment plugins | C++17, Gazebo Harmonic ModelPlugin | Simulation engineers |
| `rust/ground_station/` | Ground segment: TM ingest, TC uplink, CFDP (Class 1 now, Class 2 later via `CfdpProvider` — see [Q-C3](standards/decisions-log.md)), operator UI | Rust stable | Ground ops engineers |
| `rust/cfs_bindings/` | FFI bindings from Rust to cFS headers for ground-side decoders; FSW-adjacent BE↔LE conversion helpers (see [Q-C8](standards/decisions-log.md)) | Rust + `bindgen` | Ground ops / FSW liaison |
| `rust/ccsds_wire/` (planned, Batch B3) | Pure-Rust CCSDS primary/secondary-header + CUC pack/unpack. Sole Rust-side locus for BE wire encoding per [Q-C8](standards/decisions-log.md) | Rust stable | Ground ops engineers |
| `_defs/` | Mission-wide compile-time constants, CMake target list | C headers, CMake | Integration / CM |

The **smallsat relay** (FreeRTOS primary FSW) and **FreeRTOS subsystem MCUs** are not yet present in the tree. They will land under:

- `apps/freertos_relay/` — smallsat-class end-to-end FreeRTOS FSW
- `apps/mcu_<role>/` — one directory per subsystem-MCU class (`mcu_payload`, `mcu_rwa`, `mcu_eps`)

Placement under `apps/` rather than a parallel `freertos/` tree keeps the build graph uniform and lets `_defs/targets.cmake` enumerate them alongside cFS apps. Naming convention: `freertos_*` prefix marks non-cFS FSW so static analysis and lint rules can scope correctly.

## `_defs/` — Mission Configuration

Single source of truth for compile-time mission parameters. Every doc that cites a constant (spacecraft ID, APID allocation, task stack depth) traces back here.

| File | Purpose | Docs that mirror it |
|---|---|---|
| [`_defs/mission_config.h`](../_defs/mission_config.h) | `MISSION_NAME`, `SPACECRAFT_ID`, `SAKURA_II_SCID_BASE`, `SAKURA_II_MAX_PIPES`, `SAKURA_II_TASK_STACK` | [`interfaces/apid-registry.md`](interfaces/apid-registry.md) — SCID must match |
| [`_defs/targets.cmake`](../_defs/targets.cmake) | `MISSION_NAME`, `MISSION_APPS` app list | [`architecture/01-orbiter-cfs.md`](architecture/01-orbiter-cfs.md) (planned) — app inventory must match |
| [`_defs/cfs_app_template.cmake`](../_defs/cfs_app_template.cmake) | `sakura_add_cfs_app()` macro — per-app FSW OBJECT library, CMocka test target, gcov instrumentation (`SAKURA_COVERAGE`), per-app cppcheck target | Phase 17; every `apps/<name>/CMakeLists.txt` invokes this macro |

`MISSION_NAME` was renamed from the template placeholder `SAMPLE_MISSION` to `SAKURA_II` in Phase 11, alongside the `SAKURA_II_SCID_BASE` fleet-allocation anchor; the original SCID value `42U` was retained. Historical note preserved here for changelog-style traceability.

## `apps/` — Flight Software

Each app is a self-contained subdirectory with its own `CMakeLists.txt`. Current content:

```
apps/
└── sample_app/
    ├── CMakeLists.txt
    └── fsw/
        ├── src/           — flight source (C17, MISRA)
        └── unit-test/     — CMocka unit tests
```

Expected growth (per [`ConOps.md`](mission/conops/ConOps.md) asset inventory):

```
apps/
├── sample_app/            — example; kept as reference app
├── orbiter_<function>/    — one dir per orbiter function (cdh, adcs, comm, power, payload)
├── freertos_relay/        — smallsat relay primary FSW (FreeRTOS, not cFS)
└── mcu_<role>/            — FreeRTOS subsystem controllers (payload/rwa/eps)
```

Conventions for FSW code are in [.claude/rules/cfs-apps.md](../.claude/rules/cfs-apps.md) and [.claude/rules/general.md](../.claude/rules/general.md). Per-app design detail belongs in `architecture/01-orbiter-cfs.md` (planned), not in this map.

## `ros2_ws/` — Surface Rovers

Colcon workspace. Build from inside `ros2_ws/` with `colcon build --symlink-install`.

```
ros2_ws/src/
├── rover_bringup/         — launch files, parameter YAML
└── rover_teleop/          — teleop lifecycle node + config
```

Expected growth, organized by rover form factor (not by rover instance — per-instance proliferation is prevented by parameter-driven launch files documented in [architecture/10-scaling-and-config.md](architecture/10-scaling-and-config.md) (planned)):

```
ros2_ws/src/
├── rover_common/          — message types, shared QoS profiles, utility nodes
├── rover_land/            — wheeled rover lifecycle nodes (locomotion, nav)
├── rover_uav/             — aerial rover lifecycle nodes (flight control, state est)
├── rover_cryobot/         — subsurface rover lifecycle nodes (drill, tether, low-BW comms)
├── rover_bringup/         — (existing) composite launch for any rover class
└── rover_teleop/          — (existing) operator interface
```

Conventions are in [.claude/rules/ros2-nodes.md](../.claude/rules/ros2-nodes.md).

## `simulation/` — Gazebo

```
simulation/
└── gazebo_rover_plugin/
    ├── CMakeLists.txt
    ├── include/
    └── src/
```

Each plugin is a `ModelPlugin` subclass. Sensor-noise and radiation/time-shift effects implemented here feed the FSW via the boundary defined in [`interfaces/ICD-sim-fsw.md`](interfaces/ICD-sim-fsw.md) (planned).

## `rust/` — Ground Segment

Cargo workspace; two members today, third landing in Batch B3.

```
rust/
├── ground_station/        — TM ingest, TC uplink, CFDP (CfdpProvider), operator UI
├── cfs_bindings/          — Rust FFI to cFS message definitions (ground-side decoders)
└── ccsds_wire/            — (planned, Batch B3) pure-Rust CCSDS pack/unpack (BE wire)
```

Conventions in [.claude/rules/general.md](../.claude/rules/general.md) (Rust section) and [.claude/rules/security.md](../.claude/rules/security.md). CFDP scope is **Class 1 (unacknowledged) now, Class 2 (acknowledged) later** behind a single `CfdpProvider` trait ([Q-C3](standards/decisions-log.md)); the crate boundary is documented in `architecture/06-ground-segment-rust.md` (planned). Endianness conversion locus is `cfs_bindings` + `ccsds_wire` only ([Q-C8](standards/decisions-log.md)).

## `.claude/` — Harness

Not documented further here; see [`.claude/rules/`](../.claude/rules/) for the authoritative coding conventions and [`CLAUDE.md`](../CLAUDE.md) for the project overview. These docs **cite** those rules; they never restate them.

## `docs/` — This Tree

See [`README.md`](README.md) for the nav hub and audience routing.

## Build Artifacts (gitignored)

- `build/` — CMake output for C/C++ targets
- `target/` — Cargo output for Rust targets
- `ros2_ws/build/`, `ros2_ws/install/`, `ros2_ws/log/` — colcon output
- `.env` — local secrets (never committed; template at `.env.example` once created)
