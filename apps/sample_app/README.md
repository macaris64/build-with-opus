# sample_app — Reference cFS Application

Reference implementation of the canonical SAKURA-II cFS application pattern.
Every real mission app is built on this skeleton; read this before writing a new app.

## Purpose

Demonstrates the minimum required structure for a cFS application:

- Register with Executive Services (`CFE_ES_RegisterApp`)
- Initialise Event Services (`CFE_EVS_Register`)
- Create a Software Bus pipe and subscribe to MIDs
- Enter the run loop: receive, dispatch, and report via EVS
- Exit cleanly via `CFE_ES_ExitApp`

Contains no mission logic — it is a living template and onboarding reference.

## Message IDs

| Direction | MID (hex) | APID | Purpose |
|---|---|---|---|
| TC in | `0x1980` | `0x180` | Ground commands |
| TM trigger | `0x0900` | `0x100` | HK request (SCH-driven) |

## Command Codes (TC MID `0x1980`)

| CC | Name | Payload | Effect |
|---|---|---|---|
| `0x00` | `SAMPLE_APP_NOOP_CC` | none | Increments `CmdCounter`; logs version via EVS |
| `0x01` | `SAMPLE_APP_RESET_CC` | none | Zeroes `CmdCounter` and `ErrCounter` |

Unknown command codes increment `ErrCounter` and emit `SAMPLE_APP_CMD_ERR_EID`.

## Events

| EID | Name | Type | Trigger |
|---|---|---|---|
| 1 | `SAMPLE_APP_STARTUP_INF_EID` | INFO | Successful initialisation |
| 2 | `SAMPLE_APP_CMD_ERR_EID` | ERROR | Unknown MID or command code; SB error |
| 3 | `SAMPLE_APP_CMD_NOOP_INF_EID` | INFO | NOOP command accepted |

## Application State

```c
typedef struct {
    uint32          RunStatus;   /* CFE_ES_RunStatus_APP_RUN / ERROR / EXIT */
    CFE_SB_PipeId_t CmdPipe;     /* handle for "SAMPLE_CMD_PIPE" */
    uint16          CmdCounter;  /* accepted commands since last RESET */
    uint16          ErrCounter;  /* rejected/errored commands */
} SAMPLE_APP_Data_t;
```

## Source Layout

```
sample_app/
├── CMakeLists.txt                   sakura_add_cfs_app(sample_app)
└── fsw/
    ├── src/
    │   ├── cfe.h                    standalone cFE stub (overridden by real cFE)
    │   ├── sample_app.h
    │   ├── sample_app.c
    │   ├── sample_app_events.h
    │   └── sample_app_version.h     v1.0.0
    └── unit-test/
        └── sample_app_test.c        CMocka; 100 % branch coverage
```

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure -R sample_app_unit_tests
```

## Writing a New App from This Template

1. Copy the directory: `cp -r apps/sample_app apps/my_new_app`
2. Rename all `SAMPLE_APP` / `sample_app` symbols to `MY_NEW_APP` / `my_new_app`
3. Register APIDs in `_defs/mids.h` and `docs/interfaces/apid-registry.md`
4. Add `add_subdirectory(apps/my_new_app)` to `CMakeLists.txt`
5. Add the app name to `MISSION_APPS` in `_defs/targets.cmake`
6. Implement your logic; keep all rules in `.claude/rules/cfs-apps.md`

## Compliance

- No dynamic allocation (`malloc`/`free` banned under `apps/`)
- No `printf`/`OS_printf` — runtime messages via `CFE_EVS_SendEvent` only
- MISRA C:2012 required rules; deviations documented inline
- Unit tests cover 100 % of branches in `sample_app.c`

## Source of Truth

- [`docs/architecture/01-orbiter-cfs.md`](../../docs/architecture/01-orbiter-cfs.md) — cFS app inventory and patterns
- [`.claude/rules/cfs-apps.md`](../../.claude/rules/cfs-apps.md) — cFS-specific coding rules
