# SAKURA-II Concept of Operations

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Standards pedigree: NPR 7123.1C §3.1 and ECSS-E-ST-10C (see [../../standards/references.md](../../standards/references.md) — planned).

This ConOps is deliberately minimal for Phase A: it fixes the **actors**, the **mission phase set**, and **one nominal + one off-nominal scenario** that every downstream requirement and test can cite. Expansion lives in [`mission-phases.md`](mission-phases.md) (planned).

## 1. Mission Purpose

SAKURA-II is a software-in-the-loop demonstrator for a heterogeneous Mars fleet: orbiters, a smallsat relay, and three surface-asset classes (land rover, UAV, subsurface cryobot). It exercises a full CCSDS comms stack, radiation-affected behavior, and Earth–Mars light-time across a Dockerized simulation, on a design path toward eventual NASA HPSC flight hardware.

The simulator is the deliverable. Any flight hardware targeting is **an alignment goal**, not a deployment target for this repo.

## 2. Actors

### Vehicle actors (MVC fleet)

| Actor | Class | FSW | Count (MVC) | Count (Scale-5) |
|---|---|---|---|---|
| Orbiter-01 | Primary orbiter (science + comm) | cFS on C17 | 1 | 5 |
| Relay-01 | Smallsat relay (store-and-forward) | FreeRTOS end-to-end | 1 | 5 |
| Rover-Land-01 | Wheeled surface rover | Space ROS 2 | 1 | 5+ |
| Rover-UAV-01 | Martian UAV (helicopter-class) | Space ROS 2 | 1 | 3+ |
| Rover-Cryobot-01 | Subsurface ice-penetrating rover, tethered | Space ROS 2 | 1 | 2+ |

Scaling from MVC to Scale-5 is a **config change** (`_defs/` tables, compose profiles, ROS 2 launch arguments); see [`../../architecture/10-scaling-and-config.md`](../../architecture/10-scaling-and-config.md) (planned).

### External actors

| Actor | Role |
|---|---|
| Mission Operator | Human at ground station; authors TC uplinks and monitors TM |
| Ground Station (`rust/ground_station/`) | DSN-surrogate: TM ingest, TC uplink, CFDP Class 1 receiver, operator UI |
| Gazebo Environment | Mars physics world; generates all sensor data |
| Fault Injector | Test-only actor that stimulates functional faults (radiation surrogate, dropped packets, clock skew). See [`../../architecture/09-failure-and-radiation.md`](../../architecture/09-failure-and-radiation.md). |

### Non-actors (explicitly out of scope)

- Real DSN integration
- Real radio / RF front-end simulation (SDR-level modeling) — SAKURA-II simulates above the link layer
- Humans on Mars, crewed operations
- Interplanetary cruise guidance — SAKURA-II starts at Mars Orbit Insertion

## 3. Mission Phases

Phase boundaries are the anchor for phase-scoped requirements, V&V scenario scoping, and the compose-profile overlay set.

| # | Phase | Entry criterion | Exit criterion | Active actors |
|---|---|---|---|---|
| P1 | MOI | Sim clock reaches MOI burn window | Orbiter-01 in stable Mars orbit | Orbiter, Ground |
| P2 | Relay Deployment | Orbiter-01 stable; Relay-01 separation command | Relay-01 acquires cross-link with Orbiter-01 | Orbiter, Relay, Ground |
| P3 | Surface EDL | Surface asset separation | Surface asset on ground, nominal telemetry via Relay | Orbiter, Relay, surface-asset-under-test, Ground |
| P4 | Surface Ops (nominal) | All MVC assets nominal | Mission-elapsed-time goal met OR off-nominal trigger | All |
| P5 | Cryobot Descent | Operator TC authorizes drill start | Cryobot reaches target depth OR tether fault | Cryobot, Relay, Orbiter, Ground |
| P6 | Safe Mode | Any asset enters safe | Operator commands exit; all assets nominal | Asset-in-safe + Ground |

Phase transitions are explicit events, logged through cFE EVS on cFS assets and `rclcpp` logging on rovers. The ground station's phase indicator is driven by an event subscription, not by wall-clock, so light-time delay does not desynchronize operator awareness (it just delays it — documented in [`../../architecture/08-timing-and-clocks.md`](../../architecture/08-timing-and-clocks.md), planned).

## 4. Nominal Scenario — **SCN-NOM-01: Surface Ops HK Downlink**

**Premise.** Steady-state operations, all MVC assets healthy, Earth–Mars geometry giving ~10 min one-way light time.

**Narrative:**

1. Rover-Land-01 runs its nominal mobility + housekeeping lifecycle (`rover_land` nodes in ACTIVE state). HK at 1 Hz is published on a DDS topic.
2. The rover's comms lifecycle node packs HK into CCSDS Space Packets addressed to APID `0x300` (see [`../../interfaces/apid-registry.md`](../../interfaces/apid-registry.md)) and transmits over the simulated Proximity-1 link (CCSDS 211.0-B) to Relay-01.
3. Relay-01 (FreeRTOS) receives Proximity-1 frames, buffers them in its store-and-forward queue, and on the next Orbiter-01 pass transmits them over the cross-link as AOS VC 0 (real-time HK).
4. Orbiter-01 (cFS: SB routes → TO app) concatenates the incoming packets into the downlink AOS frame stream to ground.
5. Ground station (`rust/ground_station/`) ingests the AOS frames, decodes SPP, and emits typed telemetry points. The operator sees rover HK ~10 min after it was generated, which is **correct** — the UI's light-time indicator confirms the expected delay.
6. Operator uplinks a mode-change TC (APID `0x382`). It traverses ground → orbiter → relay → rover, each hop logging its handoff.
7. Rover acknowledges via the same downlink chain. Round-trip is ~20 min.

