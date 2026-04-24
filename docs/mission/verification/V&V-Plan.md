# SAKURA-II Verification & Validation Plan

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Bibliography: [../../standards/references.md](../../standards/references.md). ConOps: [../conops/ConOps.md](../conops/ConOps.md). System of Systems: [../../architecture/00-system-of-systems.md](../../architecture/00-system-of-systems.md). Failure model: [../../architecture/09-failure-and-radiation.md](../../architecture/09-failure-and-radiation.md). Timing model: [../../architecture/08-timing-and-clocks.md](../../architecture/08-timing-and-clocks.md). Testing rules: [../../../.claude/rules/testing.md](../../../.claude/rules/testing.md). Decisions: [../../standards/decisions-log.md](../../standards/decisions-log.md).

This plan fixes **what SAKURA-II verifies, how, and against which standards**. It is the reference every test case, CI gate, and mission-assurance review cites. The plan is **illustrative on day 1 and defensible by Phase C close** — per [`../../README.md` decisions table](../../README.md), the project evolves toward a PDR-surrogate posture; this plan's structure is what that evolution populates.

## 1. Purpose & Scope

### 1.1 What this plan covers

- **Verification**: "Did we build the thing right?" — unit, integration, and system-level tests against architecture and ICD contracts.
- **Validation**: "Did we build the right thing?" — scenario-level tests against [ConOps §4–5](../conops/ConOps.md) and the requirements that will land in `mission/requirements/` (Phase C).
- Scope is the SITL demonstrator: cFS flight software, FreeRTOS relay/MCU firmware, Space ROS 2 rovers, Gazebo simulation, and the Rust ground station. Flight-hardware V&V (HPSC target) is deferred with [Q-H8](../../standards/decisions-log.md).

### 1.2 What this plan does NOT cover

- Per-asset hardware qualification (radiation, thermal, vibe) — out of scope; addressed at a future program tier that SAKURA-II surrogates but does not claim.
- Formal certification artefacts (DO-178C objective matrix, full ECSS-Q-ST-80C compliance). SAKURA-II *aligns where feasible* ([references.md §3](../../standards/references.md)) without claiming conformance.
- Test-case bodies themselves — those live next to the code (`apps/**/fsw/unit-test/`, `ros2_ws/**/test/`, `rust/*/tests/`). This plan catalogues **classes** and **gates**.

### 1.3 Posture and pedigree

Per [references.md](../../standards/references.md), the standards this plan leans on:

| Standard | Role in this plan |
|---|---|
| **NASA-STD-8739.8B** | Software assurance & safety anchor — §9 compliance map |
| **NASA-STD-7009A** | Models-and-simulations assurance — applies to the Gazebo + `fault_injector` stack |
| **NPR 7150.2D** | NASA software engineering — classification drives test-rigor tiers (§2) |
| **ECSS-E-ST-10-02C** | Verification — the structural spine this plan's class taxonomy borrows |
| **ECSS-Q-ST-80C Rev.1** | Software product assurance — Phase-C compliance matrix |
| **IEEE 1012** (implied) | Independent V&V — single-contributor in Phase B; tracked as an open role in §10 |

Citations stay single-line; bibliographic detail lives in [references.md](../../standards/references.md). If you need to cite a standard here that isn't in references.md, update references.md first — the citation-integrity linter (planned) enforces this.

## 2. V&V Classes

Per [ECSS-E-ST-10-02C §5.4](../../standards/references.md), verification uses three classes. SAKURA-II adds scenario-driven validation on top as a fourth.

### 2.1 Unit

Component-level tests inside a single stack. No inter-stack communication, no simulator, no network.

| Stack | Framework | Entry point | Coverage measurement |
|---|---|---|---|
| cFS / C | **CMocka** per [`.claude/rules/testing.md`](../../../.claude/rules/testing.md) | `apps/**/fsw/unit-test/*_test.c` | `gcov` or `llvm-cov --show-branches` |
| ROS 2 / C++ | ROS 2 package tests (gtest) | `ros2_ws/**/test/*.cpp` | (coverage target deferred to Phase C) |
| Rust | `#[cfg(test)]` + integration `tests/` | `rust/*/tests/`, in-module `#[test]` | `cargo tarpaulin --engine llvm --out Html` |

Commands (from [CLAUDE.md](../../../CLAUDE.md), not restated here): `ctest --test-dir build --output-on-failure`, `colcon test`, `cargo test`, `cargo tarpaulin --out Html`.

