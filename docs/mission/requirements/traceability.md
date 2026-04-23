# Requirement Traceability Index

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). SRDs: [`SRD.md`](SRD.md), [`FSW-SRD.md`](FSW-SRD.md), [`GND-SRD.md`](GND-SRD.md), [`ROVER-SRD.md`](ROVER-SRD.md). V&V plan: [`../verification/V&V-Plan.md`](../verification/V&V-Plan.md). Compliance: [`../verification/compliance-matrix.md`](../verification/compliance-matrix.md). Linter: [`../../../scripts/traceability-lint.py`](../../../scripts/traceability-lint.py).

This is the **single index** that ties every `XXX-REQ-####` back to its definition site, parent, verification method, applicable phase, and V&V artefact. The [`traceability-lint.py`](../../../scripts/traceability-lint.py) linter enforces that:

- Every requirement ID defined in any SRD appears exactly once as a row here.
- Every parent link resolves to a row here.
- Every V&V pointer resolves to a section in [`V&V-Plan.md`](../verification/V&V-Plan.md) or a test artefact under `apps/`, `ros2_ws/`, `rust/`.

## 1. Conventions

- **ID**: `SYS-REQ-####`, `FSW-REQ-####`, `GND-REQ-####`, `ROV-REQ-####` — immutable once assigned, never reused.
- **Definition**: doc + section anchor where the requirement is stated.
- **Parent**: upstream `XXX-REQ-####`, or `—` for root (SYS-REQ rows), or `derived` with a reason.
- **Method**: **T** Test, **A** Analysis, **I** Inspection, **D** Demonstration (per NPR 7123.1C §3.1.7).
- **Phase**: from [`../conops/mission-phases.md`](../conops/mission-phases.md); `ALL` if every phase.
- **V&V artefact**: [`V&V-Plan.md`](../verification/V&V-Plan.md) section or a test directory.

Full statement text lives in the definition doc; this index intentionally omits it to stay scannable.

## 2. SYS-REQ — System-Level (34 reqs)

Defined in [`SRD.md`](SRD.md).

