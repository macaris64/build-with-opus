# ICD — Simulation ↔ FSW (Gazebo Fault-Injection Bridge)

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Packet bodies: [packet-catalog.md](packet-catalog.md). Protocol stack: [../architecture/07-comms-stack.md](../architecture/07-comms-stack.md). APID registry: [apid-registry.md](apid-registry.md). Decisions: [../standards/decisions-log.md](../standards/decisions-log.md).

This ICD fixes the **Gazebo ↔ Flight Software bridge** for SAKURA-II. It covers:
- The two-role bridge: **`fault_injector`** (Gazebo-side emitter) and **`sim_adapter`** (cFS-side receiver).
- The **APID reservation `0x540`–`0x543`** for the four minimum faults (per [Q-F2](../standards/decisions-log.md)): packet drop, clock skew, force safe-mode, sensor-noise corruption.
- The **payload-level CRC-16/CCITT-FALSE validation** required before `CFE_SB_Publish` — the in-process transport has no frame CRC, so the bridge carries its own per [packet-catalog §2.4](packet-catalog.md).
- The **`CFS_FLIGHT_BUILD` guard** that makes this entire APID block unreachable in any non-SITL build.
- **Sensor injection** on the sibling APID block `0x500`–`0x53F` and **FSW → sim diagnostics** on `0x570`–`0x57F`.

All multi-byte fields are **big-endian** per [Q-C8](../standards/decisions-log.md). Packet bodies are inherited verbatim from [packet-catalog §7](packet-catalog.md); this ICD routes them, validates them, and delivers them to cFE SB.

## 1. Architecture

```
┌──────────────────────────────────────┐            ┌────────────────────────────────┐
│ Gazebo container                     │            │ cFS orbiter container          │
│                                      │            │                                │
│  scenario.yaml                       │            │  ┌─────────────────────────┐   │
│       │                              │            │  │      sim_adapter app    │   │
│       ▼                              │            │  │                         │   │
│  ┌─────────────┐  SPP on 0x540-0x543 │   shared   │  │  [1] deframe + CRC-16   │   │
│  │ fault_      │─────────────────────┼──mem / UDS─┼─▶│      check              │   │
│  │ injector    │                     │            │  │  [2] decode APID        │   │
│  │ task        │  SPP on 0x500-0x53F │            │  │  [3] CFE_SB_Publish     │   │
│  └─────────────┘  (sensor-inject)    │            │  └───────────┬─────────────┘   │
│       ▲                              │            │              │                 │
│       │  SPP on 0x570-0x57F          │            │              ▼                 │
│       │  (FSW→sim echo)              │            │      cFE Software Bus          │
│       └──────────────────────────────┼────────────┼────────── subscribers:         │
│                                      │            │           [orbiter_cdh,        │
│  gazebo_rover_plugin  (pose/IMU TM)  │            │            orbiter_adcs, ...]  │
│        (via plugin API, not this ICD)│            │                                │
└──────────────────────────────────────┘            └────────────────────────────────┘
```

Transport: shared memory (preferred for low-latency sideband) OR Unix-domain socket (portable fallback). Either way, the content on the wire is **CCSDS Space Packets** with the layouts defined in [packet-catalog §7](packet-catalog.md). The choice is a `sim_adapter` configuration parameter; the ICD does not mandate one.

Wire path property:
- **No frame-layer CRC** (in-process transport has no error model).
- **Payload-level trailing CRC-16/CCITT-FALSE** on every sim-injection SPP (catalog §7.1–§7.4 fields ending in `crc16`).
- **No light-time delay**: the bridge bypasses `clock_link_model`. Sim injection is instantaneous from the FSW's perspective — exactly the property that scenario testing requires.

## 2. APID Reservations (Normative)

Per [Q-F2](../standards/decisions-log.md) and [07 §8](../architecture/07-comms-stack.md), the sim APID block `0x500`–`0x57F` is partitioned as:

| APID range | Purpose | Defined in |
|---|---|---|
| `0x500`–`0x53F` | Sensor-value injection (sim → FSW) | this ICD §6 |
| **`0x540`–`0x543`** | **Fault-injection bridge (minimum four)** | **[packet-catalog §7](packet-catalog.md); this ICD §3** |
| `0x544`–`0x56F` | Reserved for future fault types | this ICD §3.5 |
| `0x570`–`0x57F` | FSW → sim diagnostics / state echo | this ICD §7 |

