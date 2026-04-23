# SAKURA-II — Implementation Guide (Constitution)

> **Status:** v1.0 — authoritative execution plan for moving SAKURA-II from paper architecture to flight-grade code.
> **Scope:** 50 sequential phases, bottom-up. Phases 10 / 20 / 30 / 40 / 50 are hard deliverables (runnable binary or shippable library).
> **Non-scope:** coding-rule content, Q-\* rationale, requirement text — this doc **cites only**. Sources: [`docs/REPO_MAP.md`](docs/REPO_MAP.md), [`docs/architecture/`](docs/architecture/), [`docs/mission/requirements/`](docs/mission/requirements/), [`docs/standards/decisions-log.md`](docs/standards/decisions-log.md), [`docs/interfaces/`](docs/interfaces/), [`.claude/rules/`](.claude/rules/), [`CLAUDE.md`](CLAUDE.md).

---

## 1. Preamble & Scope

### 1.1 What this doc IS
The single, authoritative execution plan for SAKURA-II Phase C onward. Every code PR on the path to the Scale-5 demonstrator maps to exactly one phase in §7. A phase has a branch, a DoD, a coverage gate, a linter gate, and a prescribed AI prompting strategy. No work lands on `main` outside a phase.

### 1.2 What this doc is NOT
- Not a restatement of coding rules — see [`.claude/rules/general.md`](.claude/rules/general.md), [`.claude/rules/cfs-apps.md`](.claude/rules/cfs-apps.md), [`.claude/rules/ros2-nodes.md`](.claude/rules/ros2-nodes.md), [`.claude/rules/security.md`](.claude/rules/security.md), [`.claude/rules/testing.md`](.claude/rules/testing.md).
- Not a requirements doc — see [`docs/mission/requirements/SRD.md`](docs/mission/requirements/SRD.md) and sub-SRDs.
- Not an ICD — see [`docs/interfaces/`](docs/interfaces/).
- Not a decisions log — see [`docs/standards/decisions-log.md`](docs/standards/decisions-log.md).
- Not an architecture doc — see [`docs/architecture/`](docs/architecture/). The definition site for the Rust ground segment is [`docs/architecture/06-ground-segment-rust.md`](docs/architecture/06-ground-segment-rust.md); the §12 acceptance gate there is authoritative for Phase 30.

### 1.3 Constitution clauses (normative)
1. **No merge to `main`** unless the owning phase's DoD (§3.3) passes.
2. **No phase skipping**. Phase N+1 MUST branch from a `main` that already carries phase N.
3. **No restating**. A phase card cites source docs; it never copies their text.
4. **Every phase traces**. Every phase card names ≥ 1 `*-REQ-####` in the relevant SRD and ≥ 1 Q-\* where applicable. [`scripts/traceability-lint.py`](scripts/traceability-lint.py) remains clean after the phase lands.
5. **Bottom-up invariant**. Upstream crates (`ccsds_wire`, `cfs_bindings`) never depend on downstream (`ground_station`, `apps/**`, `ros2_ws/**`). Violations block merge.

---

## 2. Operational Guidelines — Space-Grade

### 2.1 TDD Red-Green-Refactor (absolute)
Every phase begins with a failing test. No production code is written before the failing test exists on the branch. The sequence is:
1. **Red** — commit a test that demonstrates the feature is missing or the bug exists.
2. **Green** — commit the minimal code that turns the test green.
3. **Refactor** — commit cleanups that do not change externally observable behavior.
Each step is a separate commit on the phase branch. Commit messages follow [`CLAUDE.md §Conventions`](CLAUDE.md).

### 2.2 Given / When / Then (auditability)
Every test (unit, integration, property, simulation) is written in the G/W/T form so a reviewer can reconstruct intent without reading the implementation. Skeleton:

```rust
#[test]
fn test_primary_header_rejects_invalid_version() {
    // Given a buffer whose version field is non-zero
    let mut buf = [0u8; 6];
    buf[0] = 0b0010_0000;
    // When we decode it
    let result = PrimaryHeader::decode(&buf);
    // Then we get InvalidVersion
    assert_eq!(result, Err(CcsdsError::InvalidVersion(0b001)));
}
```

```c
/* Given a fresh command counter, When a NOOP TC arrives,
 * Then CmdCounter increments and ErrCounter does not. */
static void test_cmd_noop_increments_cmd_counter(void **state);
```

### 2.3 100 % coverage (line and branch)
| Domain | Tool | Threshold | Deviation path |
|---|---|---|---|
| Rust crates | `cargo tarpaulin --engine llvm --out Xml` | 100 % line + branch per new file; ≥ 85 % per milestone crate roll-up (per [`06 §12`](docs/architecture/06-ground-segment-rust.md)) | [`docs/standards/deviations.md`](docs/standards/deviations.md) with Q-ID or issue link |
| C / cFS apps | `gcov` + `llvm-cov --show-branches` on CMocka suite | 100 % branch per `apps/**/fsw/src/` file | same |
| C++ / ROS 2 | `ament_cmake_gtest` with `-fprofile-arcs -ftest-coverage` | 100 % line + branch per new file | same |

Deviations are rare, tracked, and require reviewer approval — not bulk-waived.

### 2.4 Invariants (workspace-wide)
- **Big-Endian on the wire** — Q-C8. Only `rust/ccsds_wire/` and `rust/cfs_bindings/` may call `*_be_bytes` / `*_le_bytes`; elsewhere is a CI grep failure.
- **Zero panic** — `panic = "deny"`, `unwrap_used = "deny"`, `expect_used = "deny"` at workspace root ([`06 §1.2`](docs/architecture/06-ground-segment-rust.md)). `ccsds_wire` is `#![forbid(unsafe_code)]`.
- **Deterministic memory** — no `malloc`/`calloc`/`realloc`/`free` under `apps/**`; no VLAs (MISRA 18.8); static sizing from [`_defs/mission_config.h`](_defs/mission_config.h) ([.claude/rules/cfs-apps.md](.claude/rules/cfs-apps.md), SYS-REQ-0072).
- **`no_std`-ready for embedded crates** — `ccsds_wire` uses only `core` + `alloc` paths that a future `#![no_std]` switch can consume without API change; server-side crates stay `std`.
- **Radiation anchors** — `.critical_mem` on the C side and `Vault<T>` on the Rust side (Q-F3, SYS-REQ-0042); every radiation-sensitive static carries both the attribute and a MISRA Rule 8.11 deviation comment.

### 2.5 Workspace lints (locked at Phase 01)
Applied once in `Cargo.toml`; any phase that loosens them is rejected in review.

```toml
[workspace.lints.rust]
unsafe_code = "deny"

[workspace.lints.clippy]
all = "deny"
pedantic = "warn"
unwrap_used = "deny"
expect_used = "deny"
panic = "deny"
indexing_slicing = "warn"
cast_lossless = "warn"
missing_errors_doc = "warn"
missing_panics_doc = "warn"
```

`.cargo/config.toml` pins:
```toml
[build]
rustflags = ["-D", "warnings"]
```
`rust-toolchain.toml` pins `channel = "stable"` + `rustfmt` + `clippy`. No nightly features in any workspace member ([`06 §1.3`](docs/architecture/06-ground-segment-rust.md), Ferrocene seam).

---

## 3. Phase-Based Git Workflow

### 3.1 Branch naming
`feat/phase-XX-<short-kebab-title>` where `XX` is two-digit (`01`, `02`, …, `50`) and `<short-kebab-title>` ≤ 4 words.
Examples: `feat/phase-03-apid-newtype`, `feat/phase-25-apid-router`, `feat/phase-40-sitl-integration`.
No `fix/*` or `chore/*` branches on the 50-phase path; defects found mid-phase are rolled into the same branch.

### 3.2 Commit format
Per [`CLAUDE.md`](CLAUDE.md): `type(scope): description` where `type ∈ {feat, fix, chore, docs, test, refactor}` and `scope` matches the crate/app name (`ccsds_wire`, `cfs_bindings`, `ground_station`, `orbiter_cdh`, …).

### 3.3 Definition of Done (applies to every phase)
A phase branch is merge-ready **only when all of the following pass in CI**:
- [ ] `cargo build --workspace --all-targets` exits 0 (if Rust touched).
- [ ] `cargo clippy --workspace --all-targets -- -D warnings` exits 0 (if Rust touched).
- [ ] `cargo fmt --all -- --check` exits 0 (if Rust touched).
- [ ] `cargo test --workspace` exits 0 (if Rust touched).
- [ ] `cargo audit` reports zero HIGH / CRITICAL advisories.
- [ ] `cmake --build build && ctest --test-dir build --output-on-failure` exits 0 (if C touched).
- [ ] `cppcheck --enable=all --std=c17 apps/` reports zero new findings (if C touched).
- [ ] `colcon build --symlink-install && colcon test` exits 0 (if ROS 2 touched).
- [ ] `python3 scripts/traceability-lint.py` exits 0.
- [ ] Coverage thresholds per §2.3 met on changed files.
- [ ] All grep guards from [`06 §12`](docs/architecture/06-ground-segment-rust.md) return 0 matches (once Phase 30 has landed).
- [ ] Phase-specific DoD checks from the phase card in §7 all pass.
- [ ] `@code-reviewer` agent run; Must-Fix list is empty.
- [ ] For CRIT / TEST / crypto / comms / buffer phases: `/security-review` run; CRITICAL + HIGH findings empty.

### 3.4 PR template & merge gate
- Opens via [`.claude/skills/create-pr`](.claude/skills/) (`/create-pr`).
- Merge to `main` requires ≥ 1 approving human review after all automated gates pass.
- Hotfixes to already-merged phases land in the next unmerged phase or a tracked follow-up phase 51+ — the 50 phases are immutable once merged.

### 3.5 Milestone gates (phases 10 / 20 / 30 / 40 / 50)
A milestone phase is `[ TEST ]`-badged (§4.2) and produces a **hard deliverable**: a runnable binary or a shippable library whose release is a git tag. Milestone phases cannot be partially merged — all deliverables in §6 land together.

---

## 4. Master Implementation Matrix

### 4.1 Badge legend
| Badge | Category | Typical model |
|---|---|---|
| 🔵 `[ARCH]` | Architecture / scaffolding / module-tree / workspace setup | `claude-opus-4-7` |
| 🔴 `[CRIT]` | Critical invariant, security boundary, safety interlock, radiation anchor | `claude-opus-4-7` |
| 🟢 `[FEAT]` | Feature implementation (encoder, router, app, node, plugin) | `claude-sonnet-4-6` |
| 🟡 `[LCC]` | Lifecycle / CI / hardening / dependency maintenance | `claude-sonnet-4-6` |
| 🧪 `[TEST]` | Milestone gate — hard deliverable | `claude-sonnet-4-6` (with Opus review) |

### 4.2 The 50-phase table