| ID | Topic | Parent | Method | Phase | V&V |
|---|---|---|---|---|---|
| SYS-REQ-0001 | MVC fleet composition | — | I, D | ALL | [V&V §3](../verification/V&V-Plan.md) |
| SYS-REQ-0002 | Config-only scaling to Scale-5 | — | I, A | ALL | [V&V §10](../verification/V&V-Plan.md) |
| SYS-REQ-0003 | Above-link-physics simulation | — | I | ALL | [V&V §2.3](../verification/V&V-Plan.md) |
| SYS-REQ-0010 | Four-segment partition | — | I | ALL | [compliance §2](../verification/compliance-matrix.md) |
| SYS-REQ-0011 | Segments are deployable containers | — | I | ALL | [`../../dev/docker-runbook.md`](../../dev/docker-runbook.md) |
| SYS-REQ-0020 | CCSDS SPP at L3 | — | I, T | ALL | unit + SCN-NOM-01 |
| SYS-REQ-0021 | 10-byte secondary header | — | T, I | ALL | `ccsds_wire` proptest |
| SYS-REQ-0022 | BE on wire + conversion locus | — | I | ALL | grep + proptest |
| SYS-REQ-0023 | AOS 1024 B / TC SDLP | — | T, I | P1–P6 | [ICD-orbiter-ground](../../interfaces/ICD-orbiter-ground.md) |
| SYS-REQ-0024 | Proximity-1 on rovers | — | T, I | P3–P5 | SCN-NOM-01 |
| SYS-REQ-0025 | CFDP Class 1 on VC 2 | — | T | P4 | `ground_station` integration |
| SYS-REQ-0026 | Class-based APID allocation | — | I | ALL | APID/MID linter |
| SYS-REQ-0030 | TAI on-board, UTC at UI | — | I, T | ALL | `ccsds_wire::cuc` |
| SYS-REQ-0031 | Time-authority ladder | — | I | ALL | integration |
| SYS-REQ-0032 | 50 ms / 4 h LOS drift budget | — | A, T | ALL | drift analysis + integration |
| SYS-REQ-0033 | 1 ms fleet-sync precision | — | A, T | ALL | integration |
| SYS-REQ-0034 | `time_suspect` flag on TM | — | T | ALL | [V&V §4.1 `0x541`](../verification/V&V-Plan.md) |
| SYS-REQ-0040 | Functional fault injection `0x540`–`0x543` | — | T, D | ALL | [V&V §4](../verification/V&V-Plan.md) |
| SYS-REQ-0041 | Flight-build blocks sim APIDs | — | I, T | ALL | `scripts/check_sim_apids.py` + unit |
| SYS-REQ-0042 | `.critical_mem` / `Vault<T>` placement | — | I | ALL | grep + review |
| SYS-REQ-0043 | Clock-skew via read-hook only | — | T | ALL | [V&V §4.2 Q-F3](../verification/V&V-Plan.md) |
| SYS-REQ-0050 | Safe-mode entry path | — | T, D | ALL | [V&V §4.1 `0x542`](../verification/V&V-Plan.md) |
| SYS-REQ-0051 | No autonomous safe-mode exit | — | I, T | P6 | integration |
| SYS-REQ-0052 | Event propagation ≤ light-time + 2×retx | — | T | ALL | [V&V §3.2 TC-SCN-OFF-01-C](../verification/V&V-Plan.md) |
| SYS-REQ-0060 | UTC UI + light-time indicator | — | D, I | ALL | SCN-NOM-01 |
| SYS-REQ-0061 | Expired-window command rejection | — | T | ALL | ground-station unit |
| SYS-REQ-0070 | `-Werror` C17 clean | — | T | ALL | CI (planned) |
| SYS-REQ-0071 | `cargo clippy -D warnings` | — | T, I | ALL | CI (planned) |
| SYS-REQ-0072 | No dynamic alloc in `apps/` | — | I | ALL | cppcheck + review |
| SYS-REQ-0073 | `cargo audit` HIGH/CRIT clean | — | T | ALL | CI (planned) |
| SYS-REQ-0074 | 100% branch coverage on C + Rust | — | A, T | ALL | tarpaulin + gcov |
| SYS-REQ-0080 | Every req in traceability | — | T | ALL | [traceability-lint.py](../../../scripts/traceability-lint.py) |
| SYS-REQ-0081 | Every standards ID in references | — | T | ALL | citation linter (planned) |
| SYS-REQ-0082 | Every Q-\* has a status row | — | I | ALL | grep + review |

## 3. FSW-REQ — Flight Software (31 reqs)

Defined in [`FSW-SRD.md`](FSW-SRD.md).