**APIDs `0x540`–`0x543` are reserved exclusively for fault injection.** Any other use is a bug and must be caught by CI (see §5.2 `CFS_FLIGHT_BUILD` guard and §9 lint).

## 3. Fault-Injection Packets (APID 0x540–0x543)

Each packet inherits its full body from [packet-catalog §7](packet-catalog.md). This ICD describes:
- How `fault_injector` **emits** them.
- How `sim_adapter` **validates** them.
- Which cFS subscribers **consume** them.

### 3.1 `PKT-SIM-0540-0001` — Packet Drop

Inherits body from [packet-catalog §7.1](packet-catalog.md) (10 B user data: link_id, drop_probability_x10000, duration_ms, crc16).

**Emission**: `fault_injector` reads `scenario.faults[*].packet_drop` entries from the scenario YAML. Example:

```yaml
scenario: SCN-OFF-01
faults:
  - type: packet_drop
    at_tai_offset_s: 120
    link_id: RELAY-ROVER           # enum matches PKT-SIM-0540-0001 link_id field
    drop_probability: 0.25         # written as float; translator scales to 2500 (×10000)
    duration_ms: 60000
```

At scenario time `t = 120 s`, the task emits one `PKT-SIM-0540-0001` with `drop_probability_x10000 = 2500`, `duration_ms = 60000`.

**Subscribers**: `orbiter_comm` (if `link_id = GND-ORBITER` or `ORBITER-RELAY`), `freertos_relay.comm_task` (if `link_id = RELAY-ROVER`), `rover_comm_node` (if `link_id = ROVER-CRYOBOT`), `mcu_*_gw` (if `link_id = CFS-MCU`). Each consumer applies a per-frame Bernoulli drop with the specified probability for the specified duration.

**Revert**: at `t = 120 + 60 = 180 s` a consumer internally reverts (it tracks expiry against its own clock; no explicit revert packet required). A follow-up `PKT-SIM-0540-0001` with `drop_probability_x10000 = 0` is the explicit-clear pattern.

### 3.2 `PKT-SIM-0541-0001` — Clock Skew

Inherits body from [packet-catalog §7.2](packet-catalog.md) (16 B user data: asset_class, instance_id, offset_ms, rate_ppm_x1000, duration_s, crc16).

**Emission**: triggered per scenario to exercise the `DEGRADED` time-authority transitions from [08 §4](../architecture/08-timing-and-clocks.md).

**Subscribers**: the **target asset's time task** (`cFE_TIME` for orbiter, `time_task` for relay, per-node ROS 2 time bridge for rovers). The injection offsets/rates are applied additively to the asset's local TAI; ground observers see drifting timestamps.

**Interaction with Q-F3 EDAC hooks** (full rule at [09 §5.3](../architecture/09-failure-and-radiation.md)): clock-skew injection must not corrupt the `.critical_mem`-resident `tai_ns` value on the target — it applies at the **read-path** wrapper, not by overwriting the store. Enforced by `sim_adapter`: the SB message this packet produces is explicitly targeted at a **read-hook** MID (e.g. `0x188F`), not the time-store MID.

### 3.3 `PKT-SIM-0542-0001` — Force Safe-Mode

Inherits body from [packet-catalog §7.3](packet-catalog.md) (6 B user data: asset_class, instance_id, trigger_reason_code, crc16).

**Emission**: scenario-driven; exercises the asset-local safing path.

**Subscribers**: the target asset's mode-manager task. On receipt, the manager calls its local safe-mode entry routine and emits a `PKT-TM-*-0004` EVS event with `event_id` encoding the reason code.

### 3.4 `PKT-SIM-0543-0001` — Sensor-Noise Corruption

Inherits body from [packet-catalog §7.4](packet-catalog.md) (20 B user data: asset_class, instance_id, sensor_index, noise_model, noise_param_1, noise_param_2, duration_ms, crc16).

**Sensor index table** (the binding between `sensor_index` and a cFS/MCU sensor primitive):

