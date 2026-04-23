# 10 — Scaling & Configuration

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Bibliography: [../standards/references.md](../standards/references.md). System context: [00-system-of-systems.md](00-system-of-systems.md) §4. APID allocation: [../interfaces/apid-registry.md](../interfaces/apid-registry.md). Build metadata: [../../_defs/targets.cmake](../../_defs/targets.cmake) and [../../_defs/mission_config.h](../../_defs/mission_config.h).

This is the **anti-combinatorial-explosion doc**. It is the single knob that parameterizes SAKURA-II's fleet size. Any documentation or code change that would create per-instance content (e.g. "Orbiter-02's SDD") must instead go through this doc.

Authoritative commitment: **scaling from MVC (1 orbiter + 1 relay + 1 land + 1 UAV + 1 cryobot) to the Scale-5 target (5 orbiters, 5+ land, 3+ UAV, 2+ cryobot, 1 relay) is a configuration change — no new docs, no refactors, no new APIDs.**

## 1. The Four Configuration Surfaces

Per Q-H2, scaling configuration lives in exactly four places. Anywhere else that encodes fleet-size assumptions is a bug.

| # | Surface | Location | Governs |
|---|---|---|---|
| 1 | **Docker compose profiles** | `docker-compose.yml` + `compose/*.yaml` overlays (out of scope for this doc; infra prerequisite) | How many container instances of each class exist per profile (`minimal` / `full` / `scale-5`) |
| 2 | **Mission YAML config** | `_defs/mission.yaml` (new — to be created in Batch B3 with `01-orbiter-cfs.md`) | Per-instance parameters: instance ID, APID block assignment, time-authority role |
| 3 | **cFS build-time C headers** | [`../../_defs/mission_config.h`](../../_defs/mission_config.h) + future `_defs/apids.h` (Batch B3) | Compile-time constants (SCID, max pipes, stack depth) |
| 4 | **ROS 2 launch files** | `ros2_ws/src/rover_bringup/launch/*.launch.py` | Per-rover-instance node parameters (namespace, QoS profile, sim vs flight time) |

These four surfaces are **composable**: compose profiles spin up containers, each container reads mission YAML at startup, FSW within cFS containers uses compile-time constants, and rovers use launch-file parameters. No single asset's full configuration lives in a single file — by design, per separation-of-concerns.

## 2. Compose Profile Matrix

The Docker overlays define three named profiles; the mechanism for creating them is in [`../dev/docker-runbook.md`](../dev/docker-runbook.md) (Batch B5). This doc defines **what each profile contains**.

| Service class | Image | `minimal` | `full` | `scale-5` |
|---|---|---|---|---|
| orbiter | `sakura/cfs-orbiter` | 1 | 1 | 5 |
| relay | `sakura/freertos-relay` | 1 | 1 | 1 |
| mcu_payload | `sakura/freertos-mcu` | 1 | 1 | 5 (1 per orbiter) |
| mcu_rwa | `sakura/freertos-mcu` | 1 | 1 | 5 |
| mcu_eps | `sakura/freertos-mcu` | 1 | 1 | 5 |
| rover_land | `sakura/space-ros-rover` | 1 | 1 | 5 |
| rover_uav | `sakura/space-ros-rover` | 1 | 1 | 3 |
| rover_cryobot | `sakura/space-ros-rover` | 0 | 1 | 2 |
| gazebo | `sakura/gazebo-harmonic` | 1 | 1 | 1 |
| clock_link_model | `sakura/clock-link` | 1 | 1 | 1 |
| ground_station | `sakura/ground-station` | 1 | 1 | 1 |
| **Total containers** | | **9** | **10** | **34** |

Notes:

- **`minimal` omits the cryobot** to keep laptop-class hosts happy; its absence is non-breaking because the ConOps cryobot scenario (SCN-OFF-01) tests run under `full`.
- **One relay for all profiles.** Per [Q-C7](../standards/decisions-log.md), Scale-5 uses a star topology (relay-mediated) — multi-relay topologies are a Phase B+ additive profile extension.
- **Single Gazebo container.** Multiple worlds run as separate Gazebo plugins in the same process — scaling the physics simulator to N processes is a deferred optimization (open item below).
- **Service images are shared across instances.** `rover_land-01` and `rover_land-05` run the same image with different environment variables — see §3.

## 3. Mission YAML Contract

`_defs/mission.yaml` (to be authored in Batch B3) is the **per-instance configuration file**. Its schema is fixed here.