| ID | Topic | Parent | Method | Phase | V&V |
|---|---|---|---|---|---|
| FSW-REQ-0001 | `CFE_ES_RegisterApp()` first | SYS-REQ-0001 | I | ALL | code review |
| FSW-REQ-0002 | SB-only inter-app comms | SYS-REQ-0020 | I | ALL | grep + review |
| FSW-REQ-0003 | MIDs via named macros | SYS-REQ-0026 | I | ALL | APID/MID linter |
| FSW-REQ-0004 | `CFE_EVS_SendEvent` only | SYS-REQ-0070 | I, T | ALL | grep |
| FSW-REQ-0005 | Command-dispatch `switch`+`default` | derived | I | ALL | code review |
| FSW-REQ-0006 | ≥1 event type per app | derived | I | ALL | grep |
| FSW-REQ-0007 | Named SB pipe-depth constants | derived | I | ALL | grep |
| FSW-REQ-0008 | HK via SCH entry only | SYS-REQ-0020 | I, T | ALL | integration |
| FSW-REQ-0009 | Clean `APP_EXIT` handling | derived | T | ALL | unit |
| FSW-REQ-0020 | No dynamic alloc in `apps/` | SYS-REQ-0072 | I | ALL | cppcheck |
| FSW-REQ-0021 | No VLAs | derived (MISRA) | I, A | ALL | cppcheck |
| FSW-REQ-0022 | Length-bounded string ops | derived | I | ALL | cppcheck + grep |
| FSW-REQ-0023 | Bounds-checked indices | derived | I | ALL | cppcheck |
| FSW-REQ-0024 | `.critical_mem` on rad-sensitive state | SYS-REQ-0042 | I | ALL | grep + review |
| FSW-REQ-0030 | cFS time via `CFE_TIME` | SYS-REQ-0030 | I | ALL | integration |
| FSW-REQ-0031 | Relay `tai_ns` in `.critical_mem` | SYS-REQ-0030, SYS-REQ-0042 | I, T | ALL | unit + integration |
| FSW-REQ-0032 | MCUs are time slaves | SYS-REQ-0031 | I, T | ALL | integration |
| FSW-REQ-0033 | `time_suspect` on miss | SYS-REQ-0034 | T | ALL | [V&V §4.1](../verification/V&V-Plan.md) |
| FSW-REQ-0040 | `sim_adapter` CRC + SB publish | SYS-REQ-0040, SYS-REQ-0041 | T, I | ALL | [V&V §4](../verification/V&V-Plan.md) |
| FSW-REQ-0041 | `TO` egress filter | SYS-REQ-0041 | T | ALL | unit |
| FSW-REQ-0042 | Clock-skew at read hook | SYS-REQ-0043 | T | ALL | [V&V §4.2](../verification/V&V-Plan.md) |
| FSW-REQ-0043 | Per-asset fault-set consumers | SYS-REQ-0040 | T | ALL | [V&V §4.1](../verification/V&V-Plan.md) |
| FSW-REQ-0050 | FSW safe-mode entry path | SYS-REQ-0050 | T, D | ALL | [V&V §3.2](../verification/V&V-Plan.md) |
| FSW-REQ-0051 | Per-asset safe states | SYS-REQ-0050 | T | P6 | integration |
| FSW-REQ-0052 | EVS event on safe-mode entry | SYS-REQ-0052 | T | ALL | [V&V §3.2](../verification/V&V-Plan.md) |
| FSW-REQ-0060 | SpW for `mcu_payload` | SYS-REQ-0024 derived | I, T | P4, P5 | integration |
| FSW-REQ-0061 | CAN 2.0A for `mcu_rwa`, ≤ 20 ms latency | SYS-REQ-0024 derived | T, A | P4 | integration |
| FSW-REQ-0062 | UART/HDLC for `mcu_eps` | SYS-REQ-0024 derived, SYS-REQ-0050 | T | P4, P6 | integration |
| FSW-REQ-0063 | MCU task skeleton + priorities | derived | I | ALL | code review |
| FSW-REQ-0070 | Per-app CMocka test dir | SYS-REQ-0074 | I, T | ALL | `ctest` |
| FSW-REQ-0071 | CFE stubs under `#ifdef UNIT_TEST` | derived | I | ALL | build audit |

## 4. GND-REQ — Ground Segment (24 reqs)

Defined in [`GND-SRD.md`](GND-SRD.md).