**Gate**: every `apps/<app>/` directory has a `fsw/unit-test/` directory with at least one failure-path test per [.claude/rules/testing.md](../../../.claude/rules/testing.md). Same discipline for `ros2_ws/**/test/` and `rust/*/tests/`. Target: **100 % branch coverage** on C and Rust per [CLAUDE.md §Coding Standards](../../../CLAUDE.md); exceptions recorded in [`deviations.md`](../../standards/deviations.md).

### 2.2 Integration

Multi-stack, single-process-tree tests. A cFS app talking to a simulated peer via the cFE Software Bus; a ROS 2 lifecycle node talking to a test double. No full sim, no Docker orchestration.

| Combination | Test host | Harness |
|---|---|---|
| cFS app ↔ cFS app via SB | Linux host | CMocka + cFE test doubles |
| ROS 2 node ↔ ROS 2 node | `colcon test` with launch_testing | launch_testing + pytest |
| `ccsds_wire` ↔ `cfs_bindings` | Rust `tests/` | Fuzz via `proptest` per [.claude/rules/testing.md](../../../.claude/rules/testing.md) |
| `sim_adapter` APID validation ↔ `fault_injector` output | Linux host | CMocka with recorded `0x540`–`0x543` bytes |

**Gate**: every ICD under [`../../interfaces/`](../../interfaces/) has at least one integration test that exercises its contract end-to-end without real network transport. Missing coverage is tracked in §10.

### 2.3 System / End-to-End

Full Docker compose stack: orbiter + relay + rovers + Gazebo + clock_link_model + ground station. Driven by a scenario YAML from `simulation/scenarios/*.yaml`. Docker-compose infrastructure itself is Phase-B deferred per [`../../dev/docker-runbook.md`](../../dev/docker-runbook.md); the test harness is designed against the target state.

**Gate scenarios** (see §3): every ConOps scenario has an E2E run that checks its success criteria.

### 2.4 Scenario-driven Validation

A superset of system testing — binds ConOps scenarios to requirement IDs. Distinct from §2.3 in that it answers "did the *mission* behave correctly" rather than "did the *code* run without crash." Per [NPR 7150.2D](../../standards/references.md), this tier is what mission assurance reviews against.

## 3. Scenario-Driven Tests

Every ConOps scenario gets a V&V entry. Phase B has two ([ConOps §4–5](../conops/ConOps.md)); Phase C will expand.

### 3.1 SCN-NOM-01 — Surface Ops HK Downlink (nominal)

Reference: [ConOps §4](../conops/ConOps.md).

| Test artefact | Location | What it asserts |
|---|---|---|
| TC-SCN-NOM-01-A | `simulation/scenarios/scn-nom-01.yaml` (planned) | Full path A ([07 §9](../../architecture/07-comms-stack.md)) is traversed; all SPP sequence counters monotonic with no gaps |
| TC-SCN-NOM-01-B | Ground-station integration test | End-to-end latency = configured light-time ± 500 ms per [ICD-orbiter-ground.md §6](../../interfaces/ICD-orbiter-ground.md) |
| TC-SCN-NOM-01-C | Rover + ROS 2 launch test | All rover lifecycle nodes remain in `ACTIVE`; no EVS events above INFO on cFS assets |
| TC-SCN-NOM-01-D | Time-tag integration | TM timestamps match the [time-authority ladder](../../architecture/08-timing-and-clocks.md) under nominal conditions (no `time_suspect` flag set) |

Pass = all four artefacts green; failure in any is a scenario regression.

### 3.2 SCN-OFF-01 — Cryobot Tether Fault (off-nominal)

Reference: [ConOps §5](../conops/ConOps.md).

| Test artefact | Location | What it asserts |
|---|---|---|
| TC-SCN-OFF-01-A | `simulation/scenarios/scn-off-01.yaml` (planned) | Tether BW collapse via APID `0x540` (packet-drop on `ROVER-CRYOBOT` link); cryobot comm node transitions to DEGRADED |
| TC-SCN-OFF-01-B | Rover lifecycle assertion | Drill controller transitions to `HOLD` within N Proximity-1 intervals (concrete N from `ICD-cryobot-tether.md`) |
| TC-SCN-OFF-01-C | Event-propagation timing | Event reaches ground within light-time + 2 × tether-retransmit window per [ConOps §5](../conops/ConOps.md) |
| TC-SCN-OFF-01-D | Fate-share watchdog | DEGRADED-latch is fate-shared with an independent watchdog — *explicitly listed as a failure mode to test* in [ConOps §5](../conops/ConOps.md) |
| TC-SCN-OFF-01-E | Relay store-and-forward | Priority events preserved when operator is LOS — per [ConOps §5](../conops/ConOps.md) failure-mode bullet |