```yaml
mission:
  name: SAKURA_II
  scid: 42                  # mirrors _defs/mission_config.h SPACECRAFT_ID
  t0_epoch: "2026-07-15T00:00:00Z"   # MOI start in UTC; TAI conversion at load time
  light_time_ow_seconds: 600

assets:

  orbiters:
    - id: orbiter-01
      apid_tm_base: 0x100    # block start; +per-app offsets within 0x100–0x17F
      apid_tc_base: 0x180
      instance_id: 1
      time_authority: primary-during-los   # values: ground-only | primary-during-los | slave
      hpsc_cores: 4

  relays:
    - id: relay-01
      apid_tm_base: 0x200
      apid_tc_base: 0x240
      instance_id: 1

  mcus:
    - id: mcu_payload-01
      parent: orbiter-01
      apid_base: 0x280
      bus: spacewire
      instance_id: 1
    # ...similarly for mcu_rwa, mcu_eps

  rovers_land:
    - id: rover_land-01
      apid_tm_base: 0x300
      apid_tc_base: 0x380
      instance_id: 1
      relay_parent: relay-01
      ros_namespace: /rover_land_01

  rovers_uav:
    - id: rover_uav-01
      apid_base: 0x3C0
      instance_id: 1
      relay_parent: relay-01
      ros_namespace: /rover_uav_01

  rovers_cryobot:
    - id: rover_cryobot-01
      apid_tm_base: 0x400
      apid_tc_base: 0x440
      instance_id: 1
      tether_parent: rover_land-01
      ros_namespace: /rover_cryobot_01

fault_injection:
  enabled: true              # flipped off for any flight-build profile
  api_apid_base: 0x540
```

### Schema rules (lint-enforceable)

- Every `apid_*_base` must be inside the allocated block for its class per [`../interfaces/apid-registry.md`](../interfaces/apid-registry.md).
- `instance_id` is 1-indexed per class; must be unique within class and ≤ 255.
- `relay_parent` / `tether_parent` must reference an existing asset id.
- `time_authority` values per [`08-timing-and-clocks.md`](08-timing-and-clocks.md) §3.
- `hpsc_cores` is informational for Phase B; becomes binding when the HPSC target lands.

### Who reads it

| Container | Reads | Uses |
|---|---|---|
| orbiter (cFS) | own orbiter entry | instance_id, APID bases, time authority |
| relay (FreeRTOS) | own relay entry + all rover entries | session routing |
| mcu_* (FreeRTOS) | own entry | bus role, parent orbiter |
| rover_* (ROS 2) | own entry + relay parent | namespace, parameters, QoS |
| gazebo | all entries | spawn model instances |
| clock_link_model | `mission.light_time_ow_seconds` + all `time_authority` | delay tables |
| ground_station (Rust) | all entries | routing, operator-UI asset inventory |

Single file, multiple consumers — composition point for fleet size.

## 4. cFS Compile-Time Headers

Per Q-H1, the HPSC target uses **SMP Linux with cFS running as tasks within a single OS image**. There are no per-core AMP partitions — cFS apps are FreeRTOS-like tasks under Linux pthreads, scheduled by Linux SCHED_FIFO at priorities cFE assigns.

Implications for scaling:

- **One cFS container image per orbiter instance.** Scale-5 runs 5 instances of the same image, each reading a different `orbiter-N` entry from `_defs/mission.yaml`.
- **Compile-time constants are fleet-shared.** [`../../_defs/mission_config.h`](../../_defs/mission_config.h) defines the mission-wide `SAKURA_II_MAX_PIPES`, `SAKURA_II_TASK_STACK`, `SAKURA_II_SCID_BASE`, `SPACECRAFT_ID`, and `MISSION_NAME`. An instance does not get its own header; per-instance SCIDs derive from `SAKURA_II_SCID_BASE` by offset.
- **Per-instance parameters go through YAML**, not through per-instance headers. Fighting this creates a combinatorial header explosion that Phase A explicitly sought to prevent.

A forthcoming `_defs/apids.h` (Batch B3, authored alongside `01-orbiter-cfs.md`) will expose the APID/MID macros consumed by app code. That header is fleet-shared; per-instance disambiguation is runtime (via CFE Instance ID from YAML), not compile-time.

## 5. ROS 2 Launch File Pattern

Every rover launch file takes the same parameter set:

```python
# ros2_ws/src/rover_bringup/launch/rover_land.launch.py
# (schema — to be created in Batch B3 alongside 04-rovers-spaceros.md)

DeclareLaunchArgument("instance_id", default_value="1")
DeclareLaunchArgument("apid_tm_base", default_value="0x300")
DeclareLaunchArgument("apid_tc_base", default_value="0x380")
DeclareLaunchArgument("relay_parent", default_value="relay-01")
DeclareLaunchArgument("ros_namespace", default_value="/rover_land_01")
DeclareLaunchArgument("qos_profile", default_value="rover_default")
```

The docker-compose service for each rover instance passes these as environment variables or `ros2 launch` arguments. No per-instance launch files — one launch file per **class**, parameterized.

## 6. APID Allocation Under Scaling

Per [`../interfaces/apid-registry.md`](../interfaces/apid-registry.md), **APIDs do not multiply with instance count.** The Instance ID field in the CCSDS secondary header (§2 of `08-timing-and-clocks.md`) disambiguates up to 255 instances per APID block.

Worked example for Scale-5:
- 5 orbiters share APID block `0x100`–`0x17F` for TM.
- orbiter-01 HK packets: APID `0x100`, Instance ID = 1.
- orbiter-03 HK packets: APID `0x100`, Instance ID = 3.
- Ground-station routing decodes the Instance ID before assigning the TM point to the right telemetry channel in the operator UI.

The registry thus stays constant across all three profiles.

## 7. Time-Authority Scaling

Per [`08-timing-and-clocks.md`](08-timing-and-clocks.md) §3, the time-authority ladder is:

```
Ground → Orbiter (primary during LOS) → Relay → Rover → Cryobot
```

Under Scale-5 with multiple orbiters, exactly **one** orbiter holds `time_authority: primary-during-los`; the others run `time_authority: slave` and discipline to the primary via cross-link. The primary is picked at mission-config-time (human decision), not dynamically at runtime — this is a Phase B simplification. Dynamic primary election is a Phase B+ concern (tracked below).

## 8. What Changes When Fleet Grows — Checklist

To scale from MVC to Scale-5, a developer:

1. Edits `docker-compose.yml` replica counts per service (or adds instances via overlay file).
2. Adds the new asset entries to `_defs/mission.yaml` with new instance IDs (same APID bases).
3. No C header changes.
4. No new ROS 2 launch files.
5. No new APIDs.
6. No new ICDs.
7. No new architecture docs.
8. No new V&V scenarios (scaling is itself a V&V scenario — see `V&V-Plan.md`, Batch B4).

Failure to honor this checklist is a process bug, not an architectural reality.

## 9. Gate Tests Enforcing the Contract

Not in Phase B scope but scoped here for future linters:

- **mission-yaml validator** (Python, Batch C or later): schema check on `_defs/mission.yaml` including APID-block-membership check against `apid-registry.md`.
- **no-per-instance-artifact linter**: scan `docs/` for file names matching `orbiter-0[2-9]*` and similar; fail CI if found.
- **compose-profile ↔ mission-yaml consistency**: container counts per class match `assets.*` list length.

## 10. Decisions Resolved / Open Items

Resolved:

- **[Q-C7](../standards/decisions-log.md) inter-orbiter topology under Scale-5** — **resolved: star topology (relay-mediated)**. Direct inter-orbiter mesh is out of scope for Phase B; this simplifies routing tables and keeps Scale-5 operating with one relay. Full treatment in [`02-smallsat-relay.md`](02-smallsat-relay.md) (Batch B3, planned). Scale-5 therefore retains one relay per profile (see §2); adding a second relay is a Phase B+ compose-profile extension.

Open:

- **Gazebo multi-world scaling**. Currently one Gazebo container runs one world with all model instances; Scale-5 with 34 total containers may saturate Gazebo physics. If so, split the world per-region (e.g. Mars-north and Mars-south). Deferred until a performance measurement justifies it.
- **Dynamic primary-time-authority election** — Phase B pins authority at config time. Election is a Phase B+ concern, tracked in `08-timing-and-clocks.md` open items.
- **HPSC cross-compilation for Scale-5**. Phase B Docker runs on x86_64 Linux host. Building orbiter images for HPSC (Q-H8) is a separate scaling dimension tracked in [`../dev/build-runbook.md`](../dev/build-runbook.md) (Batch B5).

## 11. What this doc is NOT

- Not an implementation guide for Docker compose — that lives in `dev/docker-runbook.md` (Batch B5).
- Not a CM policy — that lives in `mission/configuration/CM-Plan.md` (Phase C).
- Not a deployment / release engineering doc — release artifact generation lives in `mission/configuration/release-process.md` (Phase C).
