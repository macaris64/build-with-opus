# simulation/ — Gazebo Harmonic Plugins & Worlds

Mars-surrogate simulation: world SDF, per-asset ModelPlugin suite, fault-injection sideband emitter. C++17.

## Layout

```
simulation/
├── gazebo_rover_plugin/    — wheeled-rover drive dynamics (reference plugin)
├── gazebo_uav_plugin/      — (planned) aerial-rover dynamics
├── gazebo_cryobot_plugin/  — (planned) cryobot physics + tether spool
├── gazebo_world_plugin/    — (planned) dust, gravity, irradiance
├── fault_injector/         — (planned) scenario runner → sim-fsw sideband SPPs
├── scenarios/              — (planned) *.yaml fault-injection timelines
└── worlds/                 — (planned) *.sdf mars-surrogate world definitions
```

## Build

Per-plugin CMake, built as part of the top-level C/C++ build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
```

Gazebo Harmonic development headers must be installed on the host (`libgz-sim8-dev` or equivalent for your distro).

## Where to read more

- **Architecture** — [`../docs/architecture/05-simulation-gazebo.md`](../docs/architecture/05-simulation-gazebo.md) (plugin roles, fault_injector, clock model split).
- **Sim ↔ FSW boundary** — [`../docs/interfaces/ICD-sim-fsw.md`](../docs/interfaces/ICD-sim-fsw.md) (APIDs `0x500`–`0x57F`, CRC-16 trailer rule, `CFS_FLIGHT_BUILD` guard).
- **Fault decisions** — [Q-F1, Q-F2 in `../docs/standards/decisions-log.md`](../docs/standards/decisions-log.md).
- **Coding rules** — [`../.claude/rules/general.md`](../.claude/rules/general.md) (C++ universal).

## Canonical plugin shape

Every plugin subclasses `gazebo::ModelPlugin` (or `WorldPlugin`) with `Load()`, `Reset()`, and `OnUpdate()`. `OnUpdate()` must not block and must not allocate on the hot path. Reference: [`gazebo_rover_plugin/include/rover_drive_plugin.h`](gazebo_rover_plugin/include/rover_drive_plugin.h).