| ID | Badge | Title | Branch | Primary Model | Risk | Coverage |
|---|---|---|---|---|---|---|
| `[✅]` 01 | 🔵 ARCH | Workspace bootstrap (lints, toolchain, cargo config) | `feat/phase-01-workspace-bootstrap` | `claude-opus-4-7` | M | 100 % ✅ |
| `[✅]` 02 | 🔵 ARCH | `ccsds_wire` crate skeleton | `feat/phase-02-ccsds-wire-skeleton` | `claude-opus-4-7` | L | 100 % ✅ |
| `[✅]` 03 | 🔴 CRIT | `Apid` newtype + IDLE sentinel | `feat/phase-03-apid-newtype` | `claude-opus-4-7` | M | 100 % ✅ |
| `[✅]` 04 | 🔴 CRIT | Sequence / length / funccode / instance newtypes | `feat/phase-04-sized-newtypes` | `claude-opus-4-7` | M | 100 % ✅ |
| `[✅]` 05 | 🔴 CRIT | `PacketType` + `CcsdsError` enums | `feat/phase-05-error-enum` | `claude-opus-4-7` | L | 100 % ✅ |
| `[ ]` 06 | 🔴 CRIT | `Cuc` struct + BE codec + P-Field 0x2F | `feat/phase-06-cuc-codec` | `claude-opus-4-7` | H | 100 % ✅ |
| `[ ]` 07 | 🔴 CRIT | `PrimaryHeader` encode / decode | `feat/phase-07-primary-header` | `claude-opus-4-7` | H | 100 % ✅ |
| `[ ]` 08 | 🔴 CRIT | `SecondaryHeader` + `time_suspect` | `feat/phase-08-secondary-header` | `claude-opus-4-7` | H | 100 % ✅ |
| `[ ]` 09 | 🟢 FEAT | `SpacePacket<'a>` + `PacketBuilder` | `feat/phase-09-space-packet` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 10 | 🧪 TEST | **MILESTONE 1/5 — `ccsds_wire v1.0`** | `feat/phase-10-ccsds-wire-milestone` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 11 | 🔵 ARCH | Rename SAMPLE_MISSION → SAKURA_II + SCID hook | `feat/phase-11-mission-rename` | `claude-opus-4-7` | L | 100 % ✅ |
| `[ ]` 12 | 🟢 FEAT | `cfs_bindings` safe wrappers (CFE_MSG / CFE_SB) | `feat/phase-12-cfs-msg-wrappers` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 13 | 🟢 FEAT | `_defs/mids.h` + `cfs_bindings::mids` | `feat/phase-13-mid-macros` | `claude-sonnet-4-6` | L | 100 % ✅ |
| `[ ]` 14 | 🟢 FEAT | FFI host ↔ `ccsds_wire` conversion helpers | `feat/phase-14-ffi-conversion` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 15 | 🔴 CRIT | Migrate `ground_station::telemetry` → `ccsds_wire` | `feat/phase-15-telemetry-migration` | `claude-opus-4-7` | M | 100 % ✅ |
| `[ ]` 16 | 🧪 TEST | cfs_bindings round-trip test suite | `feat/phase-16-bindings-roundtrip-tests` | `claude-sonnet-4-6` | L | 100 % ✅ |
| `[ ]` 17 | 🔵 ARCH | cFS app template CMake macro | `feat/phase-17-cfs-app-template` | `claude-opus-4-7` | M | 100 % ✅ |
| `[ ]` 18 | 🟡 LCC | MISRA C:2012 cppcheck baseline + CI hook | `feat/phase-18-misra-baseline` | `claude-sonnet-4-6` | L | n/a (gate) |
| `[ ]` 19 | 🟡 LCC | CMocka template + branch-coverage gate | `feat/phase-19-cmocka-template` | `claude-sonnet-4-6` | L | 100 % ✅ |
| `[ ]` 20 | 🧪 TEST | **MILESTONE 2/5 — `cfs_bindings v1.0` + cFS template** | `feat/phase-20-bindings-milestone` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 21 | 🔵 ARCH | `ground_station::{ingest,uplink,cfdp,mfile,ui}` scaffold | `feat/phase-21-ground-station-scaffold` | `claude-opus-4-7` | M | 100 % ✅ |
| `[ ]` 22 | 🟢 FEAT | `AosFramer` (1024 B, FECF CRC-16, OCF) | `feat/phase-22-aos-framer` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 23 | 🟢 FEAT | `VcDemultiplexer` + bounded mpsc | `feat/phase-23-vc-demux` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 24 | 🟢 FEAT | `SppDecoder` with backpressure events | `feat/phase-24-spp-decoder` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 25 | 🔴 CRIT | `ApidRouter` + fault-APID rejection | `feat/phase-25-apid-router` | `claude-opus-4-7` | H | 100 % ✅ |
| `[ ]` 26 | 🟢 FEAT | `CfdpProvider` trait + `Class1Receiver` | `feat/phase-26-cfdp-class1` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 27 | 🟢 FEAT | `MFileAssembler` out-of-order reassembly | `feat/phase-27-mfile-assembler` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 28 | 🟢 FEAT | TC uplink pipeline (TcBuilder + Cop1 + TcFramer) | `feat/phase-28-tc-uplink` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 29 | 🟢 FEAT | Operator-UI backend surfaces (WS + REST) | `feat/phase-29-ui-backend` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 30 | 🧪 TEST | **MILESTONE 3/5 — `ground_station v0.1` (Phase C Step 2 gate)** | `feat/phase-30-ground-station-milestone` | `claude-sonnet-4-6` | H | ≥ 85 % / ≥ 70 % |
| `[ ]` 31 | 🟢 FEAT | `apps/orbiter_cdh` | `feat/phase-31-orbiter-cdh` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 32 | 🟢 FEAT | `apps/orbiter_adcs` | `feat/phase-32-orbiter-adcs` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 33 | 🟢 FEAT | `apps/orbiter_comm` | `feat/phase-33-orbiter-comm` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 34 | 🟢 FEAT | `apps/orbiter_power` + `apps/orbiter_payload` | `feat/phase-34-orbiter-power-payload` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 35 | 🟢 FEAT | MCU gateway apps (SpW / CAN / UART) | `feat/phase-35-mcu-gateways` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 36 | 🟢 FEAT | `ros2_ws/src/rover_common` | `feat/phase-36-rover-common` | `claude-sonnet-4-6` | L | 100 % ✅ |
| `[ ]` 37 | 🟢 FEAT | `rover_land` + `rover_uav` + `rover_cryobot` | `feat/phase-37-rover-classes` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 38 | 🟢 FEAT | Gazebo UAV / cryobot / world plugins | `feat/phase-38-gazebo-plugins` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 39 | 🟢 FEAT | `simulation/fault_injector` (0x540–0x543 sideband) | `feat/phase-39-fault-injector` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 40 | 🧪 TEST | **MILESTONE 4/5 — SITL Integrated Binary** | `feat/phase-40-sitl-integration` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 41 | 🟢 FEAT | `apps/freertos_relay` (smallsat primary FSW) | `feat/phase-41-freertos-relay` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 42 | 🟢 FEAT | FreeRTOS MCU firmware (`mcu_payload`/`mcu_rwa`/`mcu_eps`) | `feat/phase-42-freertos-mcu-fw` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 43 | 🟢 FEAT | Time-authority ladder + `time_suspect` propagation | `feat/phase-43-time-ladder` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 44 | 🔴 CRIT | `Vault<T>` + `.critical_mem` radiation anchors | `feat/phase-44-radiation-anchors` | `claude-opus-4-7` | H | 100 % ✅ |
| `[ ]` 45 | 🟢 FEAT | Scale-5 config surfaces (mission.yaml, compose profiles) | `feat/phase-45-scale-5-config` | `claude-sonnet-4-6` | M | 100 % ✅ |
| `[ ]` 46 | 🟡 LCC | Safe-mode state machine + operator RESUME | `feat/phase-46-safe-mode` | `claude-sonnet-4-6` | H | 100 % ✅ |
| `[ ]` 47 | 🟡 LCC | Security hardening gates (audit + cppcheck + secrets) | `feat/phase-47-security-gates` | `claude-sonnet-4-6` | M | n/a (gate) |
| `[ ]` 48 | 🟡 LCC | V&V catalog + traceability closure | `feat/phase-48-vv-catalog` | `claude-sonnet-4-6` | M | n/a (gate) |
| `[ ]` 49 | 🟡 LCC | GitHub Actions CI pipeline | `feat/phase-49-ci-pipeline` | `claude-sonnet-4-6` | M | n/a (gate) |
| `[ ]` 50 | 🧪 TEST | **MILESTONE 5/5 — Mission Demonstrator v1.0** | `feat/phase-50-mission-demo` | `claude-sonnet-4-6` | H | 100 % ✅ |

---

## 5. Strategic Alignment — Bottom-Up Path

```
Block 1  ccsds_wire               ┐
(P01-10) pure Rust BE locus       │  Q-C8 locus A
         (no deps, no unsafe)     ┘
              ↓
Block 2  cfs_bindings + mids      ┐
(P11-20) + cFS template           │  Q-C8 locus B  +  cFS convention bedrock
         + MISRA baseline         ┘
              ↓
Block 3  ground_station pipelines ┐
(P21-30) AOS·VC·SPP·APID·CFDP     │  Phase C Step 2 gate (06 §12)
         COP-1·MFile·UI backend   ┘
              ↓
Block 4  apps/orbiter_*           ┐
(P31-40) apps/mcu_*_gw            │  First SITL integration
         rover_{land,uav,cryobot} │
         gazebo plugins + faults  ┘
              ↓
Block 5  freertos_relay + MCUs    ┐
(P41-50) time ladder + Vault<T>   │  Scale-5 mission demonstrator
         scale-5 + safe-mode + CI ┘
```

**Invariant:** upstream blocks never import downstream. `ccsds_wire` has zero internal deps. `cfs_bindings` depends only on `ccsds_wire`. `ground_station` depends on both. Flight apps (`apps/**`) depend on cFS / OSAL headers plus `_defs/*`. ROS 2 packages depend on rclcpp + per-class msg types. Gazebo plugins depend on Gazebo + sideband SPP encoder (which is built into the fault injector, not flight code).

This ordering is load-bearing for Q-C8: by landing `ccsds_wire` (Block 1) before any byte-producing consumer (Blocks 2–5), the "grep audit of `*_le_bytes` / `*_be_bytes` returns 0 outside locus A+B" guard in §2.9 of [`06-ground-segment-rust.md`](docs/architecture/06-ground-segment-rust.md) is enforceable from Phase 10 onward.

---

## 6. Hard Deliverables (milestone binaries / libraries)

| Milestone | Phase | Deliverable | Verified by |
|---|---|---|---|
| **1 / 5** | 10 | **`ccsds_wire v1.0`** — shippable Rust library crate. Primary + secondary header, CUC codec, SpacePacket view, PacketBuilder. `#![forbid(unsafe_code)]`. | 7 proptests + 5 KATs + grep guard + `cargo test -p ccsds_wire` + `tarpaulin -p ccsds_wire` ≥ 100 % line/branch. |
| **2 / 5** | 20 | **`cfs_bindings v1.0` + cFS App Template** — safe FFI wrappers + CMake macro that spawns a new `apps/<name>/` at parity with `apps/sample_app/`. | `cargo test -p cfs_bindings` + `cppcheck --enable=all apps/` + `ctest` + sample_app rebuilds under the new template. |
| **3 / 5** | 30 | **`ground_station v0.1` binary** — all of: AOS framer, VC demux, SPP decoder, APID router with fault-APID rejection, Class 1 CFDP receiver, M-File assembler, COP-1 uplink, operator UI backend. | [`06 §12`](docs/architecture/06-ground-segment-rust.md) acceptance gate (all 13 checkboxes). **Phase C Step 2 unblocked.** |
| **4 / 5** | 40 | **SITL Integrated Binary** — `docker compose up` brings up orbiter cFS cluster + ROS 2 rovers + Gazebo + ground_station on a private network. | `SCN-NOM-01` end-to-end: ground UI renders live HK from every asset class; `0x541` clock-skew injection toggles `time_suspect` flag; `colcon test && ctest && cargo test --workspace` green. |
| **5 / 5** | 50 | **Mission Demonstrator v1.0** — MVC → Scale-5 SITL (5 orbiters + 1 relay + 5 land + 3 UAV + 2 cryobot) with full V&V closure. | Every `SYS-REQ-####` in `SRD.md` has a passing V&V artefact in `traceability.md`; `cargo audit` clean; `cppcheck` clean; `/security-review` pass on comms/crypto/buffer; tagged release commit. |

---

## 7. Phase Detailed Breakdown

> Each card is self-contained: an engineer (or LLM) can execute the phase using only the card, the cited source docs, and the repo state at merge of the prior phase. Branch names, DoD verification commands, and AI prompting strategies are all explicit.

### Phase 01 — Workspace bootstrap 🔵 [ARCH]
**Branch:** `feat/phase-01-workspace-bootstrap` · **Model:** `claude-opus-4-7` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Lock the global Rust toolchain, rustflags, workspace lints, and shared deps so every subsequent phase inherits them automatically. Without this, Q-C8 and the zero-panic invariant cannot be CI-enforced.
**Source of Truth.** [`06 §1.2`](docs/architecture/06-ground-segment-rust.md) (workspace Cargo.toml delta), [`06 §1.3`](docs/architecture/06-ground-segment-rust.md) (toolchain pin), [`06 §1.4`](docs/architecture/06-ground-segment-rust.md) (`.cargo/config.toml`); `SYS-REQ-0071`; `SYS-REQ-0073`; Q-C8.
**Dependency Path.** (none — foundational).
**Deliverables.**
- `rust-toolchain.toml` at repo root: `channel = "stable"`, components `["rustfmt", "clippy"]`.
- `.cargo/config.toml` at repo root: `[build] rustflags = ["-D", "warnings"]`.
- `Cargo.toml` deltas: `[workspace.dependencies]` adds `cfdp-core` (TBD pin), `tokio = { version = "1", features = ["sync","rt-multi-thread","macros","time"] }`, `bytes = "1"`, `thiserror = "1"`, `crc = "3"`, `proptest = "1"`, `serde = { version = "1", features = ["derive"] }`.
- `Cargo.toml` `[workspace.lints.rust]` and `[workspace.lints.clippy]` blocks per §2.5.
- `[workspace.members]` adds placeholder `"rust/ccsds_wire"` (crate arrives Phase 02).

**Verification (DoD).** G/W/T:
- Given the new lints, When a test crate introduces `unsafe {}`, Then `cargo build -p test-crate` fails.
- Given `rustflags = "-D warnings"`, When a dead import exists, Then `cargo build` fails.
- `cargo fmt --all -- --check` exits 0; `cargo build --workspace` exits 0 (with `ccsds_wire` stub crate-root placeholder); `cargo clippy --workspace -- -D warnings` exits 0.

**AI Prompting Strategy.**
- Feed: [`06-ground-segment-rust.md §1`](docs/architecture/06-ground-segment-rust.md), [`.claude/rules/general.md`](.claude/rules/general.md).
- Agents: `@architect` to validate lint choices before commit; `@code-reviewer` post-green.
- Skills: `/code-review` pre-PR; `/create-pr` to open.
- Output style: `educational` (many new knobs; explain rationale in commit body).

---

### Phase 02 — `ccsds_wire` crate skeleton 🔵 [ARCH]
**Branch:** `feat/phase-02-ccsds-wire-skeleton` · **Model:** `claude-opus-4-7` · **Risk:** L · **Coverage:** 100 % ✅

**Objective.** Stand up `rust/ccsds_wire/` as an empty-but-buildable crate with the normative crate-root attributes so downstream phases add modules without re-litigating policy.
**Source of Truth.** [`06 §1.1`](docs/architecture/06-ground-segment-rust.md), [`06 §2.1`](docs/architecture/06-ground-segment-rust.md), [`REPO_MAP.md §rust/`](docs/REPO_MAP.md); Q-C8.
**Dependency Path.** Phase 01.
**Deliverables.**
- `rust/ccsds_wire/Cargo.toml`: `edition = "2021"`, `[dependencies] thiserror = { workspace = true }`.
- `rust/ccsds_wire/src/lib.rs`: `#![forbid(unsafe_code)]`, `#![deny(clippy::all)]`, empty module tree.
- `rust/ccsds_wire/tests/proptests.rs` stub (empty proptest module; populated in Phases 03–09).
- README line in crate header documenting "Q-C8 locus A — only BE codec in workspace".

**Verification (DoD).**
- `cargo build -p ccsds_wire` exits 0.
- `cargo clippy -p ccsds_wire -- -D warnings` exits 0.
- `rg 'unsafe ' rust/ccsds_wire/` → 0 matches (enforced by `#![forbid(unsafe_code)]`).