**Success criteria:**
- No packet loss at any hop (all SPP sequence counters monotonic, no gaps).
- Ground timestamps show end-to-end latency within the modeled light-time band.
- All lifecycle nodes remain in ACTIVE state; no EVS events above INFO severity on cFS assets.

**Traces to:** nominal-path requirements in `FSW-SRD`, `ROV-SRD`, `GND-SRD` (planned).

## 5. Off-Nominal Scenario — **SCN-OFF-01: Cryobot Tether Fault**

**Premise.** Cryobot-01 is in P5 (Descent), ~80 m below surface, communicating through its tethered optical link to the surface relay module of Rover-Land-01.

**Stimulus.** Fault injector triggers a tether-bandwidth collapse: link degrades from nominal data rate to ~1% of nominal, with intermittent full dropouts (duty cycle 30%).

**Expected Response Chain:**

1. Cryobot comm node detects link-quality indicator crossing its threshold; marks link DEGRADED and switches to low-BW encoding (short HK summary frames only).
2. Cryobot mobility node (drill controller) receives the DEGRADED state via a latched DDS topic and autonomously transitions to HOLD (drill stops, posture stable, heater duty cycle reduced to conserve power under an assumed longer surface-comms silence).
3. Rover-Land-01's relay-role comm node observes missing cryobot HK for N consecutive intervals and emits a Proximity-1 anomaly frame upstream.
4. Relay-01 propagates the anomaly over the cross-link as an AOS VC 1 (event-log) packet.
5. Orbiter-01's CDH logs the event via EVS and downlinks on next contact.
6. Ground station alerts operator. Operator's MO position shows cryobot state = `HOLD`, tether = `DEGRADED`, last good depth = 80 m.

**Successful outcome (what the mission considers "handled well"):**
- No cryobot actuator command is lost-then-executed-late (commands in flight when the fault began are rejected with a safing-period error code, not silently queued).
- The event reaches ground in a bounded time (≤ light-time + 2 × tether-retransmit window).
- Operator can uplink a RESUME TC once a repair scenario runs (SCN-OFF-01-RESUME, Phase C).

**Failure modes to explicitly test:**
- What if the fault masks HK *and* the drill controller does not transition to HOLD? Verification must show that the DEGRADED-latch is **fate-shared** with an independent watchdog, not only with a voluntary HK subscription.
- What if the relay's store-and-forward queue fills while the operator is LOS? The anomaly must be preserved as a priority event, not dropped.

**Traces to:** `FSW-REQ-####` (cryobot FSW safing behavior), `ROV-REQ-####` (drill HOLD transition), `GND-REQ-####` (operator alert latency), `SYS-REQ-####` (bounded event latency). IDs assigned in Phase C.

## 6. Operational Constraints

- **Light-time delay.** One-way latency is a configurable sim parameter in the 4–24 minute band. All operational decisions that require round-trip acknowledgment must tolerate at least 2× light-time; none may block a real-time safing response.
- **Radiation.** Phase A/B model is **functional injection only** (see [`../../architecture/09-failure-and-radiation.md §2`](../../architecture/09-failure-and-radiation.md)); bit-flip / EDAC layer is architected-for but deferred (reserved hooks at [`09 §5`](../../architecture/09-failure-and-radiation.md) and scale-up seams at [`09 §6`](../../architecture/09-failure-and-radiation.md)). ConOps-level implication: operator-visible behavior treats radiation events as discrete "this asset entered safe" events, not as memory-level faults the operator must diagnose.
- **Time authority.** Ground is primary UTC authority; orbiters maintain autonomous SCLK with a documented drift model during LOS. Ladder: Ground → Orbiter-01 SCLK → Relay → surface assets. See [`../../architecture/08-timing-and-clocks.md`](../../architecture/08-timing-and-clocks.md) (planned).
- **Comms availability.** All surface-to-ground traffic is relay-mediated (no direct-to-Earth from surface). Cryobot is relay-mediated through a surface rover, meaning its link is doubly relayed.

## 7. Safe-Mode Ladder (summary)

Per-asset-class detail will live in each asset's architecture doc. At the mission level:

1. **Detection** (local): asset-internal invariant violation, watchdog timeout, or command.
2. **Localization**: asset enters its own safe mode; limits actuator authority, preserves downlink capability if possible.
3. **Propagation**: event log to next hop upward (MCU → cFS, rover → relay, relay → orbiter → ground).
4. **Human-in-the-loop**: operator acknowledges before any RESUME command is accepted. No autonomous exit from safe mode is permitted for any asset in Phase A/B — removing this restriction is itself a requirement change that must go through CM.

## 8. Open Items (non-blocking for Phase B)

- Fleet-scaling concept-of-operations when >1 orbiter is above the horizon at the same surface asset (relay selection policy). Will land in `mission-phases.md` before Phase B closes.
- Exact light-time and drift-model constants — see [open questions in the plan](../../../.claude/plans/in-this-project-i-hazy-kahan.md).
- CFDP Class 2 upgrade path — ConOps-level impact only: operator will see delivery-acknowledged indicators; no net new scenarios at this layer.
