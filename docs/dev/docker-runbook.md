# Docker Runbook — Target-State Design Spec

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Source of truth for the compose-profile matrix: [`../architecture/10-scaling-and-config.md`](../architecture/10-scaling-and-config.md). Per-instance parameters: `_defs/mission.yaml` (planned per [10 §3](../architecture/10-scaling-and-config.md)). Build commands: [`../../CLAUDE.md`](../../CLAUDE.md). Simulation bring-up: [`simulation-walkthrough.md`](simulation-walkthrough.md).

> **[Phase B]** — this doc is a **target-state design spec**, not an executable runbook. As of this writing there is no `Dockerfile`, no `docker-compose.yml`, and no `compose/*.yaml` overlay in the repo. The commands below describe what the orchestration layer will do once the Phase-B infra PR lands. Authoring those artefacts is out of scope for this doc.

## 1. Scope and status

Phase B defers Docker orchestration per [`quickstart.md §8`](quickstart.md). This runbook locks the **target state** so that when the compose infrastructure lands, its shape is a translation exercise — not a design decision.

What this doc defines:
- The container set per compose profile (from [10 §2](../architecture/10-scaling-and-config.md)).
- The env-var contract between compose and each container.
- Start / stop / rebuild discipline.
- Who reads `_defs/mission.yaml` and when.

What this doc does **not** do:
- Author `Dockerfile` or `docker-compose.yml` content.
- Specify image-registry policy — that's a Phase C CM artefact (`mission/configuration/release-process.md`).
- Size host resources — compose profiles are designed for laptop-class hosts (`minimal`) up to ~34 containers (`scale-5`, [10 §2](../architecture/10-scaling-and-config.md)).

## 2. Compose profile inventory

From [10 §2](../architecture/10-scaling-and-config.md) — do not restate per [`../README.md`](../README.md) conventions. The target compose file implements the three profiles:

| Profile | Use | Container count |
|---|---|---|
| `minimal` | Laptop-class demo; no cryobot | 9 |
| `full` | Complete MVC (all five rover classes + both orbiter-class assets) | 10 |
| `scale-5` | 5 orbiters + multiple surface assets | 34 |

## 3. Nine-container `minimal` profile (target)

| Container | Role | Image (target name) | Reads mission.yaml key |
|---|---|---|---|
| `orbiter-01` | cFS flight software | `sakura/cfs-orbiter` | `assets.orbiters[0]` |
| `relay-01` | FreeRTOS relay | `sakura/freertos-relay` | `assets.relays[0]` |
| `mcu_payload-01` | FreeRTOS MCU | `sakura/freertos-mcu` (role=payload) | `assets.mcus[?].role=="payload"` |
| `mcu_rwa-01` | FreeRTOS MCU | `sakura/freertos-mcu` (role=rwa) | `assets.mcus[?].role=="rwa"` |
| `rover_land-01` | ROS 2 lifecycle nodes | `sakura/space-ros-rover` (class=land) | `assets.rovers_land[0]` |
| `rover_uav-01` | ROS 2 lifecycle nodes | `sakura/space-ros-rover` (class=uav) | `assets.rovers_uav[0]` |
| `gazebo` | Gazebo Harmonic + plugins | `sakura/gazebo-harmonic` | `assets.*` (spawn table) |
| `clock_link_model` | Light-time + drop/skew sim | `sakura/clock-link` | `mission.light_time_ow_seconds`, `assets.*.time_authority` |
| `ground_station` | Rust TM/TC + CFDP + UI | `sakura/ground-station` | `assets.*` (routing, UI) |

The `minimal` profile omits `mcu_eps`, `rover_cryobot`, and scale-5 duplicates per [10 §2](../architecture/10-scaling-and-config.md) — nine containers total.

`full` adds `mcu_eps-01` and `rover_cryobot-01` (→ 10 containers). `scale-5` duplicates per the [10 §2](../architecture/10-scaling-and-config.md) matrix (→ 34 containers).

## 4. Env-var contract

Each container reads a common set of env vars at entrypoint, injected by compose:

| Env var | Source | Consumer |
|---|---|---|
| `MISSION_YAML` | Compose mount of `_defs/mission.yaml` | all |
| `ASSET_ID` | Compose overrides per instance (e.g. `orbiter-01`) | container to find its own entry in `MISSION_YAML` |
| `LIGHT_TIME_OW_SECONDS` | `mission.light_time_ow_seconds` from YAML | `clock_link_model` |
| `ROS_DOMAIN_ID` | Compose network config | ROS 2 containers |
| `LD_LIBRARY_PATH` | Image build | Gazebo plugin discovery |
| `DATABASE_URL` | Operator `.env` (not compose) | `ground_station` (optional Postgres telemetry back-end) |

