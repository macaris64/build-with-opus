# Simulation Walkthrough — SCN-NOM-01 End-to-End

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Scenarios: [`../mission/conops/ConOps.md §4–5`](../mission/conops/ConOps.md). Fault packets: [`../interfaces/packet-catalog.md §7`](../interfaces/packet-catalog.md). Fault pipeline: [`../architecture/09-failure-and-radiation.md`](../architecture/09-failure-and-radiation.md). Protocol stack: [`../architecture/07-comms-stack.md §9`](../architecture/07-comms-stack.md). Sim architecture: [`../architecture/05-simulation-gazebo.md`](../architecture/05-simulation-gazebo.md). Docker target state: [`docker-runbook.md`](docker-runbook.md). Troubleshooting: [`troubleshooting.md`](troubleshooting.md).

This walkthrough runs the nominal surface-ops HK scenario (**SCN-NOM-01** from [ConOps §4](../mission/conops/ConOps.md)) end-to-end — rover HK origin → Proximity-1 → AOS cross-link → AOS downlink → ground-station UI. An appendix covers **SCN-OFF-01** (cryobot tether fault) via `0x540` fault injection.

> **[Phase B]** — full Docker compose bring-up is deferred per [`docker-runbook.md`](docker-runbook.md). This walkthrough gives the hand-spin steps for the stacks present today and marks container-orchestrated steps as `[Phase B]` placeholders.

## 1. What you'll see

- Rover HK packets departing at 1 Hz on APID `0x300`.
- Proximity-1 framing to relay, AOS cross-link to orbiter, AOS downlink to ground.
- Ground-station UI rendering rover HK **~10 minutes later** (the configured light-time from [`../mission/conops/ConOps.md §6`](../mission/conops/ConOps.md)).
- No SPP sequence-counter gaps anywhere along the chain.

Expected timing (Phase-B numbers, subject to your host):

| Step | Typical duration |
|---|---|
| Build all stacks (clean) | 3–6 min |
| Sim bring-up | ~30 s |
| First HK observed at ground | Light-time (~10 min sim-time) |
| Scenario duration | 10–15 min wall-clock |

## 2. Prerequisites

- Quickstart green ([`quickstart.md §11`](quickstart.md)) — all four stacks build + test clean.
- Gazebo Harmonic installed ([`quickstart.md §1`](quickstart.md)).
- `_defs/mission.yaml` present (planned artefact per [`../architecture/10-scaling-and-config.md §3`](../architecture/10-scaling-and-config.md)) — otherwise the hand-spin route requires editing per-stack defaults manually.
- `simulation/scenarios/scn-nom-01.yaml` present (planned artefact, see [`../architecture/05-simulation-gazebo.md §11`](../architecture/05-simulation-gazebo.md) open item).

## 3. Step-by-step (hand-spin, Phase B)

Open four terminals.

### 3.1 Terminal 1 — Gazebo + clock/link model

```bash
# From repo root
cmake --build build --target gazebo_rover_plugin
gz sim -v 3 simulation/worlds/mars-mvc.sdf      # [Phase B] world file
```

Expected: Gazebo window opens, world loads with rover + cryobot model instances. The `clock_link_model` plugin publishes simulated TAI on a known topic (per [`../architecture/05-simulation-gazebo.md §5`](../architecture/05-simulation-gazebo.md)).

**If this fails**: [`troubleshooting.md §4`](troubleshooting.md) (Gazebo section).

### 3.2 Terminal 2 — cFS orbiter

```bash
# From repo root
cmake --build build
./build/apps/orbiter_cdh/orbiter_cdh        # [Phase B] entrypoint stub
```

Expected: cFE ES / SB / TIME / EVS services come up. `sample_app` registers on APID `0x170`. Standard startup events flow (`ES_APP_STARTED`, etc.). Until [`01 §3.2`](../architecture/01-orbiter-cfs.md) mission apps are authored, this is a minimal bring-up.

**If this fails**: [`troubleshooting.md §1`](troubleshooting.md).

