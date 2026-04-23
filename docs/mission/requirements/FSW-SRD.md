# Flight Software Sub-System Requirements Document (FSW-SRD)

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Parent SRD: [`SRD.md`](SRD.md). Traceability: [`traceability.md`](traceability.md). V&V: [`../verification/V&V-Plan.md`](../verification/V&V-Plan.md). Architecture: [`../../architecture/01-orbiter-cfs.md`](../../architecture/01-orbiter-cfs.md) (cFS orbiter), [`../../architecture/02-smallsat-relay.md`](../../architecture/02-smallsat-relay.md) (FreeRTOS relay), [`../../architecture/03-subsystem-mcus.md`](../../architecture/03-subsystem-mcus.md) (FreeRTOS MCUs). Coding rules: [`../../../.claude/rules/cfs-apps.md`](../../../.claude/rules/cfs-apps.md), [`general.md`](../../../.claude/rules/general.md), [`security.md`](../../../.claude/rules/security.md).

This sub-SRD covers **flight software** — code that executes on orbiters, the smallsat relay, and subsystem MCUs. Per [`SRD.md §1`](SRD.md), every requirement here carries a `parent:` link to a `SYS-REQ-####` or declares "derived" with a justification.

Scope is `apps/**` (cFS apps) and the FreeRTOS firmware sets for relay and MCUs. Out of scope: Space ROS 2 rover firmware ([`ROVER-SRD.md`](ROVER-SRD.md)), Rust ground station ([`GND-SRD.md`](GND-SRD.md)), Gazebo plugins (non-flight).

## 1. Record format

Same as [`SRD.md §1`](SRD.md), plus one column:

| Column | Meaning |
|---|---|
| **Parent** | Parent `SYS-REQ-####` (or "derived" with rationale) |

Verification methods: **T** Test, **A** Analysis, **I** Inspection, **D** Demonstration (NPR 7123.1C §3.1.7).