| ID | Topic | Parent | Method | Phase | V&V |
|---|---|---|---|---|---|
| GND-REQ-0001 | Cargo workspace crate set | SYS-REQ-0011 | I | ALL | `Cargo.toml` |
| GND-REQ-0002 | Conversion locus = `ccsds_wire` + `cfs_bindings` | SYS-REQ-0022 | I, T | ALL | grep + proptest |
| GND-REQ-0003 | CFDP Class 1 via `CfdpProvider` | SYS-REQ-0025, SYS-REQ-0020 | I, T | P4 | integration |
| GND-REQ-0010 | Crate-root `#![deny(clippy::all)]` | SYS-REQ-0071 | I | ALL | grep |
| GND-REQ-0011 | clippy `-D warnings` clean | SYS-REQ-0071 | T | ALL | CI (planned) |
| GND-REQ-0012 | No `unwrap()` outside tests | derived | I | ALL | grep |
| GND-REQ-0013 | `// SAFETY:` on every `unsafe` | derived (security) | I | ALL | grep + review |
| GND-REQ-0014 | `cargo audit` clean | SYS-REQ-0073 | T | ALL | CI (planned) |
| GND-REQ-0020 | `Vault<T>` on rad-sensitive state | SYS-REQ-0042 | I | ALL | code review |
| GND-REQ-0030 | `ApidRouter` rejects sim-block egress | SYS-REQ-0041 | T | ALL | unit |
| GND-REQ-0031 | AOS decode + FECF check | SYS-REQ-0023 | T | P1–P6 | unit + integration |
| GND-REQ-0032 | Unknown-APID log, not silent drop | derived | T | ALL | unit |
| GND-REQ-0040 | UTC + ISO-8601 timestamps | SYS-REQ-0060 | D | ALL | SCN-NOM-01 |
| GND-REQ-0041 | Light-time indicator | SYS-REQ-0060 | D | ALL | SCN-NOM-01 |
| GND-REQ-0042 | Reject expired-window commands | SYS-REQ-0061 | T, D | ALL | UI test |
| GND-REQ-0043 | Safe-mode UI within bounded latency | SYS-REQ-0052 | T, D | ALL | SCN-OFF-01 |
| GND-REQ-0050 | UTC UI / TAI logs | SYS-REQ-0030 | I, T | ALL | unit |
| GND-REQ-0051 | 1 TCP / 60 s to orbiter | SYS-REQ-0031 | T | ALL | integration |
| GND-REQ-0052 | Leap-second as compile-time constant | derived | I | ALL | code review |
| GND-REQ-0060 | CFDP Class 1 parameters | SYS-REQ-0025 | T | P4 | integration |
| GND-REQ-0061 | CFDP 100 min transaction timeout | derived | T | P4 | unit + integration |
| GND-REQ-0062 | Max 16 concurrent CFDP transactions | derived | T | P4 | unit |
| GND-REQ-0070 | Tests in every crate | SYS-REQ-0074 | I, T | ALL | `cargo test` |
| GND-REQ-0071 | proptest on parsers/encoders | derived | T | ALL | `cargo test` |

## 5. ROV-REQ — Surface Rovers (25 reqs)

Defined in [`ROVER-SRD.md`](ROVER-SRD.md).

| ID | Topic | Parent | Method | Phase | V&V |
|---|---|---|---|---|---|
| ROV-REQ-0001 | `LifecycleNode` subclass required | SYS-REQ-0001 derived | I | ALL | grep |
| ROV-REQ-0002 | All five lifecycle callbacks | derived | T | ALL | `colcon test` |
| ROV-REQ-0003 | Non-blocking callbacks (≤ 1 ms) | derived | T, I | ALL | launch_testing |
| ROV-REQ-0004 | QoS as file-scope named constants | derived | I | ALL | grep |
| ROV-REQ-0005 | Log via `RCLCPP_INFO` only | SYS-REQ-0070 | I | ALL | grep |
| ROV-REQ-0010 | `tm_bridge` per class | SYS-REQ-0020 | I, T | ALL | integration |
| ROV-REQ-0011 | `rover_land` node composition | derived | I | P3–P5 | inspection |
| ROV-REQ-0012 | `rover_uav` node composition | derived | I | P3, P4 | inspection |
| ROV-REQ-0013 | `rover_cryobot` node composition | derived | I | P5 | inspection |
| ROV-REQ-0020 | Proximity-1 1024 B Reliable | SYS-REQ-0024 | T | P3–P5 | integration |
| ROV-REQ-0021 | 1 Hz HK; elevated rates per catalog | derived | T | P4 | SCN-NOM-01 |
| ROV-REQ-0022 | HDLC-lite tether framing | SYS-REQ-0020 | T | P5 | integration |
| ROV-REQ-0023 | CRC-16 + BW-collapse transition | derived | T | P5 | SCN-OFF-01 |
| ROV-REQ-0030 | `use_sim_time: true` under SITL | SYS-REQ-0030 | I, T | ALL | launch_testing |
| ROV-REQ-0031 | Only `time_bridge` crosses non-ROS→ROS | derived | I | ALL | grep |
| ROV-REQ-0032 | `time_bridge` consumes CCSDS tags | SYS-REQ-0031 | T | ALL | integration |
| ROV-REQ-0040 | Rover safe-mode → nodes `inactive` | SYS-REQ-0050 | T | P6 | integration |
| ROV-REQ-0041 | Cryobot `HOLD` fate-share watchdog | SYS-REQ-0050 | T | P5 | [V&V §3.2 TC-SCN-OFF-01-D](../verification/V&V-Plan.md) |
| ROV-REQ-0042 | No late-executed cryobot commands | SYS-REQ-0052 | T | P5 | [V&V §3.2](../verification/V&V-Plan.md) |
| ROV-REQ-0050 | Sensor data via ROS 2 topics | SYS-REQ-0041 | I | ALL | inspection |
| ROV-REQ-0051 | Sensor-noise injection at plugin output | SYS-REQ-0041 | T | ALL | integration |
| ROV-REQ-0060 | `rover_<class>` naming | SYS-REQ-0002 | I | ALL | inspection |
| ROV-REQ-0061 | Per-instance via launch params | SYS-REQ-0002 | I, T | ALL | launch_testing |
| ROV-REQ-0070 | Per-package test dir | SYS-REQ-0074 | I, T | ALL | `colcon test` |
| ROV-REQ-0071 | launch_testing + pytest | derived | I, T | ALL | `colcon test` |

