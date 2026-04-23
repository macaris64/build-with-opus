# How-To: Write a Gazebo Harmonic Plugin

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Reference exemplar: [`simulation/gazebo_rover_plugin/`](../../../simulation/gazebo_rover_plugin/). Rules: [`../../../.claude/rules/general.md`](../../../.claude/rules/general.md). Architecture: [`../../architecture/05-simulation-gazebo.md`](../../architecture/05-simulation-gazebo.md). Sim ↔ FSW: [`../../interfaces/ICD-sim-fsw.md`](../../interfaces/ICD-sim-fsw.md).

This guide walks adding a new Gazebo plugin under [`simulation/`](../../../simulation/). It is a signpost; architecture rationale lives in [05](../../architecture/05-simulation-gazebo.md), sim-side SPP emission rules live in [ICD-sim-fsw.md](../../interfaces/ICD-sim-fsw.md).

## 1. When this applies

Follow this guide when authoring:

- A new `ModelPlugin` (per-asset physics/dynamics) — e.g. UAV flight, cryobot drilling.
- A new `WorldPlugin` (global effects) — e.g. dust, gravity anomaly.
- The `fault_injector` plugin/sidecar or its successors.

Do **not** use this guide for:
- ROS 2 nodes — see [`launching-a-ros2-node.md`](launching-a-ros2-node.md).
- FSW code consuming the sim sideband — see [`ICD-sim-fsw.md`](../../interfaces/ICD-sim-fsw.md).

## 2. Prerequisites

- Gazebo Harmonic installed and launchable on your host ([`../quickstart.md §1`](../quickstart.md)). **Not Classic** — APIs differ; see [`../troubleshooting.md §4.3`](../troubleshooting.md).
- [`rover_drive_plugin.h`](../../../simulation/gazebo_rover_plugin/include/rover_drive_plugin.h) read — it's the canonical shape.

## 3. Steps

### 3.1 Create the plugin directory

Per [05 §6](../../architecture/05-simulation-gazebo.md):

```
simulation/<plugin_name>/
├── CMakeLists.txt
├── include/
│   └── <plugin_name>.h
└── src/
    └── <plugin_name>.cpp
```

Copy `gazebo_rover_plugin/` as a starting point. Each plugin builds independently via its own `CMakeLists.txt`.

### 3.2 Subclass and register

Canonical ModelPlugin shape (from [05 §3.1](../../architecture/05-simulation-gazebo.md)):

```cpp
class MyPlugin : public ModelPlugin {
public:
    void Load(physics::ModelPtr model, sdf::ElementPtr sdf) override;
    void Reset() override;
private:
    void OnUpdate();
    physics::ModelPtr     model_;
    event::ConnectionPtr  update_connection_;
};
GZ_REGISTER_MODEL_PLUGIN(MyPlugin)
```

The `GZ_REGISTER_MODEL_PLUGIN(...)` macro at file scope is mandatory; without it Gazebo can't find the plugin at load. If Gazebo reports "Failed to load plugin," check this first ([`../troubleshooting.md §4.1`](../troubleshooting.md)).

### 3.3 `Load()` — pre-reserve, read SDF

In `Load()`:

- Parse SDF parameters from the `<plugin>` element. Use named constants, not magic literals ([`.claude/rules/general.md`](../../../.claude/rules/general.md)).
- Pre-reserve any `std::vector` that will grow in `OnUpdate()` — **no allocation on the hot path**.
- Cache references to joints, sensors, and ROS 2 publishers. Do **not** re-construct per tick.
- Register the update hook:
  ```cpp
  update_connection_ = event::Events::ConnectWorldUpdateBegin(
      std::bind(&MyPlugin::OnUpdate, this));
  ```

### 3.4 `OnUpdate()` — hot-path discipline

Per [05 §3.1](../../architecture/05-simulation-gazebo.md) and [`.claude/rules/general.md`](../../../.claude/rules/general.md):

- Must not block. No file I/O, no socket `write()` in sync mode, no mutex held longer than a tick.
- Must not allocate. Pre-reserve in `Load()`.
- Publish ROS 2 messages via the cached node pointer.
- If the plugin emits sim-fsw sideband SPPs (APID block `0x500`–`0x57F`), it does so per [ICD-sim-fsw.md §1](../../interfaces/ICD-sim-fsw.md) with a trailing **CRC-16/CCITT-FALSE** on the user-data.

### 3.5 SDF parameter discipline

SDF `<plugin>` elements are the plugin's only runtime config. Per [05 §9](../../architecture/05-simulation-gazebo.md), per-instance values come from `_defs/mission.yaml` translated into SDF at launch — plugins themselves never read YAML.

### 3.6 Build

Plugin binary lands under `build/simulation/<plugin_name>/lib<plugin_name>.so`. From repo root:

```bash
cmake --build build --target <plugin_name>
```

Gazebo finds the `.so` via `LD_LIBRARY_PATH` or an absolute path in the SDF `filename=` attribute. If load fails, see [`../troubleshooting.md §4.1`](../troubleshooting.md).

### 3.7 Tests

Per [05 §7](../../architecture/05-simulation-gazebo.md): minimum gate is a "loads without crash" test exercising `Load()` + `Reset()` + 10 `OnUpdate()` iterations with a synthetic model pointer. Use Gazebo's gtest integration or a fixture with a mock physics engine.

### 3.8 SITL-only rule

If the plugin emits sim-fsw sideband SPPs (`0x540`–`0x543` fault injection, etc.), it is by construction SITL-only per [05 §4.4](../../architecture/05-simulation-gazebo.md) — `CFS_FLIGHT_BUILD` makes the APID block compile-time unreachable on the FSW side. The plugin itself doesn't need a flight-build guard (it's simulation code), but the consumer discipline matters.

## 4. Checklist before PR

- [ ] `GZ_REGISTER_MODEL_PLUGIN(...)` at file scope
- [ ] `OnUpdate()` is non-blocking + no-alloc
- [ ] SDF parameters read in `Load()`, not per-tick
- [ ] "Loads without crash" test exists
- [ ] CRC-16 trailer if emitting sim-fsw sideband SPPs
- [ ] Plugin builds clean with `-Wall -Wextra -Werror -pedantic`

## 5. Troubleshooting

Plugin fails to load: [`../troubleshooting.md §4.1`](../troubleshooting.md). Physics lag from `OnUpdate()`: [`../troubleshooting.md §4.2`](../troubleshooting.md). Harmonic vs Classic confusion: [`../troubleshooting.md §4.3`](../troubleshooting.md).

## 6. What this guide does NOT cover

- FSW-side sim-adapter app — see [`../../architecture/01-orbiter-cfs.md`](../../architecture/01-orbiter-cfs.md) and [`../../interfaces/ICD-sim-fsw.md`](../../interfaces/ICD-sim-fsw.md).
- Scenario YAML authoring — see [`../simulation-walkthrough.md`](../simulation-walkthrough.md) and [V&V-Plan §8](../../mission/verification/V&V-Plan.md).
- Hardware-in-the-loop variant — out of scope per [05 §11](../../architecture/05-simulation-gazebo.md).