| `sensor_index` | Sensor | Asset | Native HK packet |
|---|---|---|---|
| `0x0001` | IMU gyro x | orbiter | PKT-TM-0110-0002 `angular_rate[0]` |
| `0x0002` | IMU gyro y | orbiter | PKT-TM-0110-0002 `angular_rate[1]` |
| `0x0003` | IMU gyro z | orbiter | PKT-TM-0110-0002 `angular_rate[2]` |
| `0x0010` | Star tracker quaternion | orbiter | PKT-TM-0110-0002 `attitude_quaternion[*]` |
| `0x0020` | Battery voltage | orbiter | PKT-TM-0130-0002 `battery_voltage` |
| `0x0021` | Battery current | orbiter | PKT-TM-0130-0002 `battery_current` |
| `0x0030` | PDU temperature | orbiter | PKT-TM-0130-0002 `pdu_temp` |
| `0x0100` | Rover wheel encoder | rover-land | — (onboard only) |
| `0x0200` | Drill torque sensor | cryobot | PKT-TM-0400-0002 `drill_torque_Nm_x100` |
| `0x0201` | Borehole pressure | cryobot | PKT-TM-0400-0002 `borehole_pressure` |
| `0x0300`–`0xFFFF` | Reserved for expansion | — | — |

**Noise model reference**:

| `noise_model` | Model | Uses `noise_param_1` | Uses `noise_param_2` |
|---|---|---|---|
| `0` = OFF | Clear any active corruption | — | — |
| `1` = GAUSSIAN | Add `N(μ, σ²)` to raw sensor word | σ as i32 | μ as i32 |
| `2` = UNIFORM | Add `U(low, high)` to raw | low as i32 | high as i32 |
| `3` = STUCK-AT | Replace sensor word with fixed value | value as i32 | — |
| `4` = BIT-FLIP | XOR raw with bitmask | bitmask as i32 | — |

### 3.5 Expansion Range (APID 0x544–0x56F)

Reserved for future fault types (e.g. link-bit-error injection, power-rail glitch). Adding a new fault:
1. New PKT-ID row in [packet-catalog §7](packet-catalog.md).
2. New APID allocation in [apid-registry.md](apid-registry.md) within `0x544`–`0x56F`.
3. New `sim_adapter` subscriber route in §3.6.
4. Sensor index table (if applicable) extended in §3.4.

## 4. `sim_adapter` cFS App — Validation Path

The `sim_adapter` is a cFS app under `apps/sim_adapter/` (to be authored with [05-simulation-gazebo.md](../architecture/05-simulation-gazebo.md) in Batch B3). Its sole job is to receive sim-injection SPPs, validate them, and republish them on cFE Software Bus with translation as needed.

### 4.1 Receive & Validate Sequence

On every sim-injection SPP received from the bridge transport (shared memory or UDS), `sim_adapter` performs:

```c
/* Pseudocode — normative behavior */
int32 sim_adapter_on_packet(const uint8_t *bytes, size_t len) {
    /* Step 1: CFS_FLIGHT_BUILD guard */
    #if defined(CFS_FLIGHT_BUILD)
        return CFE_EVS_SendEvent(SIM_ADAPTER_EID_FLIGHT_BUILD, CFE_EVS_EventType_CRITICAL,
            "sim packet received in flight build — rejecting");
    #endif

    /* Step 2: Minimum length check (16-B header + some payload + 2-B CRC) */
    if (len < 16 + 2) return emit_event(SIM_ADAPTER_EID_PACKET_TOO_SHORT, len);

    /* Step 3: Header decode via ccsds_wire (FSW-adjacent C equivalent: cfs_bindings) */
    SppHeader hdr;
    if (ccsds_wire_decode_header(bytes, &hdr) != 0)
        return emit_event(SIM_ADAPTER_EID_BAD_HEADER, 0);

    /* Step 4: APID gate — must be within 0x500-0x57F */
    if (hdr.apid < 0x500 || hdr.apid > 0x57F)
        return emit_event(SIM_ADAPTER_EID_APID_OUT_OF_RANGE, hdr.apid);

    /* Step 5: Payload CRC-16/CCITT-FALSE validation */
    const size_t user_data_len = len - 16;
    const uint8_t *user = bytes + 16;
    uint16_t want = (user[user_data_len-2] << 8) | user[user_data_len-1];   /* BE */
    uint16_t got = crc16_ccitt_false(user, user_data_len - 2);
    if (want != got)
        return emit_event(SIM_ADAPTER_EID_CRC_MISMATCH, (want << 16) | got);

    /* Step 6: Route to SB */
    return route_to_sb(&hdr, user, user_data_len - 2);   /* drop trailing CRC from SB payload */
}
```