## 6. Parent Integrity

The linter enforces that every `parent:` link resolves to a defined ID above. Current non-SYS parents (derived → some rationale) are intentional — these are sub-SRD-internal decisions whose motivation is documented in the sub-SRD section heading.

## 7. V&V Artefact Coverage

Every V&V artefact column pointer either:

- Resolves to a section in [`V&V-Plan.md`](../verification/V&V-Plan.md).
- Points at a test directory (`apps/**/fsw/unit-test/`, `ros2_ws/**/test/`, `rust/*/tests/`).
- Names a linter (`cppcheck`, `grep`, `traceability-lint.py`, future CI checks).
- Is "code review" — human review; tracked but not automated.

Pointers that say "CI (planned)" are open items — see [`V&V-Plan §10`](../verification/V&V-Plan.md).

## 8. Decisions referenced by requirements

Every requirement that cites a Q-\* decision is indexed here for reverse lookup:

| Q-ID | Requirements that cite it |
|---|---|
| [Q-C2](../../standards/decisions-log.md) | GND-REQ-0060 |
| [Q-C3](../../standards/decisions-log.md) | GND-REQ-0003 |
| [Q-C4](../../standards/decisions-log.md) | SYS-REQ-0023 |
| [Q-C6](../../standards/decisions-log.md) | SYS-REQ-0021 |
| [Q-C8](../../standards/decisions-log.md) | SYS-REQ-0022, GND-REQ-0002 |
| [Q-C9](../../standards/decisions-log.md) | ROV-REQ-0022, ROV-REQ-0023 |
| [Q-F1](../../standards/decisions-log.md) | SYS-REQ-0040 |
| [Q-F2](../../standards/decisions-log.md) | SYS-REQ-0040 |
| [Q-F3](../../standards/decisions-log.md) | SYS-REQ-0042, SYS-REQ-0043, FSW-REQ-0024, FSW-REQ-0042, GND-REQ-0020 |
| [Q-F4](../../standards/decisions-log.md) | SYS-REQ-0031, SYS-REQ-0032 |
| [Q-F6](../../standards/decisions-log.md) | SYS-REQ-0033 |
| [Q-H2](../../standards/decisions-log.md) | SYS-REQ-0002 |
| [Q-H4](../../standards/decisions-log.md) | FSW-REQ-0060, FSW-REQ-0061, FSW-REQ-0062 |
| [Q-H8](../../standards/decisions-log.md) | — (deferred; tracked in [SRD §10](SRD.md)) |

## 9. Sunset Policy

Requirements are never deleted — they are marked `withdrawn: ReasonCode` (e.g. `scope-change`, `superseded-by: SYS-REQ-####`) and stay in this index. The linter accepts `withdrawn:` rows and does not require them to have V&V coverage.

No requirements are withdrawn as of Phase C initial authoring.

## 10. What this index is NOT

- Not the requirements themselves — those live in the SRDs (this is just the map).
- Not a test-case catalogue — [`V&V-Plan.md`](../verification/V&V-Plan.md) and the tests themselves own that.
- Not a compliance matrix — [`compliance-matrix.md`](../verification/compliance-matrix.md) binds requirements to external standards.
- Not a decisions log — [`../../standards/decisions-log.md`](../../standards/decisions-log.md) owns Q-\* entries; §8 above is a reverse index only.