## 2. cFS Application Pattern

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| FSW-REQ-0001 | Every cFS application's `AppMain()` shall call `CFE_ES_RegisterApp()` as its first cFE service call. | SYS-REQ-0001 | [.claude/rules/cfs-apps.md](../../../.claude/rules/cfs-apps.md) | I | ALL | code review |
| FSW-REQ-0002 | cFS applications shall communicate with each other only via the cFE Software Bus (`CFE_SB_Subscribe`, `CFE_SB_ReceiveBuffer`); direct function calls between apps are banned. | SYS-REQ-0020 | [01 §2](../../architecture/01-orbiter-cfs.md), [.claude/rules/cfs-apps.md](../../../.claude/rules/cfs-apps.md) | I | ALL | grep + review |
| FSW-REQ-0003 | cFE Message IDs shall be defined as named macros in `_defs/` or the app's `msgids.h`; literal MID values shall not appear in application source. | SYS-REQ-0026 | [.claude/rules/cfs-apps.md](../../../.claude/rules/cfs-apps.md) | I | ALL | APID/MID consistency linter |
| FSW-REQ-0004 | Runtime messaging (status, error, info) shall use `CFE_EVS_SendEvent`; `printf`, `OS_printf`, and `fprintf` are banned in flight path code. | SYS-REQ-0070 | [.claude/rules/general.md](../../../.claude/rules/general.md), [.claude/rules/cfs-apps.md](../../../.claude/rules/cfs-apps.md) | I, T | ALL | grep lint |
| FSW-REQ-0005 | Command dispatch shall use a single `switch` on the secondary-header function code with an explicit `default:` branch that increments the app's error counter and emits an "invalid command" event. | derived (MISRA + cFS idiom) | [01 §7.1](../../architecture/01-orbiter-cfs.md), [.claude/rules/cfs-apps.md](../../../.claude/rules/cfs-apps.md) | I | ALL | code review |
| FSW-REQ-0006 | Every cFS app shall register at least one event type in its `<app>_events.h`. | derived | [CLAUDE.md §Conventions](../../../CLAUDE.md), [01 §7.2](../../architecture/01-orbiter-cfs.md) | I | ALL | grep |
| FSW-REQ-0007 | cFS software-bus pipe depth shall be a named constant; literal integers shall not be passed to `CFE_SB_CreatePipe`. | derived | [.claude/rules/cfs-apps.md](../../../.claude/rules/cfs-apps.md) | I | ALL | grep |
| FSW-REQ-0008 | Every cFS mission application shall produce exactly one housekeeping packet per its assigned APID, triggered by an SCH table entry (not by the app's own timer). | SYS-REQ-0020 | [01 §7.3](../../architecture/01-orbiter-cfs.md) | I, T | ALL | integration |
| FSW-REQ-0009 | Apps shall handle `CFE_ES_RUNSTATUS_APP_EXIT` by releasing software-bus pipes and any other resources before returning from `AppMain`. | derived | [.claude/rules/cfs-apps.md](../../../.claude/rules/cfs-apps.md) | T | ALL | unit test |

## 3. Memory and Stack Discipline

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| FSW-REQ-0020 | Dynamic memory allocation (`malloc`/`calloc`/`realloc`/`free`) shall not be used in `apps/**` flight-path code. | SYS-REQ-0072 | [.claude/rules/general.md](../../../.claude/rules/general.md) | I | ALL | cppcheck |
| FSW-REQ-0021 | Variable-length arrays (MISRA C:2012 Rule 18.8) shall not be used; stack depth shall be statically bounded. | derived (MISRA) | [.claude/rules/general.md](../../../.claude/rules/general.md), [.claude/rules/security.md](../../../.claude/rules/security.md) | I, A | ALL | cppcheck |
| FSW-REQ-0022 | String operations shall use length-bounded primitives (`snprintf`, `strncpy`, `strncat`) with explicit length limits; `sprintf`, `strcpy`, `strcat` are banned. | derived (security) | [.claude/rules/security.md](../../../.claude/rules/security.md) | I | ALL | cppcheck + grep |
| FSW-REQ-0023 | Array indices derived from arithmetic shall be bounds-checked before use; arithmetic that could overflow or underflow prior to indexing shall be guarded. | derived | [.claude/rules/security.md](../../../.claude/rules/security.md) | I | ALL | cppcheck |
| FSW-REQ-0024 | Radiation-sensitive static state (cFE TIME internals, mode register, fault counters) shall be placed in the `.critical_mem` linker section via `__attribute__((section(".critical_mem")))` with a MISRA Rule 8.11 deviation comment. | SYS-REQ-0042 | [09 §5.1](../../architecture/09-failure-and-radiation.md), [.claude/rules/cfs-apps.md](../../../.claude/rules/cfs-apps.md) | I | ALL | grep + code review |

## 4. Time and Clocks

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| FSW-REQ-0030 | cFS orbiters shall source time via `CFE_TIME`; `CFE_TIME_SysTime_t` shall be converted to CCSDS CUC (7 B) only by the comms framing layer. | SYS-REQ-0030 | [08 §5.1](../../architecture/08-timing-and-clocks.md) | I | ALL | integration |
| FSW-REQ-0031 | The FreeRTOS relay shall maintain a 64-bit `tai_ns` value placed in `.critical_mem`, updated from periodic tick + orbiter cross-link time tag, and exposed via `time_now_cuc(uint8_t out[7])`. | SYS-REQ-0030, SYS-REQ-0042 | [08 §5.2](../../architecture/08-timing-and-clocks.md) | I, T | ALL | unit + integration |
| FSW-REQ-0032 | Subsystem MCUs shall be time slaves — they shall consume a periodic time-sync packet from the cFS gateway and shall not run their own correlation logic. | SYS-REQ-0031 | [08 §5.3](../../architecture/08-timing-and-clocks.md), [03 §5](../../architecture/03-subsystem-mcus.md) | I, T | ALL | integration |
| FSW-REQ-0033 | Any FSW asset whose time-sync has been missed beyond its threshold (orbiter: 4 h LOS; relay: 4 h LOS; MCU: 5 s) shall set the `time_suspect` flag on outgoing telemetry. | SYS-REQ-0034 | [08 §4](../../architecture/08-timing-and-clocks.md), [03 §5](../../architecture/03-subsystem-mcus.md) | T | ALL | [V&V §4.1 `0x541` gate](../verification/V&V-Plan.md) |

## 5. Failure and Fault Injection

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| FSW-REQ-0040 | The cFS `sim_adapter` application shall accept SPPs on APIDs `0x540`–`0x543` only under a non-`CFS_FLIGHT_BUILD` compile, validate the CRC-16/CCITT-FALSE trailer, and publish the decoded message on the cFE Software Bus. | SYS-REQ-0040, SYS-REQ-0041 | [ICD-sim-fsw §4](../../interfaces/ICD-sim-fsw.md) | T, I | ALL | [V&V §4](../verification/V&V-Plan.md) |
| FSW-REQ-0041 | The orbiter `TO` egress filter shall reject any Software Bus message whose APID resolves within `0x500`–`0x57F` from being emitted on the ground downlink, regardless of whether a subscriber exists. | SYS-REQ-0041 | [01 §11](../../architecture/01-orbiter-cfs.md), [07 §8](../../architecture/07-comms-stack.md) | T | ALL | unit test |
| FSW-REQ-0042 | Clock-skew injection consumers (`CFE_TIME` read wrapper, FreeRTOS `time_task` read wrapper) shall not overwrite the `.critical_mem`-resident time store; they shall compose the skew at the read boundary only. | SYS-REQ-0043 | [09 §5.3](../../architecture/09-failure-and-radiation.md) | T | ALL | [V&V §4.2 Q-F3 regression](../verification/V&V-Plan.md) |
| FSW-REQ-0043 | Each FSW asset class shall implement the four minimum-fault-set consumers per [09 §4](../../architecture/09-failure-and-radiation.md) (orbiter, relay, MCU, rover) — packet drop, clock skew, force safe-mode, sensor-noise. | SYS-REQ-0040 | [09 §4](../../architecture/09-failure-and-radiation.md) | T | ALL | [V&V §4.1](../verification/V&V-Plan.md) |

## 6. Safe-Mode

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| FSW-REQ-0050 | Every FSW asset shall implement a safe-mode entry path triggered by local invariant violation, watchdog timeout, or `0x542` force-safe command. | SYS-REQ-0050 | [ConOps §7](../conops/ConOps.md), [01 §11](../../architecture/01-orbiter-cfs.md), [02 §5](../../architecture/02-smallsat-relay.md), [03 §8](../../architecture/03-subsystem-mcus.md) | T, D | ALL | [V&V §3.2](../verification/V&V-Plan.md) |
| FSW-REQ-0051 | Per-asset safe states shall be: orbiter — autonomous-SCLK, HK continues, no actuation; relay — store-and-forward holds, HK continues; MCU — role-specific (RWA zero torque, EPS hold switches, payload sensors off). | SYS-REQ-0050 | [`mission-phases.md §3.6`](../conops/mission-phases.md) | T | P6 | integration |
| FSW-REQ-0052 | Safe-mode entry shall emit a `PKT-TM-*-0004` EVS event with `event_id` encoding the trigger reason within N ms of detection (N from [V&V-Plan](../verification/V&V-Plan.md)). | SYS-REQ-0052 | [ICD-sim-fsw §3.3](../../interfaces/ICD-sim-fsw.md) | T | ALL | [V&V §3.2](../verification/V&V-Plan.md) |

## 7. Bus Drivers (MCU side)

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| FSW-REQ-0060 | `mcu_payload` shall communicate with the orbiter over SpaceWire (ECSS-E-ST-50-12C). | SYS-REQ-0024 derived | [Q-H4](../../standards/decisions-log.md), [03 §1](../../architecture/03-subsystem-mcus.md) | I, T | P4, P5 | integration |
| FSW-REQ-0061 | `mcu_rwa` shall communicate with the orbiter over CAN 2.0A (ISO 11898); command latency shall be ≤ 20 ms end-to-end from cFS dispatch. | SYS-REQ-0024 derived | [Q-H4](../../standards/decisions-log.md), [03 §4.2](../../architecture/03-subsystem-mcus.md) | T, A | P4 | integration |
| FSW-REQ-0062 | `mcu_eps` shall communicate with the orbiter over UART with HDLC framing (RFC 1662) and shall accept load-shed commands from the orbiter safe-mode controller. | SYS-REQ-0024 derived, SYS-REQ-0050 | [Q-H4](../../standards/decisions-log.md), [03 §4.3](../../architecture/03-subsystem-mcus.md) | T | P4, P6 | integration |
| FSW-REQ-0063 | Each MCU class shall implement the common FreeRTOS task skeleton per [03 §3](../../architecture/03-subsystem-mcus.md) (`bus_rx`, `bus_tx`, `app_logic`, `hk_emitter`, `clock_slave`, `health`) with priorities 7, 7, 5, 4, 6, 8 respectively. | derived | [03 §3.1](../../architecture/03-subsystem-mcus.md) | I | ALL | code review |

## 8. Test Coverage

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| FSW-REQ-0070 | Every `apps/<app>/` directory shall carry a `fsw/unit-test/` directory with a CMocka test file containing at least one failure-path test. | SYS-REQ-0074 | [.claude/rules/testing.md](../../../.claude/rules/testing.md) | I, T | ALL | `ctest` |
| FSW-REQ-0071 | cFE and OSAL stubs in unit tests shall compile under `#ifdef UNIT_TEST` only; tests shall not link against the real cFE library. | derived | [.claude/rules/testing.md](../../../.claude/rules/testing.md) | I | ALL | build audit |

## 9. Open / Deferred

- SEU bit-flip consumer (Phase-B-plus).
- TMR voter wiring on `CFE_TIME` ([08 §8](../../architecture/08-timing-and-clocks.md)).
- MCU silicon selection + per-chip driver ([03 §10 open](../../architecture/03-subsystem-mcus.md)).
- HPSC cross-build target ([Q-H8](../../standards/decisions-log.md)).

## 10. What this sub-SRD is NOT

- Not a coding rulebook. Rules live in [`.claude/rules/`](../../../.claude/rules/).
- Not a segment architecture doc — architecture rationale lives in [01](../../architecture/01-orbiter-cfs.md), [02](../../architecture/02-smallsat-relay.md), [03](../../architecture/03-subsystem-mcus.md).
- Not an ICD — boundary contracts live in [`../../interfaces/`](../../interfaces/).
- Not a V&V artefact catalogue — [`../verification/V&V-Plan.md`](../verification/V&V-Plan.md) owns that.