Rule: **the env-var set is the compose-level contract**, the YAML is the *content*. Adding a new env var requires updating this table; the YAML schema itself is owned by [10 §3](../architecture/10-scaling-and-config.md).

## 5. Target workflow

Once the infra lands, the operator workflow is:

```bash
# From repo root
docker compose --profile minimal up            # [Phase B]
# or
docker compose --profile full up               # [Phase B]
# or
docker compose --profile scale-5 up            # [Phase B]
```

Expected bring-up sequence (post-implementation):
1. `clock_link_model` starts first (all time-authority consumers wait on it).
2. `orbiter-01` starts, registers cFE apps, enters runtime.
3. `relay-01` / MCUs / rovers start (order doesn't matter once `clock_link_model` is live).
4. `gazebo` starts with SDF-loaded world; publishes `/clock` from `clock_link_model`.
5. `ground_station` starts, binds UI on `http://localhost:8080`.

Teardown:
```bash
docker compose --profile minimal down          # [Phase B]
```

Clean rebuild of a single container (per [`build-runbook.md`](build-runbook.md)):
```bash
docker compose build --no-cache <service>      # [Phase B]
docker compose up --force-recreate <service>   # [Phase B]
```

## 6. Cross-refs to other runbooks

- **Builds that feed containers**: [`build-runbook.md`](build-runbook.md).
- **Scenario replay via fault injection**: [`simulation-walkthrough.md`](simulation-walkthrough.md). Scenarios from `simulation/scenarios/*.yaml` go through the `clock_link_model` and `gazebo` containers.
- **Mission-config edits**: [10 §3](../architecture/10-scaling-and-config.md) schema; edit `_defs/mission.yaml`, `docker compose up` re-reads.
- **Secrets**: `DATABASE_URL` is in `.env` (gitignored). See [`mcp-setup.md`](mcp-setup.md) for the Postgres MCP integration.
- **Troubleshooting**: the Claude Code harness section of [`troubleshooting.md §5`](troubleshooting.md) covers MCP-side gotchas.

## 7. Network topology (target)

| Container | Listens on | Connects to |
|---|---|---|
| `ground_station` | `:8080` (UI) | `orbiter-01` over internal compose network |
| `orbiter-01` | — | `relay-01` (cross-link), MCUs (bus), `clock_link_model` |
| `relay-01` | — | `orbiter-01`, rovers |
| rovers | — | `relay-01`, `gazebo` (sim bridge) |
| `gazebo` | — | `clock_link_model`, ROS 2 DDS |
| `clock_link_model` | — | all (time + link effects) |

DDS discovery uses multicast on the user-defined compose bridge network. Non-default `ROS_DOMAIN_ID` isolates parallel compose stacks on the same host.

## 8. Flight-build vs SITL

The compose orchestration is **SITL-only** by design. `CFS_FLIGHT_BUILD` is unset in all three profiles; sim-fsw APIDs `0x500`–`0x57F` are reachable per [ICD-sim-fsw.md §5](../interfaces/ICD-sim-fsw.md). There is no flight-build compose profile in Phase B — HPSC cross-build is deferred per [Q-H8](../standards/decisions-log.md).

## 9. Open items

Tracked for Phase B infra PR and Phase C:

- **Author `docker-compose.yml` + `compose/*.yaml` overlays.** Primary Phase-B infra work; out of scope for this doc.
- **Per-image Dockerfiles.** One per `sakura/*` image in §3. Phase-B infra work.
- **Image-registry policy.** Where images are pushed, retention, signing — Phase C (`mission/configuration/release-process.md`).
- **HPSC cross-build compose profile.** Deferred per [Q-H8](../standards/decisions-log.md); lands with the cross-toolchain.
- **Multi-world Gazebo split.** Scale-5 may saturate single-process Gazebo; deferred per [10 §10](../architecture/10-scaling-and-config.md).
- **Container health-checks.** Per-service `HEALTHCHECK` stanzas for orchestration readiness gates — Phase C.

## 10. What this runbook is NOT

- Not a `docker-compose.yml` file. Target artefact; Phase-B infra PR.
- Not a scaling architecture. That's [10](../architecture/10-scaling-and-config.md).
- Not a CM plan. That's Phase C (`mission/configuration/CM-Plan.md`).
- Not a release-engineering doc. That's Phase C (`mission/configuration/release-process.md`).
- Not a security model. Container security posture (rootless, seccomp, image signing) is tracked as Phase C.
