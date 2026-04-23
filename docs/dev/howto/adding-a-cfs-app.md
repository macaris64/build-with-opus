# How-To: Add a New cFS Application

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Reference exemplar: [`apps/sample_app/`](../../../apps/sample_app/). Rules: [`../../../.claude/rules/cfs-apps.md`](../../../.claude/rules/cfs-apps.md), [`general.md`](../../../.claude/rules/general.md), [`security.md`](../../../.claude/rules/security.md). Architecture: [`../../architecture/01-orbiter-cfs.md`](../../architecture/01-orbiter-cfs.md). Packet bodies: [`../../interfaces/packet-catalog.md`](../../interfaces/packet-catalog.md). APID registry: [`../../interfaces/apid-registry.md`](../../interfaces/apid-registry.md).

This guide walks adding a new cFS application under [`apps/`](../../../apps/). It is a **signpost** — the rules live in `.claude/rules/`, the packet layouts live in `interfaces/`, and the architecture rationale lives in `01-orbiter-cfs.md`. This doc tells you what order to touch files.

## 1. When this applies

Follow this guide when you are adding a new **mission-authored app** ([01 §3.2](../../architecture/01-orbiter-cfs.md)) or **gateway app** ([01 §3.3](../../architecture/01-orbiter-cfs.md)) — `orbiter_cdh`, `mcu_rwa_gw`, etc. Do **not** use it for:

- cFS stock apps (CI/TO/SCH/HK/HS) — those are adopted, not authored. See [01 §3.1](../../architecture/01-orbiter-cfs.md).
- MCU firmware (FreeRTOS, not cFS) — see [`../../architecture/03-subsystem-mcus.md §6`](../../architecture/03-subsystem-mcus.md).

## 2. Prerequisites

- Quickstart green: `cmake --build build && ctest --test-dir build` runs clean ([`../quickstart.md §4`](../quickstart.md)).
- You've read [`apps/sample_app/fsw/src/sample_app.c`](../../../apps/sample_app/fsw/src/sample_app.c) — it's the canonical shape.
- You have a proposed APID in your head — if not, check [`apid-registry.md`](../../interfaces/apid-registry.md) for the next free slot in the right block.

## 3. Steps

### 3.1 Claim an APID block

Before writing any code, allocate APIDs in [`../../interfaces/apid-registry.md`](../../interfaces/apid-registry.md) (add the rows for TM / TC APIDs your app will own). Then add at least one PKT-ID row in [`../../interfaces/packet-catalog.md`](../../interfaces/packet-catalog.md) for the HK packet you'll emit. Packet catalog entry lands *first*; MID macro comes last — enforcement per [`packet-catalog §10`](../../interfaces/packet-catalog.md).

### 3.2 Create the app skeleton

Follow [01 §5](../../architecture/01-orbiter-cfs.md):

```
apps/<app_name>/
├── CMakeLists.txt
└── fsw/
    ├── src/
    │   ├── <app_name>.c
    │   ├── <app_name>.h
    │   ├── <app_name>_events.h
    │   └── <app_name>_version.h
    └── unit-test/
        └── <app_name>_test.c
```

Copy [`apps/sample_app/`](../../../apps/sample_app/) as a starting point. Delete what you don't need; keep the AppMain shape.

### 3.3 Wire cFE services

Per [`.claude/rules/cfs-apps.md`](../../../.claude/rules/cfs-apps.md):

- `CFE_ES_RegisterApp()` is the first call in `AppMain`.
- Set up a pipe via `CFE_SB_CreatePipe` with a **named constant** for pipe depth (never a literal).
- Subscribe via `CFE_SB_Subscribe` to MIDs defined in `_defs/` (or your app's `msgids.h`) — never literal MID values.
- All status messages via `CFE_EVS_SendEvent` — `printf` / `OS_printf` / `fprintf` are banned ([`.claude/rules/general.md`](../../../.claude/rules/general.md)).
- `AppMain` must handle `CFE_ES_RUNSTATUS_APP_EXIT` — clean up pipes before returning.

### 3.4 Command dispatch

Per [`.claude/rules/cfs-apps.md`](../../../.claude/rules/cfs-apps.md) and [01 §7.1](../../architecture/01-orbiter-cfs.md): single `switch` on secondary-header function code, explicit `default:` branch that increments an error counter and emits an "invalid command" event.

### 3.5 HK packet

Per [01 §7.3](../../architecture/01-orbiter-cfs.md): every app emits exactly one HK packet per assigned APID. HK is triggered by a `SCH` entry — **not** by the app's own timer — so the cadence is centrally auditable.

Packet body matches the row you added to [`packet-catalog.md`](../../interfaces/packet-catalog.md) in step 3.1. Multi-byte fields are big-endian per [Q-C8](../../standards/decisions-log.md).

### 3.6 `.critical_mem` for radiation-sensitive state

If your app holds state that would lose the mission on a bit flip (mode register, safe-mode counter, protected config) — annotate it per [`09-failure-and-radiation.md §5.1`](../../architecture/09-failure-and-radiation.md):

```c
/* MISRA C:2012 Rule 8.11 deviation: .critical_mem placement per Q-F3. */
static uint32_t g_safe_mode_counter __attribute__((section(".critical_mem")));
```

Rule of thumb: mode, time, counters that gate safing → `.critical_mem`. Transient HK buffers → default linkage.

### 3.7 CMake wiring

Add the app to `_defs/targets.cmake`'s `MISSION_APPS` list, and an `add_subdirectory(apps/<app_name>)` if not already picked up globally. Check `cmake --build build` runs clean with `-Wall -Wextra -Werror -pedantic` — these flags are non-negotiable per [CLAUDE.md §Coding Standards](../../../CLAUDE.md).

### 3.8 Unit tests

Per [`.claude/rules/testing.md`](../../../.claude/rules/testing.md): CMocka, target 100 % branch coverage, stub at CFE/OSAL boundary under `#ifdef UNIT_TEST`. At minimum one failure-path test.

### 3.9 Static analysis

Before commit:

```bash
cppcheck --enable=all --std=c17 apps/<app_name>/
```

Zero new findings per [`.claude/rules/security.md`](../../../.claude/rules/security.md). MISRA deviations need inline justification comments.

### 3.10 Run the skill pass

Before requesting review, run [`/code-review`](../../../.claude/skills/) and [`/security-review`](../../../.claude/skills/) if your app touches comms, crypto, or data buffers. These are automation-backed checks; skipping them shifts burden to the human reviewer.

## 4. Checklist before PR

- [ ] APID row in [`apid-registry.md`](../../interfaces/apid-registry.md)
- [ ] PKT-ID row in [`packet-catalog.md`](../../interfaces/packet-catalog.md)
- [ ] App directory follows [01 §5](../../architecture/01-orbiter-cfs.md) shape
- [ ] At least one event in `<app>_events.h` per [CLAUDE.md](../../../CLAUDE.md)
- [ ] `CMakeLists.txt` + `_defs/targets.cmake` updated
- [ ] Unit test exists with at least one failure-path case
- [ ] `cppcheck` clean
- [ ] `/code-review` run

## 5. What this guide does NOT cover

- Inter-app protocols — those are ICDs under [`../../interfaces/`](../../interfaces/).
- Configuration-table design — see [`CFE_TBL` docs](../../standards/references.md) and [01 §10](../../architecture/01-orbiter-cfs.md).
- Scaling considerations — see [`../../architecture/10-scaling-and-config.md`](../../architecture/10-scaling-and-config.md).
- HPSC cross-build — deferred per [Q-H8](../../standards/decisions-log.md).