**AI Prompting Strategy.**
- Feed: [`06 §1.1`, §2.1`](docs/architecture/06-ground-segment-rust.md).
- Agents: `@architect` one-shot review to confirm crate attributes and dep list.
- Skills: `/create-pr`.
- Output style: `concise`.

---

### Phase 03 — `Apid` newtype + IDLE sentinel 🔴 [CRIT]
**Branch:** `feat/phase-03-apid-newtype` · **Model:** `claude-opus-4-7` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Lock the 11-bit APID invariant at the type boundary. Any later consumer that holds an `Apid` is guaranteed in-range; no raw `u16` leaks. Sets the pattern for every other newtype in the crate.
**Source of Truth.** [`06 §2.2`](docs/architecture/06-ground-segment-rust.md); [`docs/interfaces/apid-registry.md`](docs/interfaces/apid-registry.md); `SYS-REQ-0026`.
**Dependency Path.** Phase 02.
**Deliverables.**
- `rust/ccsds_wire/src/apid.rs`: sealed `struct Apid(u16)`, associated `const IDLE: Apid = Apid(0x7FF)`, `Apid::new(v: u16) -> Result<Self, CcsdsError>` (rejects `> 0x7FF`), `fn get(&self) -> u16`.
- Inline unit tests (G/W/T): valid range, boundary (0x000, 0x7FF), out-of-range (0x800), IDLE constant equality.

**Verification (DoD).**
- Given `Apid::new(0x800)`, When called, Then returns `Err(CcsdsError::ApidOutOfRange(0x800))`. (CcsdsError placeholder OK until Phase 05; land a temporary `#[derive(Debug)]` local error here and fold into CcsdsError in Phase 05.)
- Given `Apid::new(0x7FF)`, When called, Then equals `Apid::IDLE`.
- `cargo tarpaulin -p ccsds_wire` shows 100 % branch coverage on `apid.rs`.

**AI Prompting Strategy.**
- Feed: [`06 §2.2`, §2.8`](docs/architecture/06-ground-segment-rust.md), [`apid-registry.md`](docs/interfaces/apid-registry.md).
- Agents: `@architect` on the newtype pattern (sealing strategy); `@code-reviewer` post-green.
- Skills: `/code-review` before `/create-pr`.
- Output style: `educational` (first newtype — pattern documented here is copied by Phase 04).

---

### Phase 04 — Sized newtypes: `SequenceCount` / `PacketDataLength` / `FuncCode` / `InstanceId` 🔴 [CRIT]
**Branch:** `feat/phase-04-sized-newtypes` · **Model:** `claude-opus-4-7` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Extend the newtype discipline to every size-constrained primitive the headers need, so later modules cannot smuggle out-of-range values past type checks.
**Source of Truth.** [`06 §2.2`](docs/architecture/06-ground-segment-rust.md).
**Dependency Path.** Phase 03.
**Deliverables.**
- `rust/ccsds_wire/src/primitives.rs`: `SequenceCount(u16)` (14-bit), `PacketDataLength(u16)` (raw, validated on decode against buffer length), `FuncCode(u16)` (0x0000 reserved and rejected), `InstanceId(u8)` (1..=255; 0 reserved broadcast).
- Each type has fallible constructor returning `Result<Self, CcsdsError>` and `.get()` accessor; no `From<u16>`/`From<u8>` traits (grep audit).
- Inline G/W/T unit tests per type: valid, lower boundary, upper boundary, reserved-value rejection.

**Verification (DoD).**
- `cargo test -p ccsds_wire primitives::` covers every boundary and reserved-value case.
- `cargo tarpaulin -p ccsds_wire -- primitives` reports 100 % branch.
- Grep guard: `rg 'impl From<u16> for (Apid|SequenceCount|PacketDataLength|FuncCode)' rust/ccsds_wire/` → 0 (explicit fallible construction only).

**AI Prompting Strategy.**
- Feed: [`06 §2.2`, `§2.8`](docs/architecture/06-ground-segment-rust.md); Phase 03 merge commit as worked example.
- Agents: `@code-reviewer` post-green, focus on "no silent From impls".
- Output style: `concise` (pattern already established).

---

### Phase 05 — `PacketType` + full `CcsdsError` enum 🔴 [CRIT]
**Branch:** `feat/phase-05-error-enum` · **Model:** `claude-opus-4-7` · **Risk:** L · **Coverage:** 100 % ✅

**Objective.** Consolidate all error variants enumerated in [`06 §2.8`](docs/architecture/06-ground-segment-rust.md) into a single frozen enum. Replace Phase 03's placeholder. Add `PacketType { Tm, Tc }` discriminant.
**Source of Truth.** [`06 §2.8`](docs/architecture/06-ground-segment-rust.md) (nine variants; not `#[non_exhaustive]`).
**Dependency Path.** Phases 03, 04.
**Deliverables.**
- `rust/ccsds_wire/src/error.rs`: `CcsdsError` enum with exactly the nine variants listed in [`06 §2.8`](docs/architecture/06-ground-segment-rust.md). Derives `Debug, Error, PartialEq, Eq`. Uses `thiserror::Error` for messages.
- `rust/ccsds_wire/src/packet_type.rs`: `enum PacketType { Tm = 0, Tc = 1 }` with `fn from_bit(b: u8) -> PacketType` and `fn as_bit(self) -> u8`.
- All prior modules (Phases 03, 04) refactored to return `CcsdsError` instead of the Phase-03 placeholder; placeholder type deleted.

**Verification (DoD).**
- `cargo test -p ccsds_wire` green.
- `cargo clippy -p ccsds_wire -- -D warnings` exits 0.
- Grep guard: `rg '#\[non_exhaustive\]' rust/ccsds_wire/src/error.rs` → 0 (intentional — adding variants is semver-breaking per §2.8).

**AI Prompting Strategy.**
- Feed: [`06 §2.8`](docs/architecture/06-ground-segment-rust.md).
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 06 — `Cuc` codec + P-Field 0x2F 🔴 [CRIT]
**Branch:** `feat/phase-06-cuc-codec` · **Model:** `claude-opus-4-7` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Ship the CCSDS CUC time codec (7-byte encoded: 1 B P-Field + 4 B coarse seconds + 2 B fine units of 2⁻¹⁶ s). Enforce the P-Field = 0x2F invariant on decode. This is the single on-wire time representation for the entire mission.
**Source of Truth.** [`06 §2.3`](docs/architecture/06-ground-segment-rust.md); [`08 §2`](docs/architecture/08-timing-and-clocks.md); Q-C6; `SYS-REQ-0021`, `SYS-REQ-0030`.
**Dependency Path.** Phases 02, 05.
**Deliverables.**
- `rust/ccsds_wire/src/cuc.rs`: `pub struct Cuc { pub coarse: u32, pub fine: u16 }`, `pub const P_FIELD: u8 = 0x2F`, `pub fn encode_be(&self, out: &mut [u8; 7])`, `pub fn decode_be(buf: &[u8]) -> Result<Self, CcsdsError>`.
- `decode_be` rejects any P-Field ≠ 0x2F with `CcsdsError::InvalidPField(got)`; rejects buffer shorter than 7 with `CcsdsError::BufferTooShort`.
- Inline G/W/T unit tests: valid round-trip, bad P-Field, short buffer, boundary values (coarse=0, fine=0; coarse=u32::MAX, fine=u16::MAX).

**Verification (DoD).**
- Property test (scaffold; populated fully in Phase 10 milestone): `prop_cuc_roundtrip` — for arbitrary `(coarse, fine)`, decode(encode(x)) == x.
- 100 % branch on `cuc.rs`.
- `rg 'from_le_bytes|to_le_bytes' rust/ccsds_wire/src/cuc.rs` → 0 (BE only).

**AI Prompting Strategy.**
- Feed: [`06 §2.3`](docs/architecture/06-ground-segment-rust.md), [`08 §2`](docs/architecture/08-timing-and-clocks.md).
- Agents: `@architect` (CUC semantics review); `@code-reviewer` post-green.
- Output style: `educational` (time semantics are subtle — explain epoch 1958, leap-second-free TAI).

---

### Phase 07 — `PrimaryHeader` encode / decode 🔴 [CRIT]
**Branch:** `feat/phase-07-primary-header` · **Model:** `claude-opus-4-7` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Ship the 6-byte CCSDS primary header codec with the full set of decode-time guards from §2.4. This is the first end-to-end BE codec to exercise the newtypes from Phases 03–04.
**Source of Truth.** [`06 §2.4`](docs/architecture/06-ground-segment-rust.md); [`apid-registry.md`](docs/interfaces/apid-registry.md) primary-header table; `SYS-REQ-0020`, `SYS-REQ-0022`; Q-C8.
**Dependency Path.** Phases 03, 04, 05, 06.
**Deliverables.**
- `rust/ccsds_wire/src/primary.rs`: `pub struct PrimaryHeader` with private fields; accessors `apid`, `packet_type`, `sequence_count`, `data_length`; `pub const LEN: usize = 6`; `pub fn decode(buf: &[u8]) -> Result<Self, CcsdsError>`; `pub fn encode(&self, out: &mut [u8; Self::LEN])`.
- Decode-time guards: version `[7:5]` of byte 0 must be `0b000` → `InvalidVersion` on fail; sec-hdr flag must be 1 (SAKURA-II always stamps time); sequence-flags bits must be `0b11` → `SequenceFlagsNotStandalone` on fail; APID via `Apid::new` (yields `ApidOutOfRange`).
- Inline G/W/T unit tests per guard + happy-path roundtrip.

**Verification (DoD).**
- `cargo test -p ccsds_wire primary::` green.
- `cargo tarpaulin -p ccsds_wire -- primary` reports 100 % branch on `primary.rs`.
- Grep guard: `rg 'pub [a-z_]+: u(8|16|32)' rust/ccsds_wire/src/primary.rs` → 0 (no raw public numeric fields per §2.9).

**AI Prompting Strategy.**
- Feed: [`06 §2.4`, §2.9`](docs/architecture/06-ground-segment-rust.md).
- Agents: `@architect` (bit-field packing review); `@code-reviewer`.
- Output style: `educational` (bit-packing logic benefits from commentary in the commit body).

---

### Phase 08 — `SecondaryHeader` + `time_suspect` 🔴 [CRIT]
**Branch:** `feat/phase-08-secondary-header` · **Model:** `claude-opus-4-7` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Ship the 10-byte secondary header codec (7 B CUC + 2 B BE func code + 1 B instance ID) and expose the `time_suspect` flag derived from bit 0 of the TM func code.
**Source of Truth.** [`06 §2.5`](docs/architecture/06-ground-segment-rust.md); [`08 §4`](docs/architecture/08-timing-and-clocks.md); Q-C6; Q-F4; `SYS-REQ-0021`, `SYS-REQ-0034`.
**Dependency Path.** Phases 04, 05, 06.
**Deliverables.**
- `rust/ccsds_wire/src/secondary.rs`: `pub struct SecondaryHeader` private fields; accessors `time()`, `func_code()`, `instance_id()`, `time_suspect()`; `pub const LEN: usize = 10`; `decode`, `encode`.
- `time_suspect()` returns `self.func_code().get() & 0x0001 != 0`.
- Inline G/W/T tests: roundtrip, InstanceId==0 rejection, FuncCode==0 rejection, time_suspect flag on/off.

**Verification (DoD).**
- 100 % branch on `secondary.rs`.
- `cargo test -p ccsds_wire secondary::` green.

**AI Prompting Strategy.**
- Feed: [`06 §2.5`](docs/architecture/06-ground-segment-rust.md), [`08 §4`](docs/architecture/08-timing-and-clocks.md).
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 09 — `SpacePacket<'a>` + `PacketBuilder` 🟢 [FEAT]
**Branch:** `feat/phase-09-space-packet` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Compose Phases 07–08 into a borrowed `SpacePacket<'a>` view over caller-owned bytes (no allocation on the hot path) and a fluent `PacketBuilder` for the ground-TC path.
**Source of Truth.** [`06 §2.6`, §2.7`](docs/architecture/06-ground-segment-rust.md).
**Dependency Path.** Phases 07, 08.
**Deliverables.**
- `rust/ccsds_wire/src/packet.rs`: `pub struct SpacePacket<'a> { pub primary, pub secondary, pub user_data: &'a [u8] }`, `pub const HEADER_LEN: usize = 16`, `pub fn parse(buf: &'a [u8]) -> Result<Self, CcsdsError>`, `pub fn total_len(&self) -> usize`.
- `rust/ccsds_wire/src/builder.rs`: `pub struct PacketBuilder { ... }` with `tm(apid)`, `tc(apid)`, `func_code`, `instance_id`, `cuc`, `sequence_count`, `user_data`, `build() -> Result<Vec<u8>, CcsdsError>`.
- `parse` enforces `primary.data_length().get() as usize + 7 == buf.len()` or returns `LengthMismatch { declared, actual }`.

**Verification (DoD).**
- `cargo test -p ccsds_wire packet:: builder::` green.
- `cargo tarpaulin -p ccsds_wire` 100 % on both files.

**AI Prompting Strategy.**
- Feed: [`06 §2.6, §2.7`](docs/architecture/06-ground-segment-rust.md).
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 10 — MILESTONE 1/5 · `ccsds_wire v1.0` 🧪 [TEST]
**Branch:** `feat/phase-10-ccsds-wire-milestone` · **Model:** `claude-sonnet-4-6` (with `@architect` Opus review) · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Ship `ccsds_wire v1.0`: the seven property tests, the five known-answer tests, and the grep guard, all locked in. Tag `ccsds_wire-v1.0.0` on merge.
**Source of Truth.** [`06 §2.10`](docs/architecture/06-ground-segment-rust.md); [`packet-catalog.md §4`](docs/interfaces/packet-catalog.md); [`06 §12`](docs/architecture/06-ground-segment-rust.md) (excerpted); Q-C8.
**Dependency Path.** Phases 03–09.
**Deliverables.**
- `rust/ccsds_wire/tests/proptests.rs`: seven named properties exactly as in [`06 §2.10`](docs/architecture/06-ground-segment-rust.md) — `prop_primary_header_roundtrip`, `prop_secondary_header_roundtrip`, `prop_cuc_roundtrip`, `prop_space_packet_roundtrip`, `prop_rejects_short_buffer`, `prop_rejects_invalid_version`, `prop_rejects_invalid_pfield`.
- `rust/ccsds_wire/tests/known_answers.rs`: at least five hand-rolled KATs from [`packet-catalog.md §4`](docs/interfaces/packet-catalog.md) — `kat_pkt_tm_0100_0002`, `kat_pkt_tm_0101_0002`, `kat_pkt_tm_0110_0002`, `kat_pkt_tm_0400_0004`, `kat_pkt_tc_0184_8100`.
- CI step: `rg 'from_le_bytes|to_le_bytes' rust/ -g '!target'` must return 0 (added to `.github/workflows/*.yml` in Phase 49; land a shell-script equivalent in `scripts/grep-lints.sh` now).
- `CHANGELOG.md` entry in `rust/ccsds_wire/`.

**Verification (DoD — hard deliverable).**
- `cargo test -p ccsds_wire --all-targets` green.
- `cargo test -p ccsds_wire --test proptests` exits 0 (all seven names present).
- `cargo test -p ccsds_wire --test known_answers` exits 0 (all five KATs).
- `cargo tarpaulin -p ccsds_wire --out Xml` line + branch ≥ 100 % per file (milestone roll-up ≥ 85 %).
- `bash scripts/grep-lints.sh` exits 0.
- Git tag `ccsds_wire-v1.0.0` applied after merge.

**AI Prompting Strategy.**
- Feed: [`06 §2.10`, §12`](docs/architecture/06-ground-segment-rust.md); [`packet-catalog.md §4`](docs/interfaces/packet-catalog.md); Q-C8 entry in [`decisions-log.md`](docs/standards/decisions-log.md).
- Agents: `@architect` final sign-off; `@code-reviewer` full sweep; `@debugger` on any proptest regressions.
- Skills: `/security-review` (first run — validates BE locus invariant), `/code-review`, `/create-pr`.
- Output style: `concise`.

---

### Phase 11 — Mission rename + SCID hook 🔵 [ARCH]
**Branch:** `feat/phase-11-mission-rename` · **Model:** `claude-opus-4-7` · **Risk:** L · **Coverage:** 100 % ✅

**Objective.** Flip `MISSION_NAME` from `SAMPLE_MISSION` to `SAKURA_II` in `_defs/` and make SCID fleet-allocatable without re-editing C in every subsequent app.
**Source of Truth.** [`REPO_MAP.md §_defs/`](docs/REPO_MAP.md); [`apid-registry.md §Identifiers and Ranges`](docs/interfaces/apid-registry.md); `SYS-REQ-0026`.
**Dependency Path.** Phase 01.
**Deliverables.**
- `_defs/mission_config.h`: `#define SPACECRAFT_ID` kept at 42 but comment anchors fleet-policy + a second macro `#define SAKURA_II_SCID_BASE 42U` added so future instances derive by offset.
- `_defs/targets.cmake`: `set(MISSION_NAME "SAKURA_II")`. `MISSION_APPS` still `[sample_app]` until Phases 31–42 grow it.
- `rust/cfs_bindings/src/lib.rs`: regenerated bindgen output picks up the new macro; `mission::MISSION_NAME` re-exported as `"SAKURA_II"`.
- Doc cross-refs: grep and fix any stale `SAMPLE_MISSION` in `docs/` (should be minimal).

**Verification (DoD).**
- `cmake -B build && cmake --build build` green.
- `cargo test -p cfs_bindings` green (existing mission-invariant tests pick up the new name).
- `rg 'SAMPLE_MISSION' .` returns only expected historical references in `docs/REPO_MAP.md` changelog / this guide.

**AI Prompting Strategy.**
- Feed: [`REPO_MAP.md`](docs/REPO_MAP.md), `_defs/`.
- Agents: `@architect` for SCID allocation policy.
- Output style: `concise`.

---

### Phase 12 — `cfs_bindings` safe wrappers (CFE_MSG / CFE_SB) 🟢 [FEAT]
**Branch:** `feat/phase-12-cfs-msg-wrappers` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Expose the minimum cFS message types Rust ground tooling needs (`CFE_MSG_Message_t`, `CFE_SB_Buffer_t`, `CFE_SB_MsgId_t`) as safe wrappers so ground code can decode packets captured from a cFS app without dropping into raw bindgen output.
**Source of Truth.** [`06 §3.1`, §3.2`](docs/architecture/06-ground-segment-rust.md); [`apid-registry.md §cFE Message ID (MID) Scheme`](docs/interfaces/apid-registry.md).
**Dependency Path.** Phases 02, 11.
**Deliverables.**
- `rust/cfs_bindings/src/message.rs`: `struct Message`, `struct SbBuffer`, `struct MsgId(u16)` with `fn from_apid(apid: Apid, is_cmd: bool) -> Self` that encodes `0x1800 | apid` (TC) or `0x0800 | apid` (TM) per registry.
- All constructors fallible; no public raw-pointer fields.
- Inline G/W/T unit tests per wrapper.

**Verification (DoD).**
- `cargo test -p cfs_bindings message::` green.
- 100 % branch on `message.rs`.

**AI Prompting Strategy.**
- Feed: [`06 §3`](docs/architecture/06-ground-segment-rust.md); [`apid-registry.md`](docs/interfaces/apid-registry.md).
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 13 — `_defs/mids.h` + `cfs_bindings::mids` 🟢 [FEAT]
**Branch:** `feat/phase-13-mid-macros` · **Model:** `claude-sonnet-4-6` · **Risk:** L · **Coverage:** 100 % ✅

**Objective.** Define every APID from [`apid-registry.md`](docs/interfaces/apid-registry.md) once in `_defs/mids.h` as `#define`d MID macros, mirror them into `cfs_bindings::mids`, and forbid literal MIDs in any `apps/**` source.
**Source of Truth.** [`apid-registry.md`](docs/interfaces/apid-registry.md); [`.claude/rules/cfs-apps.md §MIDs`](.claude/rules/cfs-apps.md); `SYS-REQ-0026`.
**Dependency Path.** Phase 12.
**Deliverables.**
- `_defs/mids.h`: `#define SAMPLE_APP_HK_MID`, `SAMPLE_APP_CMD_MID`, `ORBITER_CDH_HK_MID`, `ORBITER_CDH_CMD_MID`, ... covering every allocated APID in the registry.
- `rust/cfs_bindings/src/mids.rs`: re-exports generated via bindgen + a verification test that every MID in `_defs/mids.h` has a matching registry row (parses the markdown table in `apid-registry.md`).
- `apps/sample_app` refactored to consume `SAMPLE_APP_HK_MID` / `SAMPLE_APP_CMD_MID` from `_defs/mids.h` instead of inline literals.

**Verification (DoD).**
- `ctest --test-dir build` green (sample_app rebuilds).
- `cargo test -p cfs_bindings mids::` green.
- Grep guard: `rg '0x1[0-9A-Fa-f]{3}U' apps/` returns only macro-definition sites, no inline literal MIDs (a false-positive allowlist may live in `scripts/grep-lints.sh`).

**AI Prompting Strategy.**
- Feed: [`apid-registry.md`](docs/interfaces/apid-registry.md), [`.claude/rules/cfs-apps.md`](.claude/rules/cfs-apps.md).
- Agents: `@researcher` (to sweep and classify every existing literal); `@code-reviewer`.
- Output style: `concise`.

---

### Phase 14 — FFI host ↔ `ccsds_wire` conversion helpers 🟢 [FEAT]
**Branch:** `feat/phase-14-ffi-conversion` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Provide the one-and-only Rust-side second BE conversion locus: wrappers that convert between host-endian cFS C structs and `ccsds_wire::SpacePacket`. This is Q-C8 locus B. Nothing else in the workspace may do BE↔LE.
**Source of Truth.** [`06 §3.2`](docs/architecture/06-ground-segment-rust.md); Q-C8.
**Dependency Path.** Phases 09, 12.
**Deliverables.**
- `rust/cfs_bindings/src/convert.rs`: `fn from_c_message(msg: &Message) -> Result<Vec<u8>, ...>` (host struct → BE bytes via `ccsds_wire::PacketBuilder`); `fn to_c_message(buf: &[u8]) -> Result<Message, ...>` (BE bytes via `ccsds_wire::SpacePacket::parse` → host struct).
- Tests: round-trip (C → bytes → C) equality on every field.

**Verification (DoD).**
- `cargo test -p cfs_bindings convert::` green.
- Grep guard (expanding Phase 10 scope to include `cfs_bindings`): `rg 'from_le_bytes|to_le_bytes' rust/ -g '!rust/ccsds_wire' -g '!rust/cfs_bindings' -g '!target'` → 0 matches.

**AI Prompting Strategy.**
- Feed: [`06 §3.2`](docs/architecture/06-ground-segment-rust.md); Q-C8.
- Agents: `@architect` (boundary review); `@code-reviewer`; `/security-review` (second run — buffer safety at FFI).
- Output style: `concise`.

---

### Phase 15 — Migrate `ground_station::telemetry` → `ccsds_wire` consumer 🔴 [CRIT]
**Branch:** `feat/phase-15-telemetry-migration` · **Model:** `claude-opus-4-7` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Delete the duplicate BE primary-header parser in `rust/ground_station/src/telemetry.rs` and replace with `use ccsds_wire::PrimaryHeader`, per [`06 §1.6`](docs/architecture/06-ground-segment-rust.md).
**Source of Truth.** [`06 §1.6`](docs/architecture/06-ground-segment-rust.md); Q-C8.
**Dependency Path.** Phase 10.
**Deliverables.**
- `rust/ground_station/src/telemetry.rs`: either **deleted** or reduced to a re-export shim `pub use ccsds_wire::{PrimaryHeader, CcsdsError};`.
- `rust/ground_station/src/main.rs`: `use ccsds_wire::PrimaryHeader;` replaces the local type.
- Existing tests either migrated to `ccsds_wire/tests/known_answers.rs` (Phase 10) or deleted.

**Verification (DoD).**
- `cargo test --workspace` green.
- `rg 'from_le_bytes|to_le_bytes|from_be_bytes|to_be_bytes' rust/ground_station/` → 0 matches.
- `rg 'struct TelemetryPacket' rust/` → 0 matches (legacy type eliminated).

**AI Prompting Strategy.**
- Feed: [`06 §1.6`](docs/architecture/06-ground-segment-rust.md); current `rust/ground_station/src/telemetry.rs`.
- Agents: `@architect` (migration plan); `@debugger` on any regression; `@code-reviewer`.
- Output style: `educational` (captures the "nothing else does BE" invariant in the commit body).

---

### Phase 16 — cfs_bindings round-trip test suite 🧪 [TEST]
**Branch:** `feat/phase-16-bindings-roundtrip-tests` · **Model:** `claude-sonnet-4-6` · **Risk:** L · **Coverage:** 100 % ✅

**Objective.** Harden Phase 14 with a dedicated integration test exercising C-struct → wire-bytes → SpacePacket → C-struct round-trips on every MID-class combination in `_defs/mids.h`.
**Source of Truth.** [`06 §3.2`](docs/architecture/06-ground-segment-rust.md); [`apid-registry.md`](docs/interfaces/apid-registry.md).
**Dependency Path.** Phases 13, 14.
**Deliverables.**
- `rust/cfs_bindings/tests/roundtrip.rs`: one test per MID macro covering TM and TC paths.

**Verification (DoD).**
- `cargo test -p cfs_bindings --test roundtrip` green.
- Coverage roll-up on `cfs_bindings` ≥ 100 % branch.

**AI Prompting Strategy.**
- Feed: phase 13/14 commits; [`apid-registry.md`](docs/interfaces/apid-registry.md).
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 17 — cFS app template CMake macro 🔵 [ARCH]
**Branch:** `feat/phase-17-cfs-app-template` · **Model:** `claude-opus-4-7` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Freeze the `apps/sample_app/` shape (src/ + unit-test/ + events header + version header) as a reusable CMake macro so every new orbiter or MCU-gateway app lands with the same scaffolding, tests, and `CFE_EVS_SendEvent` pattern from day one.
**Source of Truth.** [`.claude/rules/cfs-apps.md`](.claude/rules/cfs-apps.md); [`REPO_MAP.md §apps/`](docs/REPO_MAP.md); `apps/sample_app/` reference.
**Dependency Path.** Phases 13, 19 (circular but soft — land CMake macro first, test template rides Phase 19).
**Deliverables.**
- `_defs/cfs_app_template.cmake`: `function(sakura_add_cfs_app app_name)` that wires src/ + unit-test/ + CTest target + cppcheck target.
- `apps/sample_app/CMakeLists.txt`: refactored to `sakura_add_cfs_app(sample_app)`.
- Generator note in `_defs/targets.cmake` listing the MISSION_APPS that new apps plug into.

**Verification (DoD).**
- `cmake --build build` green; `ctest` green (sample_app retains tests).
- `cppcheck --enable=all --std=c17 apps/` clean.

**AI Prompting Strategy.**
- Feed: `apps/sample_app/CMakeLists.txt`, [`.claude/rules/cfs-apps.md`](.claude/rules/cfs-apps.md).
- Agents: `@architect`.
- Output style: `educational` (documents the onboarding contract for all future apps).

---

### Phase 18 — MISRA C:2012 cppcheck baseline + CI hook 🟡 [LCC]
**Branch:** `feat/phase-18-misra-baseline` · **Model:** `claude-sonnet-4-6` · **Risk:** L · **Coverage:** n/a (gate)

**Objective.** Run `cppcheck --enable=all --std=c17 apps/` once, snapshot zero baseline findings, and wire a blocking CI check so future PRs cannot introduce new findings.
**Source of Truth.** [`CLAUDE.md §Build & Test Commands`](CLAUDE.md); [`.claude/rules/security.md`](.claude/rules/security.md); `SYS-REQ-0070`.
**Dependency Path.** Phases 11, 17.
**Deliverables.**
- `scripts/cppcheck-gate.sh` — wraps cppcheck with per-file severity filters and baseline diff.
- CI stub in `scripts/ci/` (wired to Actions in Phase 49).

**Verification (DoD).**
- `bash scripts/cppcheck-gate.sh` exits 0 on current tree.
- Baseline file `cppcheck-baseline.txt` committed.

**AI Prompting Strategy.**
- Feed: current cppcheck output; [`.claude/rules/security.md`](.claude/rules/security.md).
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 19 — CMocka template + branch-coverage gate 🟡 [LCC]
**Branch:** `feat/phase-19-cmocka-template` · **Model:** `claude-sonnet-4-6` · **Risk:** L · **Coverage:** 100 % ✅

**Objective.** Codify the CMocka test-file pattern (G/W/T names, CFE stubs, `UNIT_TEST` guard) and wire gcov / llvm-cov so coverage gates apply automatically to every app added in Block 4.
**Source of Truth.** [`.claude/rules/testing.md`](.claude/rules/testing.md); `apps/sample_app/fsw/unit-test/sample_app_test.c` reference.
**Dependency Path.** Phase 17.
**Deliverables.**
- Template test file installed by `sakura_add_cfs_app` macro (Phase 17 extension): `apps/<name>/fsw/unit-test/<name>_test.c` boilerplate.
- `scripts/coverage-gate.sh` wrapping `gcov` + threshold check (100 % branch per file for changed files).

**Verification (DoD).**
- `ctest --test-dir build --output-on-failure` green.
- `bash scripts/coverage-gate.sh` exits 0 for `apps/sample_app`.

**AI Prompting Strategy.**
- Feed: [`.claude/rules/testing.md`](.claude/rules/testing.md); sample_app test file.
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 20 — MILESTONE 2/5 · `cfs_bindings v1.0` + cFS template 🧪 [TEST]
**Branch:** `feat/phase-20-bindings-milestone` · **Model:** `claude-sonnet-4-6` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Ship `cfs_bindings v1.0` and the cFS app template together as the "HAL / FFI bridge" hard deliverable. Any subsequent cFS app (Phases 31–35, 41–42) plugs into this template without architecture re-litigation.
**Source of Truth.** [`06 §3`](docs/architecture/06-ground-segment-rust.md); Phases 11–19.
**Dependency Path.** Phases 11–19.
**Deliverables.**
- Git tag `cfs_bindings-v1.0.0` on merge.
- `CHANGELOG.md` in `rust/cfs_bindings/`.
- `_defs/cfs_app_template.cmake` documented in `docs/REPO_MAP.md` (README update only — no new doc).
- cppcheck baseline + coverage gate passing.

**Verification (DoD — hard deliverable).**
- `cargo test -p cfs_bindings --all-targets` green.
- `cargo clippy -p cfs_bindings -- -D warnings` exits 0.
- `ctest --test-dir build` green (sample_app retains tests via new template).
- `bash scripts/cppcheck-gate.sh` exits 0.
- `bash scripts/coverage-gate.sh apps/sample_app` exits 0.
- `rg 'from_le_bytes|to_le_bytes' rust/ -g '!rust/ccsds_wire' -g '!rust/cfs_bindings' -g '!target'` → 0 matches.
- `python3 scripts/traceability-lint.py` exits 0.

**AI Prompting Strategy.**
- Feed: Phase 10 milestone as worked example; [`06 §3`](docs/architecture/06-ground-segment-rust.md).
- Agents: `@architect` sign-off; `@code-reviewer` full sweep; `/security-review` on the FFI boundary.
- Output style: `concise`.

---

### Phase 21 — `ground_station::{ingest,uplink,cfdp,mfile,ui}` scaffold 🔵 [ARCH]
**Branch:** `feat/phase-21-ground-station-scaffold` · **Model:** `claude-opus-4-7` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Carve the ground-station binary into the five submodules of [`06 §5–10`](docs/architecture/06-ground-segment-rust.md) and stand up the tokio async runtime entry point. Empty modules, typed channels, no logic yet.
**Source of Truth.** [`06 §5.1`](docs/architecture/06-ground-segment-rust.md) (flow diagram); [`06 §5.3`](docs/architecture/06-ground-segment-rust.md) (channel capacities).
**Dependency Path.** Phases 15, 20.
**Deliverables.**
- `rust/ground_station/src/{ingest,uplink,cfdp,mfile,ui}/mod.rs` — empty, documented.
- `rust/ground_station/src/main.rs`: `#[tokio::main]`; command-line arg parsing (clap or `std::env`); stub pipeline wiring.

**Verification (DoD).**
- `cargo build -p ground_station` green.
- `cargo clippy -p ground_station -- -D warnings` exits 0.

**AI Prompting Strategy.**
- Feed: [`06 §5`](docs/architecture/06-ground-segment-rust.md).
- Agents: `@architect`.
- Output style: `educational`.

---

### Phase 22 — `AosFramer` 🟢 [FEAT]
**Branch:** `feat/phase-22-aos-framer` · **Model:** `claude-sonnet-4-6` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Implement the AOS 1024-byte transfer frame receiver: frame sync, FECF CRC-16/CCITT-FALSE verification, OCF (CLCW) extraction, and publication to a `tokio::sync::watch` for Cop1Engine (Phase 28).
**Source of Truth.** [`06 §5.2`, §6.5`, §9.1, §9.3`](docs/architecture/06-ground-segment-rust.md); Q-C4; `SYS-REQ-0023`.
**Dependency Path.** Phase 21.
**Deliverables.**
- `rust/ground_station/src/ingest/framer.rs`: `struct AosFramer`, `struct AosFrame { vc_id, ocf, data_field }`, async `fn run(&mut self, reader: impl AsyncRead)`.
- FECF mismatch → discard + `aos_fecf_errors_total.inc()` + rate-limited `AOS-FECF-MISMATCH` event (≤ 1/s).
- Link-state tracker (`Aos/Los/Degraded`) with the transition rules from [`06 §9.3`](docs/architecture/06-ground-segment-rust.md).
- G/W/T tests: valid frame, bad FECF, missing OCF, back-to-back idle frames.

**Verification (DoD).**
- `cargo test -p ground_station framer::` green; 100 % branch.
- Latency budget assertion (micro-bench): FECF over 1024 B < 5 ms (per [`06 §5.5`](docs/architecture/06-ground-segment-rust.md)).

**AI Prompting Strategy.**
- Feed: [`06 §5`, §6.5`, §9`](docs/architecture/06-ground-segment-rust.md); [`07 §3`](docs/architecture/07-comms-stack.md).
- Agents: `@architect` (frame-boundary correctness); `@code-reviewer`.
- Output style: `educational` (CLCW/OCF semantics are subtle).

---

### Phase 23 — `VcDemultiplexer` 🟢 [FEAT]
**Branch:** `feat/phase-23-vc-demux` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Fan out AOS frames to per-VC `tokio::sync::mpsc` bounded channels with the capacities specified in [`06 §5.3`](docs/architecture/06-ground-segment-rust.md). Unknown VC → discard + `EVENT-AOS-UNKNOWN-VC`.
**Source of Truth.** [`06 §5.2, §5.3`](docs/architecture/06-ground-segment-rust.md); [`apid-registry.md §AOS VC`](docs/interfaces/apid-registry.md).
**Dependency Path.** Phase 22.
**Deliverables.**
- `rust/ground_station/src/ingest/demux.rs`: `struct VcDemultiplexer`, configurable VC-ID → sender map (default covers VC 0/1/2/3/63).
- Backpressure: `try_send` full → drop + `<stage>_dropped_total.inc()` + rate-limited `EVENT-INGEST-BACKPRESSURE`.
- G/W/T tests: happy path per VC, unknown VC rejection, full-channel backpressure event.

**Verification (DoD).**
- `cargo test -p ground_station demux::` green; 100 % branch.

**AI Prompting Strategy.**
- Feed: [`06 §5.3`](docs/architecture/06-ground-segment-rust.md).
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 24 — `SppDecoder` 🟢 [FEAT]
**Branch:** `feat/phase-24-spp-decoder` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Parse per-VC M_PDU bytes into `ccsds_wire::SpacePacket<'_>` views and forward to `ApidRouter` (Phase 25). Decode failures logged once per variant with rate-limited events.
**Source of Truth.** [`06 §5.2`](docs/architecture/06-ground-segment-rust.md).
**Dependency Path.** Phases 09, 23.
**Deliverables.**
- `rust/ground_station/src/ingest/decoder.rs`: `struct SppDecoder`, async loop calling `SpacePacket::parse`.
- Decode error → discard + labeled counter + rate-limited `EVENT-SPP-DECODE-FAIL` with error-variant label.

**Verification (DoD).**
- `cargo test -p ground_station decoder::` green; 100 % branch.

**AI Prompting Strategy.**
- Feed: [`06 §5.2`](docs/architecture/06-ground-segment-rust.md); `ccsds_wire::SpacePacket`.
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 25 — `ApidRouter` + fault-APID rejection 🔴 [CRIT]
**Branch:** `feat/phase-25-apid-router` · **Model:** `claude-opus-4-7` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Dispatch packets to HK / EventLog / CfdpPdu / RoverForward / IdleFill / Rejected sinks per [`06 §5.4`](docs/architecture/06-ground-segment-rust.md). Security-critical: reject APIDs `0x540`–`0x543` on RF regardless of VC (sideband-only per Q-F2); same for `0x500`–`0x53F`, `0x544`–`0x57F`, `0x600`–`0x67F`.
**Source of Truth.** [`06 §5.4, §8.2`](docs/architecture/06-ground-segment-rust.md); Q-F2; `SYS-REQ-0041`.
**Dependency Path.** Phase 24.
**Deliverables.**
- `rust/ground_station/src/ingest/router.rs`: `enum Route { Hk, EventLog, CfdpPdu, RoverForward, IdleFill, Rejected { reason: RejectReason } }`; `enum RejectReason { ForbiddenFaultInjectApid, ForbiddenSimApid, ForbiddenGroundInternal, UnknownBlock }`; `struct ApidRouter { ... }` `fn route(&self, vc_id: u8, pkt: &SpacePacket<'_>) -> Route`.
- Security test suite: `rejects_fault_apids_on_rf` covering each of 0x540, 0x541, 0x542, 0x543; identical pattern for sim + ground-internal blocks.
- Metric: `forbidden_apid_seen_total` labeled by APID.

**Verification (DoD).**
- `cargo test -p ground_station ingest::router::rejects_fault_apids_on_rf` passes for **each** of 0x540/0x541/0x542/0x543.
- 100 % branch on `router.rs` (every reject-reason arm tested).
- `/security-review` run; CRITICAL findings empty.

**AI Prompting Strategy.**
- Feed: [`06 §5.4, §8.2`](docs/architecture/06-ground-segment-rust.md); Q-F2; `SYS-REQ-0041`.
- Agents: `@architect`, `@code-reviewer`; `/security-review` (third run — fault-APID rejection).
- Output style: `educational` (security-critical path deserves explanatory commits).

---

### Phase 26 — `CfdpProvider` + `Class1Receiver` 🟢 [FEAT]
**Branch:** `feat/phase-26-cfdp-class1` · **Model:** `claude-sonnet-4-6` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Implement the `CfdpReceiver` + `CfdpProvider` trait pair from [`06 §4.2`](docs/architecture/06-ground-segment-rust.md) and `Class1Receiver` as a `cfdp-core`-backed implementation. Resolves Q-C3 from `pending → resolved`.
**Source of Truth.** [`06 §4`](docs/architecture/06-ground-segment-rust.md); [`07 §5.2`](docs/architecture/07-comms-stack.md) (frozen receiver signature); Q-C2, Q-C3; `SYS-REQ-0025`.
**Dependency Path.** Phases 21, 25.
**Deliverables.**
- `rust/ground_station/src/cfdp/mod.rs`: trait pair + `TransactionId`, `CfdpError`, `TransactionOutcome` types from [`06 §4.2`](docs/architecture/06-ground-segment-rust.md).
- `rust/ground_station/src/cfdp/class1.rs`: `struct Class1Receiver` wrapping `cfdp_core::Daemon`, cap = 16 concurrent transactions, CRC-32 (IEEE 802.3) check, timeout = 10 × OWLT.
- `rust/ground_station/src/cfdp/adapter.rs`: TAI (CUC epoch 1958) ↔ Unix epoch at the `cfdp-core` boundary — exactly here, nowhere else.
- `docs/standards/decisions-log.md`: Q-C3 row status flipped `pending → resolved`.

**Verification (DoD).**
- `cargo test -p ground_station cfdp::` green; 100 % branch (each error/timeout/outcome path).
- `/security-review` (fourth run — file I/O, CRC-32, partial-file semantics).
- `python3 scripts/traceability-lint.py` green (Q-C3 status change propagates).

**AI Prompting Strategy.**
- Feed: [`06 §4`](docs/architecture/06-ground-segment-rust.md); [`07 §5`](docs/architecture/07-comms-stack.md); Q-C3 entry.
- Agents: `@architect` (trait boundary); `@researcher` (cfdp-core crate behavior); `@code-reviewer`.
- Output style: `educational` (first stateful protocol engine).

---

### Phase 27 — `MFileAssembler` 🟢 [FEAT]
**Branch:** `feat/phase-27-mfile-assembler` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Build the reusable out-of-order chunk reassembler for both M-File (surface assets forwarded by the relay) and CFDP Class 1 file-data PDUs, per [`06 §7`](docs/architecture/06-ground-segment-rust.md).
**Source of Truth.** [`06 §7`](docs/architecture/06-ground-segment-rust.md); [`ICD-relay-surface.md §6`](docs/interfaces/ICD-relay-surface.md).
**Dependency Path.** Phase 26.
**Deliverables.**
- `rust/ground_station/src/mfile/mod.rs`: `struct MFileAssembler` — `BTreeMap<u32, Vec<u8>>` chunk map, `BitSet` received index, `timeout_tai: Cuc`, CRC-32 EOF verification, `MFILE_MAX_ASSEMBLY_RAM_MB` cap honoring oldest-wins eviction.
- Duplicate semantics: matching content → `dup_ok_total.inc()` and discard; differing → `dup_mismatch_total.inc()` + keep first + `MFILE-DUP-MISMATCH` event.

**Verification (DoD).**
- `cargo test -p ground_station mfile::` green; 100 % branch; cases for in-order, out-of-order, duplicate (match + mismatch), timeout, RAM-cap breach.

**AI Prompting Strategy.**
- Feed: [`06 §7`](docs/architecture/06-ground-segment-rust.md); [`ICD-relay-surface.md §6`](docs/interfaces/ICD-relay-surface.md).
- Agents: `@architect`; `@code-reviewer`.
- Output style: `concise`.

---

### Phase 28 — TC uplink pipeline 🟢 [FEAT]
**Branch:** `feat/phase-28-tc-uplink` · **Model:** `claude-sonnet-4-6` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Implement the catalog → `Cop1Engine` (FOP-1 state machine) → `TcFramer` (512 B SDLP) path with CLCW feedback from the AOS framer's `watch` channel. Includes emergency Type-BD on VC 7.
**Source of Truth.** [`06 §6`](docs/architecture/06-ground-segment-rust.md); [`07 §3`](docs/architecture/07-comms-stack.md); [`packet-catalog.md`](docs/interfaces/packet-catalog.md); `SYS-REQ-0023`.
**Dependency Path.** Phases 22, 25.
**Deliverables.**
- `rust/ground_station/src/uplink/builder.rs`: `struct TcBuilder` validates operator intent against `packet-catalog.md`.
- `rust/ground_station/src/uplink/cop1.rs`: `struct Cop1Engine` FOP-1 state machine (window=15, T1=2×(OWLT+5 s), max_retrans=3).
- `rust/ground_station/src/uplink/framer.rs`: `struct TcFramer`, 512 B SDLP framing, VC 0/1 AD + VC 7 BD bypass.
- G/W/T tests: every FOP-1 transition diagrammed in [`06 §6.4`](docs/architecture/06-ground-segment-rust.md).

**Verification (DoD).**
- `cargo test -p ground_station uplink::` green; 100 % branch (every FOP-1 state); include a state-exhaustion test driving retransmits ≥ 3 → INITIAL.
- `/security-review` (fifth run — command validation, emergency BD path).

**AI Prompting Strategy.**
- Feed: [`06 §6`](docs/architecture/06-ground-segment-rust.md); [`07 §3`](docs/architecture/07-comms-stack.md); [`packet-catalog.md`](docs/interfaces/packet-catalog.md).
- Agents: `@architect` (state machine review); `@code-reviewer`.
- Output style: `educational`.

---

### Phase 29 — Operator-UI backend surfaces 🟢 [FEAT]
**Branch:** `feat/phase-29-ui-backend` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Expose the seven surfaces listed in [`06 §10.1`](docs/architecture/06-ground-segment-rust.md) over WebSocket + REST. TAI → UTC conversion lives at the JSON serialization boundary (nowhere else). Framework choice deferred to `docs/dev/ground-ui.md`; backend emits serde-ready shapes.
**Source of Truth.** [`06 §10`](docs/architecture/06-ground-segment-rust.md); [`08 §5.5`](docs/architecture/08-timing-and-clocks.md); `SYS-REQ-0060`, `SYS-REQ-0061`.
**Dependency Path.** Phases 22–28.
**Deliverables.**
- `rust/ground_station/src/ui/mod.rs`: WebSocket + REST endpoints for the seven surfaces.
- `rust/ground_station/src/ui/time.rs`: TAI → UTC converter (the only place UTC exists).
- Command-validity window check (rejects TCs whose auth window expired given current light-time estimate).

**Verification (DoD).**
- `cargo test -p ground_station ui::` green; 100 % branch.
- G/W/T: command validity window expired → TC rejected (`SYS-REQ-0061`).

**AI Prompting Strategy.**
- Feed: [`06 §10`](docs/architecture/06-ground-segment-rust.md); [`08 §5`](docs/architecture/08-timing-and-clocks.md).
- Agents: `@architect`; `@code-reviewer`.
- Output style: `concise`.

---

### Phase 30 — MILESTONE 3/5 · `ground_station v0.1` (Phase C Step 2 gate) 🧪 [TEST]
**Branch:** `feat/phase-30-ground-station-milestone` · **Model:** `claude-sonnet-4-6` (with Opus review) · **Risk:** H · **Coverage:** ≥ 85 % `ccsds_wire` / ≥ 70 % `ground_station::{cfdp,ingest}`

**Objective.** Ship `ground_station v0.1` binary. Satisfy the entire acceptance gate defined in [`06 §12`](docs/architecture/06-ground-segment-rust.md) — all 13 checkboxes. This phase **unblocks Phase C Step 2** and flips Q-C8 to `resolved`.
**Source of Truth.** [`06 §12`](docs/architecture/06-ground-segment-rust.md) — authoritative; not duplicated here.
**Dependency Path.** Phases 21–29.
**Deliverables.**
- Git tag `ground_station-v0.1.0` on merge.
- `CHANGELOG.md` in `rust/ground_station/`.
- `docs/standards/decisions-log.md`: Q-C3 and Q-C8 rows flipped `pending → resolved`.
- `rust/ground_station/README.md`: one-page quickstart (`cargo run -p ground_station`).

**Verification (DoD — hard deliverable).** Every checkbox in [`docs/architecture/06-ground-segment-rust.md §12`](docs/architecture/06-ground-segment-rust.md) passes — 13 items, no skipping. Additionally:
- `python3 scripts/traceability-lint.py` exits 0.
- `@code-reviewer`, `@architect`, and `/security-review` all green.

**AI Prompting Strategy.**
- Feed: [`06` entire doc](docs/architecture/06-ground-segment-rust.md); Phase 10 and 20 tags as worked examples.
- Agents: `@architect` sign-off; `@code-reviewer` full sweep; `/security-review` full comms/crypto/buffer pass; `@debugger` if any gate fails.
- Skills: `/security-review`, `/code-review`, `/create-pr`.
- Output style: `concise`.

---

### Phase 31 — `apps/orbiter_cdh` 🟢 [FEAT]
**Branch:** `feat/phase-31-orbiter-cdh` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** First post-scaffold cFS app: Command & Data Handling. Aggregates per-app HK, dispatches orbiter mode transitions, applies EVS filter commands. Exercises the `sakura_add_cfs_app` macro end-to-end.
**Source of Truth.** [`docs/architecture/01-orbiter-cfs.md`](docs/architecture/01-orbiter-cfs.md); [`apid-registry.md`](docs/interfaces/apid-registry.md) orbiter TM 0x101–0x10F, TC 0x181; `FSW-SRD.md` (CDH section); [`.claude/rules/cfs-apps.md`](.claude/rules/cfs-apps.md).
**Dependency Path.** Phase 20.
**Deliverables.**
- `apps/orbiter_cdh/` via `sakura_add_cfs_app(orbiter_cdh)`: `fsw/src/{orbiter_cdh.c, .h, _events.h, _version.h}`, `fsw/unit-test/orbiter_cdh_test.c`.
- `_defs/targets.cmake`: `MISSION_APPS` adds `orbiter_cdh`.
- HK aggregation (subscribes to every orbiter TM MID, publishes combined HK on 0x101).

**Verification (DoD).**
- `ctest` green; `cppcheck` clean; `bash scripts/coverage-gate.sh apps/orbiter_cdh` exits 0.
- G/W/T tests per command code (NOOP / RESET / mode transition).

**AI Prompting Strategy.**
- Feed: [`01-orbiter-cfs.md`](docs/architecture/01-orbiter-cfs.md); [`apid-registry.md`](docs/interfaces/apid-registry.md); `apps/sample_app/` as template.
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 32 — `apps/orbiter_adcs` 🟢 [FEAT]
**Branch:** `feat/phase-32-orbiter-adcs` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Attitude Determination & Control app. Publishes attitude quaternion (0x110), wheel telemetry (0x111–0x11F) sourced from `mcu_rwa_gw` SB messages, accepts target quaternion on 0x182 TC.
**Source of Truth.** [`01-orbiter-cfs.md`](docs/architecture/01-orbiter-cfs.md); [`apid-registry.md`](docs/interfaces/apid-registry.md); `FSW-SRD.md` (ADCS); Q-F4 (time-suspect).
**Dependency Path.** Phases 31, 35 (soft — gateway lands Phase 35; stub SB ingress until then).
**Deliverables.**
- `apps/orbiter_adcs/` via template.
- Quaternion validation (unit-norm within tolerance) at TC ingestion.

**Verification (DoD).**
- G/W/T: valid quat accepted; non-unit quat rejected → ErrCounter++.
- `ctest` + `cppcheck` + coverage gate.

**AI Prompting Strategy.**
- Feed: [`01-orbiter-cfs.md`](docs/architecture/01-orbiter-cfs.md).
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 33 — `apps/orbiter_comm` 🟢 [FEAT]
**Branch:** `feat/phase-33-orbiter-comm` · **Model:** `claude-sonnet-4-6` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Radio manager + CFDP Class 1 file receiver on the orbiter. Drives AOS VC 0/1/2 emission, tracks link budget, exposes downlink rate control on 0x183.
**Source of Truth.** [`01-orbiter-cfs.md`](docs/architecture/01-orbiter-cfs.md); [`07-comms-stack.md`](docs/architecture/07-comms-stack.md); [`ICD-orbiter-ground.md`](docs/interfaces/ICD-orbiter-ground.md); `SYS-REQ-0023, 0025`.
**Dependency Path.** Phase 31.
**Deliverables.**
- `apps/orbiter_comm/` via template.
- CFDP Class 1 receive loop (C-side — consumes `cfs_bindings` types via generated headers, no direct `from_be_bytes` — Q-C8).

**Verification (DoD).**
- `ctest` + `cppcheck` + coverage gate.
- `/security-review` (sixth run — comms surface).

**AI Prompting Strategy.**
- Feed: [`01-orbiter-cfs.md`](docs/architecture/01-orbiter-cfs.md); [`07-comms-stack.md`](docs/architecture/07-comms-stack.md); [`ICD-orbiter-ground.md`](docs/interfaces/ICD-orbiter-ground.md).
- Agents: `@architect`; `@code-reviewer`; `/security-review`.
- Output style: `educational`.

---

### Phase 34 — `apps/orbiter_power` + `apps/orbiter_payload` 🟢 [FEAT]
**Branch:** `feat/phase-34-orbiter-power-payload` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Two apps bundled: EPS telemetry (0x130–0x13F via `mcu_eps_gw`), safety-interlocked load switches (0x184); payload on/off + science mode (0x140–0x15F TM, 0x185 TC).
**Source of Truth.** [`01-orbiter-cfs.md`](docs/architecture/01-orbiter-cfs.md); [`apid-registry.md`](docs/interfaces/apid-registry.md).
**Dependency Path.** Phase 31.
**Deliverables.**
- Both apps via template.
- Safety interlock: `orbiter_power` never toggles a load switch whose interlock table row says "prohibited given mode".

**Verification (DoD).**
- G/W/T per interlock row.
- `ctest` + `cppcheck` + coverage gate.
- `/security-review` (seventh run — interlock logic).

**AI Prompting Strategy.**
- Feed: [`01-orbiter-cfs.md`](docs/architecture/01-orbiter-cfs.md).
- Agents: `@code-reviewer`; `/security-review`.
- Output style: `concise`.

---

### Phase 35 — MCU gateway apps 🟢 [FEAT]
**Branch:** `feat/phase-35-mcu-gateways` · **Model:** `claude-sonnet-4-6` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Ship the three cFS-side gateways that bridge the SB to simulated physical buses (SpW / CAN / UART-HDLC), per `ICD-mcu-cfs.md`.
**Source of Truth.** [`03-subsystem-mcus.md`](docs/architecture/03-subsystem-mcus.md); [`ICD-mcu-cfs.md`](docs/interfaces/ICD-mcu-cfs.md); Q-H4.
**Dependency Path.** Phase 31.
**Deliverables.**
- `apps/mcu_payload_gw/` (SpW, 0x280–0x28F).
- `apps/mcu_rwa_gw/` (CAN 2.0A, 0x290–0x29F).
- `apps/mcu_eps_gw/` (UART/HDLC, 0x2A0–0x2AF).
- Each app: SB ingress/egress + bus driver stub (real driver lands with MCU firmware in Phase 42).

**Verification (DoD).**
- `ctest` + `cppcheck` + coverage gate on each gateway.
- G/W/T: frame corruption → drop + EVS event, never SB-publish.

**AI Prompting Strategy.**
- Feed: [`03-subsystem-mcus.md`](docs/architecture/03-subsystem-mcus.md); [`ICD-mcu-cfs.md`](docs/interfaces/ICD-mcu-cfs.md).
- Agents: `@architect`; `@code-reviewer`.
- Output style: `concise`.

---

### Phase 36 — `ros2_ws/src/rover_common` 🟢 [FEAT]
**Branch:** `feat/phase-36-rover-common` · **Model:** `claude-sonnet-4-6` · **Risk:** L · **Coverage:** 100 % ✅

**Objective.** Shared QoS profiles (as named constants — [`ros2-nodes.md`](.claude/rules/ros2-nodes.md)), msg types, `tm_bridge` helper (forwards ROS 2 state to `ccsds_wire::PacketBuilder` via `cfs_bindings` types), and a `LifecycleNode` base utility for Phase 37 classes.
**Source of Truth.** [`04-rovers-spaceros.md`](docs/architecture/04-rovers-spaceros.md); [`.claude/rules/ros2-nodes.md`](.claude/rules/ros2-nodes.md).
**Dependency Path.** Phase 20.
**Deliverables.**
- `ros2_ws/src/rover_common/` (ament_cmake): `include/rover_common/qos.hpp` (named QoS constants), msg types, `tm_bridge.{hpp,cpp}`.

**Verification (DoD).**
- `colcon test` green on `rover_common`; 100 % branch on `tm_bridge`.

**AI Prompting Strategy.**
- Feed: [`04-rovers-spaceros.md`](docs/architecture/04-rovers-spaceros.md); [`ros2-nodes.md`](.claude/rules/ros2-nodes.md); `rover_teleop` as template.
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 37 — `rover_land` + `rover_uav` + `rover_cryobot` 🟢 [FEAT]
**Branch:** `feat/phase-37-rover-classes` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Three lifecycle-node packages, one per asset class. Each exposes at minimum: locomotion/flight/drill nav, state estimation, TM bridge to CCSDS via `rover_common::tm_bridge`.
**Source of Truth.** [`04-rovers-spaceros.md`](docs/architecture/04-rovers-spaceros.md); [`apid-registry.md §Rover-*`](docs/interfaces/apid-registry.md); [`ICD-cryobot-tether.md`](docs/interfaces/ICD-cryobot-tether.md) (cryobot HDLC-lite, Q-C9); `ROVER-SRD.md`.
**Dependency Path.** Phase 36.
**Deliverables.**
- `ros2_ws/src/rover_land/` — wheeled locomotion node, nav.
- `ros2_ws/src/rover_uav/` — flight control, state est.
- `ros2_ws/src/rover_cryobot/` — drill, tether comm (HDLC-lite encoder per Q-C9), low-BW TM.
- Full lifecycle test per package: configure → activate → deactivate → cleanup round trip ([`ros2-nodes.md`](.claude/rules/ros2-nodes.md)).

**Verification (DoD).**
- `colcon test` green on each package.
- Cryobot tether frame roundtrip test (HDLC-lite + CRC-16/CCITT-FALSE).

**AI Prompting Strategy.**
- Feed: [`04-rovers-spaceros.md`](docs/architecture/04-rovers-spaceros.md); [`ICD-cryobot-tether.md`](docs/interfaces/ICD-cryobot-tether.md); `rover_teleop` as template.
- Agents: `@architect` (cryobot tether); `@code-reviewer`.
- Output style: `concise`.

---

### Phase 38 — Gazebo UAV / cryobot / world plugins 🟢 [FEAT]
**Branch:** `feat/phase-38-gazebo-plugins` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Three `ModelPlugin` subclasses per [`05-simulation-gazebo.md`](docs/architecture/05-simulation-gazebo.md): UAV dynamics, cryobot physics + tether spool, world (dust/gravity/irradiance).
**Source of Truth.** [`05-simulation-gazebo.md`](docs/architecture/05-simulation-gazebo.md); `gazebo_rover_plugin` as reference.
**Dependency Path.** Phase 17 (template) + 37.
**Deliverables.**
- `simulation/gazebo_uav_plugin/`, `simulation/gazebo_cryobot_plugin/`, `simulation/gazebo_world_plugin/` — each a SHARED library built by CMake, each with its own unit test harness.

**Verification (DoD).**
- `cmake --build build` green.
- `OnUpdate` non-allocation assertion (allocation sentinel in test hook — fail if malloc called).

**AI Prompting Strategy.**
- Feed: [`05-simulation-gazebo.md`](docs/architecture/05-simulation-gazebo.md); `gazebo_rover_plugin` as template.
- Agents: `@architect`; `@code-reviewer`.
- Output style: `concise`.

---

### Phase 39 — `simulation/fault_injector` 🟢 [FEAT]
**Branch:** `feat/phase-39-fault-injector` · **Model:** `claude-sonnet-4-6` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Scenario-YAML runner that emits sideband SPPs on APIDs 0x540 (packet drop), 0x541 (clock skew), 0x542 (force safe-mode), 0x543 (sensor-noise corruption). Compile-out via `CFS_FLIGHT_BUILD` guard guaranteed by `SYS-REQ-0041`.
**Source of Truth.** [`ICD-sim-fsw.md`](docs/interfaces/ICD-sim-fsw.md); Q-F1, Q-F2; `SYS-REQ-0040, 0041`.
**Dependency Path.** Phase 38.
**Deliverables.**
- `simulation/fault_injector/` — Rust or C++ (prefer Rust — reuses `ccsds_wire::PacketBuilder`). Emits SPPs over a Unix domain socket or localhost UDP into the cFS sim container.
- `simulation/scenarios/` — initial YAML scenarios: `SCN-NOM-01.yaml`, `SCN-OFF-01-clockskew.yaml`, `SCN-OFF-02-safemode.yaml`.

**Verification (DoD).**
- Integration test: fault_injector → cFS sim → `orbiter_cdh` receives expected APID; ground `ApidRouter` *would* reject it on RF (verified by wiring the same SPP into the Phase 25 router test and asserting rejection).
- `/security-review` (eighth run — fault path isolation).
- Compile-out check: `grep -r "fault_injector" apps/` returns 0 matches.

**AI Prompting Strategy.**
- Feed: [`ICD-sim-fsw.md`](docs/interfaces/ICD-sim-fsw.md); Q-F1, Q-F2.
- Agents: `@architect`; `@code-reviewer`; `/security-review`.
- Output style: `educational` (flight-path isolation is subtle — documented in commit body).

---

### Phase 40 — MILESTONE 4/5 · SITL Integrated Binary 🧪 [TEST]
**Branch:** `feat/phase-40-sitl-integration` · **Model:** `claude-sonnet-4-6` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** `docker compose up` brings up the full MVC SITL stack — orbiter cFS cluster + ROS 2 rovers + Gazebo + `ground_station` + `fault_injector` — on a private docker network, running `SCN-NOM-01` end-to-end. Ground UI renders live HK from every asset class; fault-injection `0x541` toggles `time_suspect` flag and the flag propagates to the UI badge.
**Source of Truth.** [`00-system-of-systems.md`](docs/architecture/00-system-of-systems.md); [`dev/docker-runbook.md`](docs/dev/docker-runbook.md); `mission/verification/V&V-Plan.md`.
**Dependency Path.** Phases 31–39.
**Deliverables.**
- `compose.yaml` at repo root (or `docker/compose.yaml`) wiring all components.
- `scripts/sitl-smoke.sh` — scripted end-to-end: compose up → run SCN-NOM-01 → assert HK seen → compose down.
- Git tag `sitl-v1.0.0`.

**Verification (DoD — hard deliverable).**
- `bash scripts/sitl-smoke.sh` exits 0.
- `colcon test && ctest && cargo test --workspace` green.
- Manual UI screenshot (or headless capture) confirming live HK + time_suspect badge under 0x541 injection.

**AI Prompting Strategy.**
- Feed: Phase 30 tag as worked example; `00-system-of-systems.md`; docker-runbook.
- Agents: all four (`@architect`, `@researcher`, `@debugger`, `@code-reviewer`); `/security-review` full run.
- Output style: `concise`.

---

### Phase 41 — `apps/freertos_relay` 🟢 [FEAT]
**Branch:** `feat/phase-41-freertos-relay` · **Model:** `claude-sonnet-4-6` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Smallsat relay primary FSW on FreeRTOS. Star-topology cross-link forward per Q-C7; Proximity-1 down to surface assets; AOS up to orbiter.
**Source of Truth.** [`02-smallsat-relay.md`](docs/architecture/02-smallsat-relay.md); [`ICD-orbiter-relay.md`](docs/interfaces/ICD-orbiter-relay.md); [`ICD-relay-surface.md`](docs/interfaces/ICD-relay-surface.md); Q-C7; `SYS-REQ-0024`.
**Dependency Path.** Phase 35.
**Deliverables.**
- `apps/freertos_relay/` — FreeRTOS task graph (tx_task, rx_task, xlink_task, hk_task); Proximity-1 hailing (1 Hz during acquisition, 30 s LOS per Q-C5).
- Shared library `apps/freertos_common/` for FreeRTOS idioms reusable by Phase 42.

**Verification (DoD).**
- `ctest` green; `cppcheck` clean; coverage gate.
- SITL smoke: relay forwards a rover TM APID (0x300) to the orbiter, which forwards to ground.

**AI Prompting Strategy.**
- Feed: [`02-smallsat-relay.md`](docs/architecture/02-smallsat-relay.md); `ICD-orbiter-relay.md`, `ICD-relay-surface.md`.
- Agents: `@architect`; `@code-reviewer`.
- Output style: `educational`.

---

### Phase 42 — FreeRTOS MCU firmware (`mcu_payload` / `mcu_rwa` / `mcu_eps`) 🟢 [FEAT]
**Branch:** `feat/phase-42-freertos-mcu-fw` · **Model:** `claude-sonnet-4-6` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Three MCU firmwares pairing with Phase 35 gateways: SpW payload controller, CAN reaction-wheel controller, UART/HDLC EPS controller.
**Source of Truth.** [`03-subsystem-mcus.md`](docs/architecture/03-subsystem-mcus.md); [`ICD-mcu-cfs.md`](docs/interfaces/ICD-mcu-cfs.md); Q-H4.
**Dependency Path.** Phases 35, 41.
**Deliverables.**
- `apps/mcu_payload/`, `apps/mcu_rwa/`, `apps/mcu_eps/` — FreeRTOS task sets using `apps/freertos_common/`.
- Simulated buses: SpW / CAN / UART drivers over Unix domain sockets in SITL.

**Verification (DoD).**
- SITL: each MCU emits its TM block; gateway publishes on SB; orbiter HK aggregates.

**AI Prompting Strategy.**
- Feed: [`03-subsystem-mcus.md`](docs/architecture/03-subsystem-mcus.md).
- Agents: `@architect`; `@code-reviewer`.
- Output style: `concise`.

---

### Phase 43 — Time-authority ladder + `time_suspect` propagation 🟢 [FEAT]
**Branch:** `feat/phase-43-time-ladder` · **Model:** `claude-sonnet-4-6` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Implement the Ground→Orbiter→Relay→Rover→Cryobot CUC sync chain with drift budget 50 ms / 4 h (Q-F4). `time_suspect` flag set upstream propagates downstream automatically.
**Source of Truth.** [`08-timing-and-clocks.md`](docs/architecture/08-timing-and-clocks.md); Q-F4, Q-F6; `SYS-REQ-0030, 0031, 0032, 0033, 0034`.
**Dependency Path.** Phases 33, 41.
**Deliverables.**
- CUC sync packets in `packet-catalog.md`; encoder / decoder across all asset classes.
- Free-running-clock drift monitor that sets `time_suspect` when estimated drift exceeds per-asset threshold.

**Verification (DoD).**
- Drift analysis (A method per SYS-REQ-0032); integration test injects 60 ms drift over 4 h SITL accelerated time → `time_suspect` set → ground UI renders suspect badge.

**AI Prompting Strategy.**
- Feed: [`08-timing-and-clocks.md`](docs/architecture/08-timing-and-clocks.md); Q-F4; `SYS-REQ-0030–0034`.
- Agents: `@architect`; `@code-reviewer`.
- Output style: `educational`.

---

### Phase 44 — `Vault<T>` + `.critical_mem` radiation anchors 🔴 [CRIT]
**Branch:** `feat/phase-44-radiation-anchors` · **Model:** `claude-opus-4-7` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Ship the `Vault<T>` Rust wrapper ([`09 §5.2`](docs/architecture/09-failure-and-radiation.md)) for radiation-sensitive ground / relay state (CFDP transaction-id counters, `ApidRouter` filter table, future `tai_ns` mirror). Apply `__attribute__((section(".critical_mem")))` with MISRA 8.11 deviation comments to every radiation-sensitive static in every cFS app from Block 4.
**Source of Truth.** [`09-failure-and-radiation.md §5`](docs/architecture/09-failure-and-radiation.md); Q-F3; `SYS-REQ-0042, 0043`; [`.claude/rules/cfs-apps.md`](.claude/rules/cfs-apps.md) §11; [`.claude/rules/general.md`](.claude/rules/general.md) (Rust Vault<T> rule).
**Dependency Path.** Phases 31–35, 40, 41–42.
**Deliverables.**
- `rust/ground_station/src/vault.rs` (or a dedicated `rust/vault/` crate if reused by relay Rust tooling): `struct Vault<T>` with scrubber + ECC hook points.
- Every radiation-sensitive static across Block 4 / 5 tagged with `.critical_mem` attribute + inline MISRA 8.11 deviation comment.
- `[docs/standards/deviations.md]` updated with the MISRA 8.11 deviation record.

**Verification (DoD).**
- Grep audit: `rg 'critical_mem' apps/` matches at least one occurrence in every radiation-sensitive file listed in `09 §5.1`.
- Rust `Vault<T>` unit tests cover scrub-on-read, ECC-correct-single, ECC-flag-double.
- `/security-review` (ninth run — radiation anchors).

**AI Prompting Strategy.**
- Feed: [`09-failure-and-radiation.md`](docs/architecture/09-failure-and-radiation.md); Q-F3.
- Agents: `@architect`; `@code-reviewer`; `/security-review`.
- Output style: `educational`.

---

### Phase 45 — Scale-5 config surfaces 🟢 [FEAT]
**Branch:** `feat/phase-45-scale-5-config` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** 100 % ✅

**Objective.** Make the four Q-H2 config surfaces (Docker compose profiles + `_defs/mission.yaml` + cFS compile-time headers + ROS 2 launch files) carry the entire difference between MVC (1+1+1+1+1) and Scale-5 (5+1+5+3+2), with no per-instance code branches.
**Source of Truth.** [`10-scaling-and-config.md`](docs/architecture/10-scaling-and-config.md); Q-H2; `SYS-REQ-0002`.
**Dependency Path.** Phase 40.
**Deliverables.**
- `_defs/mission.yaml` — OWLT, per-class instance count, RAM caps, drift thresholds.
- `compose.profiles.yaml` — `mvc`, `scale-5` profiles.
- ROS 2 launch-file per-instance parameter expansion.
- Parser stub in `ground_station::config`.

**Verification (DoD).**
- `docker compose --profile scale-5 up` smoke-runs (asserting 5 orbiter containers come up, each with a distinct `instance_id`).
- `python3 scripts/traceability-lint.py` green.

**AI Prompting Strategy.**
- Feed: [`10-scaling-and-config.md`](docs/architecture/10-scaling-and-config.md); Q-H2.
- Agents: `@architect`; `@code-reviewer`.
- Output style: `concise`.

---

### Phase 46 — Safe-mode state machine 🟡 [LCC]
**Branch:** `feat/phase-46-safe-mode` · **Model:** `claude-sonnet-4-6` · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** Every asset class implements safe-mode entry on local invariant violation / watchdog timeout / operator `0x542`. Exit requires operator RESUME TC (no autonomous exit during Phase A/B per SYS-REQ-0051). Safe-mode entry events propagate to ground within `light-time + 2 × retransmit-window`.
**Source of Truth.** `SYS-REQ-0050, 0051, 0052`; [`09-failure-and-radiation.md §4`](docs/architecture/09-failure-and-radiation.md); `mission/conops/ConOps.md §7`.
**Dependency Path.** Phases 31–42.
**Deliverables.**
- Safe-mode hooks in every cFS app, FreeRTOS task, and ROS 2 lifecycle node.
- RESUME TC dispatcher in `orbiter_cdh` + `freertos_relay`.
- Integration test: inject watchdog timeout → safe-mode entry → ground UI event within SLA.

**Verification (DoD).**
- G/W/T per asset class.
- `/security-review` (tenth run — safe-mode integrity).

**AI Prompting Strategy.**
- Feed: [`09-failure-and-radiation.md §4`](docs/architecture/09-failure-and-radiation.md); `SYS-REQ-0050–0052`.
- Agents: `@architect`; `@code-reviewer`; `/security-review`.
- Output style: `educational`.

---

### Phase 47 — Security hardening gates 🟡 [LCC]
**Branch:** `feat/phase-47-security-gates` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** n/a (gate)

**Objective.** Lock security-posture CI gates: `cargo audit` zero HIGH/CRITICAL, `cppcheck` zero new findings, `scripts/traceability-lint.py` clean, pre-commit secret scanner (reusing [`.claude/hooks/protect-secrets.sh`](.claude/hooks/protect-secrets.sh)).
**Source of Truth.** [`.claude/rules/security.md`](.claude/rules/security.md); `SYS-REQ-0073, 0080`.
**Dependency Path.** Phases 18, 19, 30.
**Deliverables.**
- `scripts/security-gate.sh` aggregating all four checks.
- pre-commit hook wiring `protect-secrets.sh` + `grep-lints.sh`.

**Verification (DoD).**
- `bash scripts/security-gate.sh` exits 0.
- `/security-review` (eleventh run — comprehensive).

**AI Prompting Strategy.**
- Feed: [`.claude/rules/security.md`](.claude/rules/security.md); [`.claude/hooks/protect-secrets.sh`](.claude/hooks/protect-secrets.sh).
- Agents: `@code-reviewer`; `/security-review`.
- Output style: `concise`.

---

### Phase 48 — V&V catalog + traceability closure 🟡 [LCC]
**Branch:** `feat/phase-48-vv-catalog` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** n/a (gate)

**Objective.** Every `*-REQ-####` in every SRD has at least one T/A/I/D artefact cited in `traceability.md`. V&V catalog covers SCN-NOM-01, SCN-OFF-01 (time-suspect), SCN-OFF-02 (safe-mode), fault-injection demos.
**Source of Truth.** `mission/requirements/{SRD.md, FSW-SRD.md, GND-SRD.md, ROVER-SRD.md}`; `mission/requirements/traceability.md`; `mission/verification/V&V-Plan.md`.
**Dependency Path.** Phases 40–46.
**Deliverables.**
- Every new row needed in `traceability.md` lands.
- `V&V-Plan.md` updated with concrete test/demo pointers.

**Verification (DoD).**
- `python3 scripts/traceability-lint.py` exits 0.
- Counter-check: every SYS-REQ id grep-found in `traceability.md`.

**AI Prompting Strategy.**
- Feed: SRDs + traceability.md + V&V-Plan.md.
- Agents: `@researcher` (requirements sweep); `@code-reviewer`.
- Output style: `concise`.

---

### Phase 49 — GitHub Actions CI pipeline 🟡 [LCC]
**Branch:** `feat/phase-49-ci-pipeline` · **Model:** `claude-sonnet-4-6` · **Risk:** M · **Coverage:** n/a (gate)

**Objective.** Wire every gate built in Phases 10 / 18 / 19 / 30 / 47 / 48 into GitHub Actions. Every PR runs the full pipeline; merge blocked on any failure.
**Source of Truth.** [`dev/ci-workflow.md`](docs/dev/ci-workflow.md); [`CLAUDE.md`](CLAUDE.md).
**Dependency Path.** Phase 48.
**Deliverables.**
- `.github/workflows/{c-build.yml, rust-build.yml, ros2-build.yml, security.yml, traceability.yml}`.
- Matrix build: Ubuntu x86_64 (HPSC target deferred per Q-H8).

**Verification (DoD).**
- Open a canary PR — all five workflows pass.
- `gh pr checks <num>` shows all green.

**AI Prompting Strategy.**
- Feed: [`dev/ci-workflow.md`](docs/dev/ci-workflow.md); [`CLAUDE.md §Build & Test Commands`](CLAUDE.md).
- Agents: `@code-reviewer`.
- Output style: `concise`.

---

### Phase 50 — MILESTONE 5/5 · Mission Demonstrator v1.0 🧪 [TEST]
**Branch:** `feat/phase-50-mission-demo` · **Model:** `claude-sonnet-4-6` (with Opus final review) · **Risk:** H · **Coverage:** 100 % ✅

**Objective.** End-to-end MVC → Scale-5 SITL demonstrator. 5 orbiters + 1 relay + 5 land + 3 UAV + 2 cryobot. Every `SYS-REQ-####` passes its V&V artefact. Every Q-\* decision that was `pending` is now `resolved`. Clean `cargo audit`, clean `cppcheck`, clean `/security-review` on comms/crypto/buffer surfaces. Tagged release on `main`.
**Source of Truth.** `SRD.md`; `FSW-SRD.md`; `GND-SRD.md`; `ROVER-SRD.md`; `traceability.md`; `V&V-Plan.md`; [`06 §12`](docs/architecture/06-ground-segment-rust.md); [`decisions-log.md`](docs/standards/decisions-log.md); `SYS-REQ-0002` (Scale-5).
**Dependency Path.** Phases 41–49.
**Deliverables.**
- Git tag `sakura-ii-v1.0.0` on `main`.
- `CHANGELOG.md` at repo root.
- `docs/standards/decisions-log.md`: all pending Q-\*s appropriately flipped (Q-H8 remains `open` per project plan).
- Release notes covering every milestone 1–5 and V&V evidence.

**Verification (DoD — hard deliverable).**
- `docker compose --profile scale-5 up` runs SCN-NOM-01 end-to-end over ≥ 1 hour of accelerated SITL time; HK drops < 1 %; no unexpected safe-mode entries.
- `cargo test --workspace && cargo clippy --workspace -- -D warnings && cargo audit && ctest && colcon test && bash scripts/security-gate.sh && python3 scripts/traceability-lint.py` all exit 0.
- `cargo tarpaulin --workspace` reports ≥ 85 % per milestone crate; 100 % per new file (deviations justified in `deviations.md`).
- `@code-reviewer`, `@architect`, `/security-review` all green.

**AI Prompting Strategy.**
- Feed: every doc cited in §1.2; Phase 40 tag as worked example.
- Agents: all four (`@architect`, `@researcher`, `@debugger`, `@code-reviewer`); `/security-review` final run.
- Output style: `concise` (reference-document production, no exploration).

---

## 8. AI Prompting Strategy — Global Playbook

### 8.1 Model selection
| Badge | Primary model | Rationale |
|---|---|---|
| 🔵 ARCH | `claude-opus-4-7` | Scaffolding + workspace-wide decisions deserve high-deliberation model. |
| 🔴 CRIT | `claude-opus-4-7` | Security / invariant / radiation anchors are non-negotiable correctness. |
| 🟢 FEAT | `claude-sonnet-4-6` | Feature work inside a defined pattern; faster iteration. |
| 🟡 LCC | `claude-sonnet-4-6` | CI, deps, hardening gates — pattern-heavy; Sonnet sufficient. |
| 🧪 TEST | `claude-sonnet-4-6` + Opus review | Milestone gates need thoroughness + sign-off; Sonnet drives, Opus reviews. |

### 8.2 Agent matrix
| Agent | When | Typical phases |
|---|---|---|
| `@architect` | Any phase touching module boundaries, new trait signatures, workspace lints, or Q-\* resolution | 01, 02, 06, 07, 14, 15, 17, 21, 22, 26, 28, 30, 33, 35, 38, 39, 41, 44, 50 |
| `@researcher` | Requirements sweep, dep analysis, existing-code tracing | 13, 26, 48 |
| `@code-reviewer` | Post-green on every phase (blocking) | all 50 |
| `@debugger` | Test failures not resolved in first pass | ad-hoc (10, 30, 40, 50 likely) |

### 8.3 Skill matrix
| Skill | When | Typical phases |
|---|---|---|
| `/code-review` | Pre-PR, always | all 50 |
| `/create-pr` | PR open, always | all 50 |
| `/security-review` | Comms / crypto / buffer / radiation / safe-mode / FFI | 10, 14, 20, 25, 26, 28, 30, 33, 34, 39, 44, 46, 47, 50 |
| `/update-deps` | Phase 47 plus ad-hoc monthly | 47 + ongoing |
| `/fix-issue` | Any GitHub issue triage outside the 50-phase path | ad-hoc |

### 8.4 Rules feed (per phase badge)
| Badge | Rules feed |
|---|---|
| 🔵 ARCH | `.claude/rules/general.md`, `.claude/rules/security.md` |
| 🔴 CRIT | + `.claude/rules/cfs-apps.md` (for C phases) or `.claude/rules/ros2-nodes.md` (for C++ phases) |
| 🟢 FEAT | language-specific rules only |
| 🟡 LCC | `.claude/rules/security.md`, `.claude/rules/testing.md` |
| 🧪 TEST | full rules set + `06 §12` (Phase 30) or SCN-specific V&V |

### 8.5 Output style
| Phase kind | Style | Rationale |
|---|---|---|
| First instance of a new pattern (first newtype, first cFS app, first ROS 2 node class) | `educational` | Commits document the pattern for later reuse |
| Subsequent phases copying an established pattern | `concise` | Pattern already understood; minimize noise |
| Milestone gates | `concise` | Focus on gate pass/fail, not exposition |

---

## 9. Appendices

### Appendix A — Phase → SRD traceability (index)
Every phase card names ≥ 1 requirement ID. The full matrix lives in `docs/mission/requirements/traceability.md` (updated per phase). At Phase 48 closure, the following minimum coverage is asserted by `scripts/traceability-lint.py`:
- Every `SYS-REQ-####` in `SRD.md` is referenced by ≥ 1 phase.
- Every `FSW-REQ-####` in `FSW-SRD.md` is referenced by ≥ 1 phase in Block 4 or 5.
- Every `GND-REQ-####` in `GND-SRD.md` is referenced by ≥ 1 phase in Block 3 or 5.
- Every `ROV-REQ-####` in `ROVER-SRD.md` is referenced by ≥ 1 phase in Block 4 or 5.

### Appendix B — Phase → Q-\* index
| Q-\* | Phase(s) where resolved/anchored |
|---|---|
| Q-C2 (CFDP CRC-32) | 26 |
| Q-C3 (CfdpProvider) | 26, 30 (status flip) |
| Q-C4 (AOS 1024 B) | 22 |
| Q-C5 (Proximity-1 hailing) | 41 |
| Q-C6 (Secondary header 10 B) | 08 |
| Q-C7 (Star topology) | 41 |
| Q-C8 (BE locus) | 01 (lint), 10 (library), 14 (FFI), 30 (status flip) |
| Q-C9 (HDLC-lite tether) | 37 |
| Q-F1 (fault-inject transport) | 39 |
| Q-F2 (fault-inject APIDs) | 25, 39 |
| Q-F3 (critical_mem + Vault<T>) | 44 |
| Q-F4 (time authority + drift) | 43 |
| Q-F6 (fleet-sync precision) | 43 |
| Q-H1 (HPSC SMP Linux) | 45 (SITL host profile; HPSC cross-build deferred) |
| Q-H2 (four config surfaces) | 45 |
| Q-H4 (MCU bus families) | 35, 42 |
| Q-H8 (HPSC cross-build) | remains `open` after Phase 50 — tracked in `deviations.md` for Phase B+ follow-up |

### Appendix C — Milestone acceptance gate pointers
| Milestone | Gate spec |
|---|---|
| Phase 10 | §2.10 + §12 (proptest subset + KATs + grep) of [`06-ground-segment-rust.md`](docs/architecture/06-ground-segment-rust.md) |
| Phase 20 | Phase 20 card §Verification (cargo / ctest / cppcheck / coverage / grep) |
| Phase 30 | Full [`06 §12`](docs/architecture/06-ground-segment-rust.md) acceptance gate (13 checkboxes) — authoritative, not duplicated |
| Phase 40 | Phase 40 card §Verification (sitl-smoke.sh + colcon/ctest/cargo + UI capture) |
| Phase 50 | Phase 50 card §Verification (compose --profile scale-5 + full test sweep + every SRD requirement traced) |

### Appendix D — Glossary of guide-internal terms
Every term in this guide resolves against [`docs/GLOSSARY.md`](docs/GLOSSARY.md). If a term appears in this guide that is NOT in the glossary, the fix is to add it to `GLOSSARY.md` in the same PR; do not redefine terms locally. Expected net count of new guide-introduced terms: **0**.

---

## 10. How to use this guide

1. Find your next phase in the Master Matrix (§4.2) — the first row whose status is `[ ]`.
2. Open the detail card in §7. Read **Objective**, **Source of Truth**, **Dependency Path**, **Deliverables**, **Verification (DoD)**, **AI Prompting Strategy**.
3. Create the branch: `git checkout -b feat/phase-XX-<short-title>` from latest `main`.
4. Load the AI Prompting Strategy context: the listed docs, agents, skills, rules files, and output style.
5. Run the phase in Red-Green-Refactor discipline (§2.1).
6. At DoD satisfaction, run `@code-reviewer` then `/create-pr`. Request human review.
7. On merge, mark the matrix row status `[x]` via a follow-up docs commit on `main` and move to the next phase.

At phases 10 / 20 / 30 / 40 / 50, tag the release, publish the CHANGELOG entry, and pause before opening the next phase branch — the milestone is the checkpoint.

— *End of IMPLEMENTATION_GUIDE.md*
