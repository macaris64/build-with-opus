# 08 — Timing & Clocks

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Bibliography: [../standards/references.md](../standards/references.md) — notably CCSDS 301.0-B-4 (Time Code Formats) and CCSDS 302.0-B-1 (Time Management). System context: [00-system-of-systems.md](00-system-of-systems.md). Operational context: [../mission/conops/ConOps.md](../mission/conops/ConOps.md) §6 (Time authority).

This doc is the **single source of truth for time** across SAKURA-II. Every ICD that carries a timestamp resolves its format here. Every V&V latency budget traces back to the clock model here. Every segment doc that configures cFE TIME, `rclcpp::Clock`, FreeRTOS ticks, or a Rust clock bridge cites this doc.

## 1. Time Scales Used

Five time scales appear in SAKURA-II. Confusing them has caused real-mission loss; keep the distinctions sharp.

| Scale | Definition | Where it appears |
|---|---|---|
| **UTC** | Coordinated Universal Time — civil time with leap seconds | Operator UI, ground-station displays, command authorization windows |
| **TAI** | International Atomic Time — monotonic, no leap seconds | All **internal** timestamps on-board every SAKURA-II asset; time-tag field of every CCSDS secondary header |
| **MET** | Mission Elapsed Time — seconds since a mission-defined T₀ epoch (Phase P1 MOI start) | Log files, scenario scripting, V&V timeline references |
| **SCLK** | Spacecraft Clock — free-running integer counter local to each asset | cFE TIME service, correlation to TAI via telemetry |
| **Sim Wall-Clock** | Host-machine wall time while SITL is running | Docker logs, troubleshooting; never crosses an ICD |

**The on-board scale is TAI**; the user-facing scale is UTC. Conversion happens at exactly two boundaries:
- The ground-station UI (`rust/ground_station/`) — TAI→UTC for display, UTC→TAI for command validity windows.
- Scenario scripting at authoring time — author writes UTC, tooling converts to TAI before injection.

**Leap-second policy**: ignore internally. TAI is leap-second-free by construction; UTC conversion at the UI applies the current TAI–UTC offset (a compile-time constant refreshed only on ground-station release). Documented deviation from naive CCSDS interpretations — see [deviations.md](../standards/deviations.md) once a deviation is formally taken.

## 2. Timestamp Format on the Wire

Per decision Q-C6, the CCSDS secondary header carries **Time + Command code + Instance ID**. The time field follows CCSDS 301.0-B-4 **CUC (CCSDS Unsegmented Time Code)** with a fleet-standard sub-second width.

### CUC layout (fleet-wide)

| Field | Width | Meaning |
|---|---|---|
| P-Field (preamble) | 1 byte | Fixed = `0x2F` → TAI epoch 1958-01-01, 4-byte coarse + 2-byte fine |
| Coarse time | 4 bytes, big-endian | Seconds since TAI epoch (1958-01-01 00:00:00 TAI) |
| Fine time | 2 bytes, big-endian | Sub-second, unit = 2⁻¹⁶ s ≈ 15.26 µs |

Total on-wire size: **7 bytes**. Resolution: 2⁻¹⁶ s ≈ 15.26 µs. Range: 2³² seconds ≈ 136 years from TAI epoch — ample for any plausible mission.

Per [Q-C8](../standards/decisions-log.md), CUC fields are encoded/decoded on the Rust side exclusively via `ccsds_wire::cuc::{encode, decode}` (designed in [`06-ground-segment-rust.md`](06-ground-segment-rust.md), Batch B3).

**Fleet precision requirement** (per Q-F6): 1 ms. The CUC fine field gives ~15 µs of precision; the 1 ms figure is the **end-to-end fleet-sync precision**, not the CUC field width. CUC carries more precision than the fleet keeps synchronized, which is correct — the resolution should exceed the sync bound.

### Secondary header layout in full

Per packet-catalog and every ICD:

| Field | Width | Notes |
|---|---|---|
| CUC time tag | 7 bytes | As above |
| Command / Telemetry code | 2 bytes, big-endian | Command code for TC; function code / HK-packet-type for TM |
| Instance ID | 1 byte | Identifies instance within a vehicle class (up to 255 instances per APID block, satisfying scale targets with margin) |

**Total secondary header**: 10 bytes. Combined with the 6-byte CCSDS primary header, every SAKURA-II Space Packet has a 16-byte framing overhead before user data. This is the same for every boundary except the cryobot tether (see §7).

## 3. Fleet Time Authority Ladder

The authoritative source of UTC is hierarchical, with documented fallback. Per decision Q-F4, the hybrid posture is:

```
   Ground Station (UTC from host NTP)      ← primary UTC authority
         │
         │  Time Correlation Packets (TCP)
         ▼
   Orbiter-01 cFE TIME service              ← TAI/SCLK master when in AOS with ground
         │
         │  In-band time tag in every AOS frame
         ▼
   Relay-01 FreeRTOS time task              ← disciplines to orbiter cross-link time tag
         │
         │  Proximity-1 time tag
         ▼
   Rovers (land / UAV / cryobot)            ← discipline to relay
         │
         │  ROS 2 clock bridge
         ▼
   Per-node rclcpp::Clock                   ← inherits from rover comm node
```

During AOS with ground, the chain is closed and drift is correction-tracked. During LOS periods, each hop maintains its own free-running clock with a documented drift bound.

### Time Correlation Packet (TCP) cadence

- **Ground → Orbiter**: 1 TCP per 60 s while in AOS. Carries ground's UTC (converted to TAI) plus round-trip delay probe.
- **Orbiter → Relay**: time tag inline on every AOS cross-link frame — no separate TCP needed.
- **Relay → Rover**: time tag inline on every Proximity-1 frame (hailing frames emitted at 1 Hz during acquisition per [Q-C5](../standards/decisions-log.md) also carry a time tag).
- **Rover → Cryobot**: time tag inline on every tether frame (bandwidth-budgeted: ≥ 1 tag per second even in BW-collapse).

## 4. LOS Drift Budget

Per decision Q-F4, **50 ms drift over 4 h LOS**. That is a drift-rate bound of:

```
50 ms / 14400 s = 3.47 µs/s = 3.47 ppm
```

This is achievable with a temperature-compensated crystal oscillator (TCXO) and is aggressive enough that a software-only drift model (no hardware assist) would fail it. `architecture/01-orbiter-cfs.md` (planned, Batch B3) will specify the HPSC clock-source configuration that meets this bound.

### Per-asset drift allocation

The 50 ms end-of-LOS budget is apportioned across the time-authority ladder:

| Asset | Allocated drift over 4 h LOS | Achievability |
|---|---|---|
| Orbiter-01 (TAI master during LOS) | 30 ms (2.08 ppm) | TCXO + SCLK calibration, feasible |
| Relay-01 | +10 ms (0.70 ppm additional, relative to orbiter) | TCXO, feasible |
| Rover (nominal) | +7 ms (0.49 ppm additional) | TCXO, feasible |
| Cryobot (subsurface, thermal-constrained) | +3 ms (0.21 ppm additional) | OCXO or periodic re-sync; see open item below |
| **Total** | **≤ 50 ms** end-to-end | |

The cryobot's thermal environment (borehole, tether-heat-limited) makes TCXO performance uncertain. **Open item for B3**: if the cryobot cannot hit 0.21 ppm, the surface rover re-sync cadence across the tether must tighten; this is a tether-BW tradeoff that lands in [`../interfaces/ICD-cryobot-tether.md`](../interfaces/ICD-cryobot-tether.md) (planned).

### What happens at t = 4 h of LOS

If LOS exceeds 4 h with no correlation frame received, the relevant asset transitions its time service to `DEGRADED` state and tags every subsequent outgoing packet with a `time_suspect` flag in the secondary header's command/telemetry code low bit. The ground station surfaces this flag explicitly; V&V scenarios exercise it.

## 5. Per-Segment Clock Implementation

### 5.1 cFS Orbiter (cFE TIME service)

cFE provides a `CFE_TIME_SysTime_t` (seconds + subseconds, 32+32) internal representation. SAKURA-II configuration:

- **Master mode**: `TIME_CFG_SRC_MET` during LOS; `TIME_CFG_SRC_EXTERNAL` when a ground TCP is valid.
- **Subseconds unit**: 2⁻³² s internal, converted to CUC 2⁻¹⁶ s on the wire by a lossy truncation (documented; the residual is sub-µs and well below any latency-budget line).
- **SCLK definition**: integer MET seconds since T₀; published over SB on MID `0x189F` (TBR — placeholder pending app-specific allocation in `01-orbiter-cfs.md`).

### 5.2 FreeRTOS Relay

FreeRTOS has no native wall-clock service. SAKURA-II adds a `time_task` (priority just below comm tasks) that:

1. Maintains a 64-bit `tai_ns` value updated from the periodic tick (1 ms tick interrupt → 1 000 000 ns increment, corrected periodically against orbiter time tags).
2. Provides `time_now_cuc(uint8_t out[7])` for the comm stack's packet framers.
3. Triggers the `DEGRADED` flag if no valid orbiter time tag has been seen for 4 h.

### 5.3 FreeRTOS Subsystem MCUs

Each MCU inherits its time from the orbiter's cFS gateway app via a dedicated SB message (`0x28F` range — subsystem-MCU APID block from [`../interfaces/apid-registry.md`](../interfaces/apid-registry.md)). MCUs do **not** run their own time-correlation logic — they are slaves to the cFS gateway. This simplifies FMEA: an MCU with a stuck clock is detectable at the gateway, not the MCU itself.