### 3.3 Terminal 3 — ROS 2 rovers

```bash
cd ros2_ws
source install/setup.bash
ros2 launch rover_bringup rover.launch.py       # existing
```

Expected: `rover_teleop` lifecycle node transitions `unconfigured → inactive → active`. `rover_bringup` spawns any composed nodes per [`../architecture/04-rovers-spaceros.md §4`](../architecture/04-rovers-spaceros.md). With `use_sim_time: true` set, the rovers listen to Gazebo's `/clock`.

**If this fails**: [`troubleshooting.md §2`](troubleshooting.md).

### 3.4 Terminal 4 — Rust ground station

```bash
cd rust
cargo run -p ground_station                     # [Phase B] binary is planned; current member is library-only
```

Expected: UI binds on `http://localhost:8080` (per [`docker-runbook.md §5`](docker-runbook.md) target state). TM ingestion stream starts empty.

**If this fails**: [`troubleshooting.md §3`](troubleshooting.md).

### 3.5 Observe SCN-NOM-01 path A

Per [`../architecture/07-comms-stack.md §9 Path A`](../architecture/07-comms-stack.md):

1. Rover comm node packs HK into CCSDS Space Packets on APID `0x300`, code `0x0001`, at 1 Hz.
2. Proximity-1 framing to Relay-01 (FreeRTOS).
3. Relay store-and-forward queues until next orbiter pass.
4. AOS cross-link frame to Orbiter-01.
5. Orbiter SB routes to `TO` app; `TO` emits AOS VC 0 downlink.
6. `clock_link_model` adds +10 min light-time delay.
7. Ground-station AOS decoder → SPP decoder → TM pipeline.
8. Operator UI updates with rover HK.

In the UI you should see:

- **Light-time indicator** — ~10 min between "packet origin" and "UI display".
- **No gaps in SPP sequence counter** for APID `0x300`.
- **Rover lifecycle state = `ACTIVE`** per [`../mission/conops/ConOps.md §4`](../mission/conops/ConOps.md).

### 3.6 Send a mode-change TC

From the UI (per step 6 of [`../mission/conops/ConOps.md §4`](../mission/conops/ConOps.md)):

- Select target: `rover_land-01`.
- TC: mode-change on APID `0x382` (TC block for rover-land TCs per [`../interfaces/apid-registry.md`](../interfaces/apid-registry.md)).
- Expected round trip: ~20 min wall-clock (2× light-time + processing).
- Expected ACK: rover acknowledges via the same downlink chain; lifecycle state reflects the requested mode.

## 4. Compose-orchestrated variant (target state)

Once the Phase-B infra PR lands ([`docker-runbook.md §5`](docker-runbook.md)):

```bash
docker compose --profile minimal up           # [Phase B]
```

All nine containers in the `minimal` profile come up in the order prescribed by [`docker-runbook.md §5`](docker-runbook.md). Replace terminals 1–4 above with a single command. Scenarios replay via:

```bash
make scenario SCN-NOM-01                      # [Phase B]
```

(or equivalent wrapper; exact interface fixed when `fault_injector` lands per [`../architecture/05-simulation-gazebo.md §11`](../architecture/05-simulation-gazebo.md).)

## 5. Appendix — SCN-OFF-01 (Cryobot Tether Fault)

Reference: [`../mission/conops/ConOps.md §5`](../mission/conops/ConOps.md).

Scenario: the cryobot is descending (Phase P5). A tether-bandwidth collapse triggers BW-collapse mode; the drill controller must transition to `HOLD`.

### 5.1 Stimulus

Fault injection via APID `0x540` (packet drop) targeting the `ROVER-CRYOBOT` link per [packet-catalog §7.1](../interfaces/packet-catalog.md):

```yaml
# simulation/scenarios/scn-off-01.yaml       [Phase B]
scenario: SCN-OFF-01
duration_s: 900
faults:
  - type: packet_drop
    at_tai_offset_s: 120
    link_id: ROVER-CRYOBOT
    drop_probability: 0.90       # near-total drop
    duration_ms: 600000          # 10 minutes
```