### 3.3 Binding to requirements (Phase C)

When SRDs land in `mission/requirements/` (Phase C), each test artefact above gets a `traces-to` column listing `FSW-REQ-####`, `ROV-REQ-####`, `GND-REQ-####`, `SYS-REQ-####`. The [traceability linter (planned)](../../README.md) will enforce that every requirement has at least one test artefact and every test artefact traces to at least one requirement.

## 4. Fault-Injection Tests

One test class per APID in the minimum fault set from [Q-F2](../../standards/decisions-log.md). Every class lives in the system tier (§2.3) because it exercises the full `fault_injector` → `sim_adapter` → subscriber chain from [09 §2](../../architecture/09-failure-and-radiation.md). Fault-packet bodies are in [packet-catalog §7](../../interfaces/packet-catalog.md).

### 4.1 Per-fault test matrix

| Fault | Gate scenario | Unit-level coverage | Integration-level coverage |
|---|---|---|---|
| `0x540` packet drop | SCN-OFF-01 | `sim_adapter` CRC validation + `link_id` enum decode | Per-segment Bernoulli-drop consumers ([09 §4](../../architecture/09-failure-and-radiation.md) table) |
| `0x541` clock skew | Time-authority regression | `CFE_TIME` read-hook composition | `time_suspect` flag propagates to TM per [08 §4](../../architecture/08-timing-and-clocks.md) |
| `0x542` force safe-mode | SCN-OFF-01 RESUME path (Phase C) | Mode-manager dispatch | Per-segment safing-path ladders |
| `0x543` sensor-noise | Per-app HK residual | Noise-model decode (OFF/GAUSSIAN/UNIFORM/STUCK/BIT-FLIP) | HK residual distribution within expected band for each sensor |

### 4.2 Q-F3 EDAC read-hook invariant (regression)

Per [09 §5.3](../../architecture/09-failure-and-radiation.md) — clock-skew injection must never write to the `.critical_mem`-resident `tai_ns` store. A dedicated regression test asserts:

1. Before `0x541` injection: `g_tai_ns` checksum = X.
2. After injection: `g_tai_ns` checksum = X (unchanged); `cfe_time_now()` returns shifted value.
3. Post-revert: `cfe_time_now()` returns un-shifted value.

This is the strongest assertion that `.critical_mem` integrity survives the fault path. Failure here is a Sev-1 regression.

### 4.3 Reserved (Phase-B-plus)

SEU / bit-flip stimulus, EDAC scrub-event injection — reserved per [09 §6](../../architecture/09-failure-and-radiation.md). Test artefacts listed here for traceability:

| Reserved fault | Stimulus source | Gate |
|---|---|---|
| Bit-flip (new APID in `0x544`–`0x56F`) | `fault_injector` scenario keyword | Scrubber corrects within 1 scrub interval; no downstream impact |
| EDAC scrub event | `orbiter_scrubber` emission | HK packet counts monotonic |

Both activate only when Phase-B-plus lands the scrubber task + linker-script `.critical_mem` placement ([09 §9](../../architecture/09-failure-and-radiation.md)).

## 5. Coverage & Gate Criteria

All targets come from [CLAUDE.md](../../../CLAUDE.md); this section binds them to a release gate.

### 5.1 Code-coverage targets

| Stack | Tool | Target | Enforcement |
|---|---|---|---|
| cFS / C | `ctest` + `gcov` | **100 % branch coverage** per [CLAUDE.md §C / FSW](../../../CLAUDE.md) + [.claude/rules/testing.md](../../../.claude/rules/testing.md) | Unit gate |
| Rust | `cargo tarpaulin --out Html` | **100 % branch coverage** target per [.claude/rules/testing.md](../../../.claude/rules/testing.md); Linux x86_64 only | Unit gate |
| ROS 2 / C++ | `colcon test` | coverage target deferred to Phase C | Informational |
| Gazebo plugins | gtest via plugin unit tests | "loads without crash" minimum per [05 §7](../../architecture/05-simulation-gazebo.md) | Informational |

### 5.2 Static-analysis gates

Every PR passes:

| Tool | Scope | Gate condition |
|---|---|---|
| `cppcheck --enable=all --std=c17 apps/` | cFS FSW | **Zero new findings** per [.claude/rules/security.md](../../../.claude/rules/security.md) |
| `cargo clippy -- -D warnings` | All Rust crates | **Zero warnings**, no suppressions without justification comment |
| `cargo audit` | All Rust crates | **Zero HIGH or CRITICAL** advisories per [.claude/rules/security.md](../../../.claude/rules/security.md) |
| MISRA deviation scan | `apps/**` | Every deviation has an inline `/* MISRA C:2012 Rule X.Y deviation: ... */` comment per [.claude/rules/general.md](../../../.claude/rules/general.md) |

### 5.3 Scenario gates

Per §3, SCN-NOM-01 and SCN-OFF-01 are the Phase-B release gates. Both must pass with no regressions before:

- A tag is cut from `master`.
- The "V&V passed" checkbox on any release PR is checked.

Scenario additions are a release-gate change and require updating this plan.

### 5.4 Mandatory PR checks

Per [CLAUDE.md §PR & Review Process](../../../CLAUDE.md):

- `ctest` passes.
- `cargo test` passes.
- `cargo clippy -- -D warnings` passes.
- At least one approving review.

Currently enforced **manually**; CI automation is a §10 open item.

## 6. Test Infrastructure Inventory

### 6.1 What exists today

| Location | Artefact | Stack |
|---|---|---|
| [`apps/sample_app/fsw/unit-test/sample_app_test.c`](../../../apps/sample_app/fsw/unit-test/sample_app_test.c) | CMocka reference | cFS / C |
| [`ros2_ws/src/rover_teleop/test/test_teleop.cpp`](../../../ros2_ws/src/rover_teleop/test/test_teleop.cpp) | gtest reference | ROS 2 / C++ |
| [`rust/ground_station/`](../../../rust/ground_station/) | Cargo workspace member | Rust |
| [`rust/cfs_bindings/`](../../../rust/cfs_bindings/) | Cargo workspace member | Rust |
| [`simulation/gazebo_rover_plugin/`](../../../simulation/gazebo_rover_plugin/) | Reference Gazebo plugin | Gazebo / C++ |

### 6.2 What is missing (tracked)

- `rust/ground_station/tests/` — no integration tests yet.
- `rust/cfs_bindings/tests/` — no integration tests yet.
- `simulation/scenarios/` — no scenario YAML files yet ([05 §6](../../architecture/05-simulation-gazebo.md) directory planned).
- `apps/sim_adapter/` — `sim_adapter` app itself is planned per [ICD-sim-fsw.md §4](../../interfaces/ICD-sim-fsw.md).
- `apps/orbiter_*`, `apps/mcu_*`, `apps/freertos_relay/` — planned per [01 §3](../../architecture/01-orbiter-cfs.md), [02](../../architecture/02-smallsat-relay.md), [03 §6](../../architecture/03-subsystem-mcus.md).

### 6.3 How the plan uses what exists

Until the missing artefacts land, this plan's gates are **designed-for**, not **measured**. The scenario and integration tables above describe the target state; the unit-level gates (§5.1) run today against `sample_app`, `rover_teleop`, and the two Rust crates.

## 7. CI & Gating

### 7.1 Current state (open)

No `.github/workflows/` directory exists today. The PR gate in [CLAUDE.md §PR & Review Process](../../../CLAUDE.md) is **enforced manually** by reviewers. This is acceptable for the SITL demonstrator; it is not acceptable for a PDR-surrogate posture.

### 7.2 Target state (Phase C)

Per [`../../README.md`](../../README.md) Verification / CI section:

| Check | Scope | Tool |
|---|---|---|
| Link check | `docs/**/*.md` | GitHub Action invoking a link-check script |
| Mermaid / PlantUML parse | Every fenced diagram | `@mermaid-js/mermaid-cli` + PlantUML |
| Requirement-ID linter | Every `XXX-REQ-####` resolves | `scripts/traceability-lint.py` (planned, Phase C) |
| APID / MID consistency | Every MID macro has an APID row | [`scripts/check_sim_apids.py`](../../interfaces/ICD-sim-fsw.md) (planned) |
| Citation integrity | Every standards ID exists in references.md | planned |
| Unit test gates | `ctest` + `colcon test` + `cargo test` + `cargo clippy` | GitHub Actions matrix |
| Static analysis | `cppcheck`, `cargo audit` | GitHub Actions |