### 4.2 CRC-16/CCITT-FALSE Implementation

Parameters per [packet-catalog §2.4](packet-catalog.md): poly `0x1021`, init `0xFFFF`, no reflect, no final XOR. Implementation is a 256-entry table in `apps/sim_adapter/fsw/src/crc16_ccitt_false.c`; cross-checked against test vector `CRC("123456789") = 0x29B1` in unit test.

### 4.3 Event IDs

| Event ID | Severity | Meaning |
|---|---|---|
| `0x07010001` | CRITICAL | `sim_adapter` received a packet under `CFS_FLIGHT_BUILD` |
| `0x07010002` | ERROR | Packet length below minimum |
| `0x07010003` | ERROR | SPP header decode failed |
| `0x07010004` | ERROR | APID outside `0x500`–`0x57F` |
| `0x07010005` | ERROR | Payload CRC-16 mismatch (`result_detail` = `(want << 16) \| got`) |
| `0x07010006` | INFO | Fault-injection applied (one per successful route) |
| `0x07010007` | ERROR | Unknown APID within sim range — no configured subscriber |

### 4.4 Performance

`sim_adapter` is a 100 Hz task; each packet costs ≤ 100 µs on x86_64 (CRC computation, 256-entry LUT). At a worst-case scenario rate of ~10 sim SPPs/s the CPU cost is negligible.

## 5. Flight-Build Lock-Out

### 5.1 Compile-Time Guard

Every source file that references an APID in `0x500`–`0x57F` (including `sim_adapter` entirely) is gated on `#ifndef CFS_FLIGHT_BUILD`. The macro is defined in `_defs/mission_config.h` for flight builds only; for SITL builds it is explicitly undefined. Violating the guard is a link-time failure — symbols simply do not exist in a flight build.

### 5.2 Runtime Defense in Depth

Even though the compile guard prevents `sim_adapter` from existing in a flight build, downstream consumers (e.g. `orbiter_cdh`) have a **secondary check**: any SB message whose MID resolves to an APID in `0x500`–`0x57F` is rejected. This is a defense against a future regression where someone accidentally compiles in a sim-injection subscriber path.

### 5.3 CI Enforcement

A `scripts/check_sim_apids.py` lint (planned for Phase C) scans `apps/**` for any literal `0x5[0-7][0-9A-F]` hex constant outside `apps/sim_adapter/**` and fails if found. This catches the case where a developer reuses a sim APID without realizing it.

## 6. Sensor Injection (APID 0x500–0x53F)

Sensor injection is symmetric to fault injection but **replaces** a sensor reading at the sensor-primitive layer rather than applying noise.

### 6.1 `PKT-SIM-0500-0001` — Set Sensor Value

| Attribute | Value |
|---|---|
| APID | `0x500` |
| Function code | `0x0001` |
| CRC | payload-level |

User-data:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 2 B | `sensor_index` | `u16` BE | same table as §3.4 |
| +2 | 4 B | `new_raw_value` | `i32` BE | Value in the native raw type of the sensor (signed to cover both i16 and u16 sensors via sign-extension rules at the consumer) |
| +6 | 4 B | `duration_ms` | `u32` BE | 0 = until cleared |
| +10 | 2 B | `crc16` | `u16` BE | CRC over [0..9] |
| **Total user data** | **12 B** | | | |

### 6.2 Sensor-Injection Reservation (`0x501`–`0x53F`)

Reserved for typed sensor injection variants (stream injection, noise-free playback from recorded HK, etc.). Allocation happens in [apid-registry.md](apid-registry.md) and body in [packet-catalog](packet-catalog.md); this ICD only reserves the range.

## 7. FSW → Sim Diagnostics (APID 0x570–0x57F)

### 7.1 `PKT-SIM-0570-0001` — State Echo

The `sim_adapter` emits a state-echo SPP once per second confirming:
- Number of sim-injection packets successfully routed in the past second.
- Number of CRC mismatches in the past second.
- Number of APID-out-of-range rejections in the past second.

This is consumed by the Gazebo `fault_injector` task to validate that injected faults actually arrive. If `fault_injector` sees a mismatch (it emitted 10 packets in the past second but `sim_adapter` reports only 9 routed), it raises a scenario error.

User-data:

| Offset | Width | Field | Type | Unit |
|---|---|---|---|---|
| +0 | 4 B | `packets_routed_last_s` | `u32` BE | count |
| +4 | 4 B | `crc_mismatches_last_s` | `u32` BE | count |
| +8 | 4 B | `apid_rejects_last_s` | `u32` BE | count |
| +12 | 4 B | `uptime_s` | `u32` BE | seconds since `sim_adapter` start |
| +16 | 1 B | `adapter_mode` | `mode_enum8` | 0=STARTUP, 1=NOMINAL, 2=DEGRADED, 3=STOPPED |
| +17 | 1 B | `reserved` | `u8` | `0x00` |
| +18 | 2 B | `crc16` | `u16` BE | CRC over [0..17] |
| **Total user data** | **20 B** | | | |

## 8. Fault Behavior

| Fault | Detected at | Action | Event ID |
|---|---|---|---|
| Packet too short | `sim_adapter` | Drop + event | `0x07010002` |
| Bad SPP header | `sim_adapter` | Drop + event | `0x07010003` |
| APID out of sim range | `sim_adapter` | Drop + event | `0x07010004` |
| Payload CRC-16 mismatch | `sim_adapter` | Drop + event; report to `fault_injector` via §7.1 echo counter | `0x07010005` |
| Subscribed MID has no live subscriber | cFE SB | Drop at SB (cFE default) | cFE-generated |
| `sim_adapter` stopped or crashed | `fault_injector` | Scenario error — halt scenario | scenario runner |

## 9. Compliance Matrix

### 9.1 Fault-Injection Packets (normative, exclusive use)

| PKT-ID | APID | Function code | Body defined in | Routing |
|---|---|---|---|---|
| PKT-SIM-0540-0001 | `0x540` | `0x0001` | [packet-catalog §7.1](packet-catalog.md) | §3.1 |
| PKT-SIM-0541-0001 | `0x541` | `0x0001` | [packet-catalog §7.2](packet-catalog.md) | §3.2 |
| PKT-SIM-0542-0001 | `0x542` | `0x0001` | [packet-catalog §7.3](packet-catalog.md) | §3.3 |
| PKT-SIM-0543-0001 | `0x543` | `0x0001` | [packet-catalog §7.4](packet-catalog.md) | §3.4 |

### 9.2 Sensor-Injection Packets (sibling range)

| PKT-ID | APID | Function code | Body defined in | Routing |
|---|---|---|---|---|
| PKT-SIM-0500-0001 | `0x500` | `0x0001` | this ICD §6.1 | §6 |

### 9.3 FSW → Sim Diagnostics

| PKT-ID | APID | Function code | Body defined in | Emitter |
|---|---|---|---|---|
| PKT-SIM-0570-0001 | `0x570` | `0x0001` | this ICD §7.1 | `sim_adapter` |

### 9.4 Cross-Reference to Q-* Decisions

| Decision | How this ICD honors it |
|---|---|
| [Q-C8](../standards/decisions-log.md) Big-endian + conversion locus | All fields BE; SPP header decoding in `sim_adapter` uses `cfs_bindings` (FSW side); `fault_injector` Rust-side uses `ccsds_wire` per §1 |
| [Q-F1](../standards/decisions-log.md) Fault injection via cFE SB | `sim_adapter` always terminates with `CFE_SB_Publish`; no direct memory writes to subscriber state |
| [Q-F2](../standards/decisions-log.md) Minimum fault set on `0x540`–`0x543` | Enforced in §2 reservation; §3.1–§3.4 define each |
| [Q-F3](../standards/decisions-log.md) `Vault<T>` / `.critical_mem` protection | §3.2 Clock Skew must target read-hook MID, not time-store MID, to preserve `.critical_mem` integrity |

## 10. What this ICD is NOT

- Not a Gazebo plugin design — the sensor-bridge side of Gazebo is in [`../architecture/05-simulation-gazebo.md`](../architecture/05-simulation-gazebo.md) (Batch B3).
- Not a cFS app-design doc — the `sim_adapter` app specification is in [`../architecture/01-orbiter-cfs.md`](../architecture/01-orbiter-cfs.md) (Batch B3). This ICD is the interface contract it must honor.
- Not a scenario-scripting guide — scenario YAML schema and execution live in [`../dev/simulation-walkthrough.md`](../dev/simulation-walkthrough.md) (Batch B5).
- Not a flight-safety certification — flight-build exclusion is enforced in §5, but the formal argument for why this is safe is a Phase C verification artifact.