### 5.2 Expected response chain

Per [`../mission/conops/ConOps.md §5`](../mission/conops/ConOps.md):

1. Cryobot comm node detects link-quality threshold crossed → DEGRADED state.
2. Drill controller receives latched DEGRADED topic → transitions to `HOLD`.
3. Rover-land relay-role comm node observes missing cryobot HK for N consecutive intervals.
4. Relay emits Proximity-1 anomaly frame upstream.
5. Relay-01 propagates as AOS VC 1 (event log) packet.
6. Orbiter-01 CDH logs via EVS; downlinks on next contact.
7. Ground station alerts operator; UI shows: cryobot state = `HOLD`, tether = `DEGRADED`, last good depth.

### 5.3 Gate assertions

From [`../mission/verification/V&V-Plan.md §3.2`](../mission/verification/V&V-Plan.md):

- Drill transitions to `HOLD` within N Proximity-1 intervals.
- No cryobot actuator command is lost-then-executed-late — commands in flight at fault onset are rejected with a safing-period error.
- Event reaches ground within `light-time + 2 × tether-retransmit window`.
- DEGRADED-latch is fate-shared with an independent watchdog (not only the voluntary HK subscription).
- Relay store-and-forward queue preserves the anomaly as priority event when operator is LOS.

### 5.4 Resume path

A `SCN-OFF-01-RESUME` scenario (Phase C) exercises the operator `RESUME` TC that transitions the cryobot out of `HOLD` once the tether recovers. Not runnable in Phase B.

## 6. Q-F3 read-hook regression (bonus)

Per [`../architecture/09-failure-and-radiation.md §5.3`](../architecture/09-failure-and-radiation.md) — inject a clock skew (APID `0x541`) and assert that the `.critical_mem`-resident `tai_ns` store's checksum is unchanged:

```yaml
# simulation/scenarios/q-f3-read-hook.yaml   [Phase B]
scenario: Q-F3-READ-HOOK
duration_s: 60
faults:
  - type: clock_skew
    at_tai_offset_s: 10
    asset_class: ORBITER
    instance_id: 1
    offset_ms: 5000
    rate_ppm_x1000: 0
    duration_s: 30
```

Ground-side regression assertion: `cfe_time_now()` returns a +5000 ms shifted value during the fault window; `g_tai_ns` store checksum is identical before, during, and after.

## 7. What to do if it fails

- **Build fails**: [`build-runbook.md`](build-runbook.md) + [`troubleshooting.md`](troubleshooting.md).
- **A stack doesn't start**: check the per-stack section of [`troubleshooting.md`](troubleshooting.md).
- **SPP gaps appear**: investigate the first hop. Sequence-counter gaps upstream of the relay point to a rover publisher bug; gaps only visible downstream of the orbiter point to the cross-link or AOS encoder.
- **Light-time wrong**: check `LIGHT_TIME_OW_SECONDS` in the `clock_link_model` env vars ([`docker-runbook.md §4`](docker-runbook.md)) or `mission.light_time_ow_seconds` in the YAML.
- **TC never arrives**: verify `CFS_FLIGHT_BUILD` is unset — a flight-build cFS image rejects sim-injection APIDs and also rejects TCs originated by a sim-only ingest path ([`ICD-sim-fsw.md §5`](../interfaces/ICD-sim-fsw.md)).

## 8. What this walkthrough is NOT

- Not a scenario schema. Schema lives with `fault_injector` when it lands ([`../architecture/05-simulation-gazebo.md §11`](../architecture/05-simulation-gazebo.md)).
- Not a CI smoke test. System-level regression in CI is a Phase-C open item per [`../mission/verification/V&V-Plan.md §7`](../mission/verification/V&V-Plan.md).
- Not a tutorial on Space Packet Protocol. See [`../architecture/07-comms-stack.md §2`](../architecture/07-comms-stack.md).
- Not a Docker tutorial. Target-state bring-up is specified in [`docker-runbook.md`](docker-runbook.md); authoring the compose file is Phase-B infra work.