### 5.4 Space ROS 2 Rovers

ROS 2 has three clock types (`ROSTime`, `SystemTime`, `SteadyTime`). SAKURA-II uses:

- **Simulation published `/clock`** during SITL — rovers set `use_sim_time: true`.
- In a hardware-in-the-loop future, `/clock` is published by a dedicated **time-bridge LifecycleNode** that consumes CCSDS time tags from the relay link and republishes as ROS Time.

The time-bridge node is the **only** place where non-ROS time crosses into ROS time. All other rover nodes use `rclcpp::Clock` (`RCL_ROS_TIME`), not host wall-clock. `std::chrono::system_clock` is banned in rover node code — static analysis in CI will enforce this during Batch B3.

### 5.5 Rust Ground Station

- Internal representation: `time::OffsetDateTime` in UTC (the [`time` crate](https://crates.io/crates/time)).
- Boundary decoders (`cfs_bindings`) convert CUC → TAI seconds → UTC on read, and UTC → TAI → CUC on write (for TC packets).
- Operator UI timestamps: UTC with millisecond precision, ISO-8601 format. Never TAI on the UI (too confusing).
- Log files: TAI internally (so log correlation is leap-second-free).

## 6. Light-Time Model

Earth ↔ Mars signal delay varies with orbital geometry:

- **Nominal MVC configuration**: one-way light time = 10 minutes (mid-band).
- **Configurable range**: 4–24 minutes, settable via `docker-compose` environment variable `LIGHT_TIME_OW_SECONDS` (out-of-scope for this doc; captured in [`10-scaling-and-config.md`](10-scaling-and-config.md)).
- **Model**: fixed value per scenario in Phase B. Ephemeris-driven variation (SPICE kernel integration) is deferred; the seam is the `clock_link_model` container in [`00-system-of-systems.md`](00-system-of-systems.md) §2.

Light-time is applied **once** at the ground ↔ space boundary — never double-applied at cross-links or surface links. A unit test of the `clock_link_model` enforces this (Batch B4 `V&V-Plan.md`).

## 7. Cryobot Tether — Time Tagging Under Bandwidth Pressure

The tether has a BW-collapse mode (10 Mbps → 100 kbps, per Q-C1). Full 10-byte secondary headers on every packet become prohibitive under collapse. Mitigations (detailed in [`../interfaces/ICD-cryobot-tether.md`](../interfaces/ICD-cryobot-tether.md), planned):

- **Nominal**: full 10-byte secondary header.
- **BW-collapse**: reduced 4-byte secondary header (2-byte coarse-time delta from last sync + 2-byte instance+code). A periodic full 10-byte "sync" packet re-anchors.

The frame-level decoder reconstructs the full time tag from the delta + last-sync-anchor. The anchor interval is bounded such that a single missed anchor cannot cause a time-tag error greater than the LOS drift budget.

## 8. Reserved Seams (bit-flip / EDAC layer, Phase-B-next)

The time service is a prime target for radiation effects (a bit flip in the SCLK counter causes a huge jump). Reserved hooks:

- **Orbiter cFE**: cFE TIME's `CFE_TIME_Copy()` wrapper is the single place where a future TMR (triple-modular-redundant) read could plug in. The architecture does not add TMR in Phase B, but every user of `CFE_TIME_SysTime_t` goes through a shared helper (to be defined in `01-orbiter-cfs.md`, Batch B3) so the plug-in surface is preserved.
- **Relay / MCU**: the 64-bit `tai_ns` value is stored at an address that the future EDAC-protected memory region will contain; all accesses go through `time_now_cuc()` so callers do not need to change.
- **Rust ground-side**: no bit-flip exposure — ground hosts are assumed unradiated.

See [`09-failure-and-radiation.md §5`](09-failure-and-radiation.md) for the full reserved-hook strategy (Q-F3 definition site).

## 9. Open Items (tracked, not blocking)

- **Cryobot OCXO availability.** If OCXO is not feasible within the cryobot's thermal/power budget, the surface-rover re-sync cadence across the tether must tighten. Decision needed before `ICD-cryobot-tether.md` closes (Batch B2).
- **NTP posture on the ground station container.** For SITL, `chronyd` against the host is adequate. For cloud/CI runs, pin a public NTP pool. Captured in `dev/docker-runbook.md` (Batch B5).
- **Mission T₀ epoch.** ConOps says "P1 MOI start"; a concrete simulated date is pending and will land in `mission/conops/mission-phases.md` (Batch B5).

## 10. What this doc is NOT

- Not a reliability analysis of clock hardware — that lives in `09-failure-and-radiation.md`.
- Not a treatment of ephemeris or orbital mechanics — that is out of scope entirely for Phase B (SPICE-kernel integration is deferred).
- Not a specification of *which* TCXO/OCXO parts fly on which asset — those are board-level decisions belonging to a (future) hardware doc.