### 7.3 Why manual today

PR-gate automation is deferred until Phase C so it lands with requirement-ID traceability and the compliance matrix — these are the same GitHub Actions matrix. Staging them separately would double the CI implementation cost.

## 8. Scenario Authoring Discipline

Every scenario YAML under `simulation/scenarios/*.yaml` must:

1. **Name the ConOps scenario it validates** (`ConOps §4` or `§5`, or "new scenario" with a ConOps-level PR first).
2. **Enumerate fault injections** only via [APID `0x540`–`0x543`](../../interfaces/apid-registry.md) (Phase B) — no ad-hoc fault keywords.
3. **Declare expected observable outcomes** — what HK changes, what events fire, what mode transitions occur.
4. **Declare a bounded duration** — open-ended scenarios are not V&V artefacts.
5. **Trace to requirements** — Phase C mandates `traces-to:` field; Phase B scenarios may omit it with an inline TODO.

Schema for the YAML lives with `fault_injector` when it lands ([05 §11](../../architecture/05-simulation-gazebo.md) open item).

## 9. Standards Compliance Map

Binding, not duplication. Each row cites a standard row in [references.md](../../standards/references.md) and names the section here that carries the obligation.

| Standard | Obligation | This plan's binding |
|---|---|---|
| [NASA-STD-8739.8B](../../standards/references.md) | Software assurance & safety process | §2 class taxonomy, §5 gates, §7 CI target-state |
| [NASA-STD-7009A](../../standards/references.md) | Model-and-simulation assurance | §4 fault-injection, §3 scenarios, §8 authoring discipline |
| [NPR 7150.2D](../../standards/references.md) | NASA software engineering | §2 test rigor tiers, §9 compliance map itself (Phase-C populated) |
| [ECSS-E-ST-10-02C](../../standards/references.md) | Verification taxonomy | §2 three-class split |
| [ECSS-Q-ST-80C Rev.1](../../standards/references.md) | Software product assurance | §5.2 static-analysis gates, §9 row itself (Phase C) |

A Phase-C compliance matrix (`mission/verification/compliance-matrix.md`, planned per [`../../README.md`](../../README.md)) expands each row here into line-level objectives with pass/fail evidence.

## 10. Open Items

Tracked, not blocking for Phase B close.

- **CI workflows (`.github/workflows/`)** — §7 target state; lands in Phase C with the traceability linter.
- **Coverage baseline via `cargo tarpaulin`** — first run lands with the `rust/ground_station/` + `rust/cfs_bindings/` test suites.
- **Independent V&V role (IEEE 1012)** — single-contributor posture in Phase B; tracked as a program-surrogate role that lands when the project grows.
- **E2E scaling tests** — Scale-5 profile test run per [10 §2](../../architecture/10-scaling-and-config.md); gated on the Scale-5 compose overlay landing ([`../../dev/docker-runbook.md`](../../dev/docker-runbook.md)).
- **Requirement traceability** — `mission/requirements/traceability.md` + linter script. Phase C.
- **Compliance matrix** — expansion of §9 into a line-level per-standard matrix. Phase C.
- **`simulation/scenarios/`** — YAML files for SCN-NOM-01, SCN-OFF-01, and the Q-F3 read-hook regression. Land with `fault_injector` implementation ([05 §11](../../architecture/05-simulation-gazebo.md)).
- **Static analysis on ROS 2 C++** — clang-tidy integration is Phase C; manual review until then.
- **`deviations.md` population** — any coverage or lint target that is *knowingly* missed gets a row in [`deviations.md`](../../standards/deviations.md) with justification.

## 11. What this plan is NOT

- Not a test-case catalogue. Test bodies live next to source, not here.
- Not a requirements document — that tier lives under [`../requirements/`](../requirements/) in Phase C.
- Not a compliance matrix — that tier lives under [`compliance-matrix.md`](compliance-matrix.md) (planned, Phase C).
- Not a CM plan — release cadence, version pinning, and build reproducibility live in [`../configuration/CM-Plan.md`](../configuration/CM-Plan.md) (planned, Phase C).
- Not a certification artifact. SAKURA-II aligns with NASA / ECSS standards ([references.md §3](../../standards/references.md)); it does not claim conformance.
- Not a coding rulebook. Test-authoring rules are in [.claude/rules/testing.md](../../../.claude/rules/testing.md); this plan cites them.
