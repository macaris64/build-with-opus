# apps/ — cFS & FreeRTOS Flight Software

Flight-software applications. Each direct child is a self-contained app with its own `CMakeLists.txt`.

## Layout

```
apps/
├── sample_app/        — reference cFS app; kept as a living template
├── orbiter_*/         — (planned) cFS mission apps per orbiter subsystem
├── freertos_relay/    — (planned) FreeRTOS primary FSW for the smallsat relay
├── mcu_*/             — (planned) FreeRTOS firmware for subsystem MCUs
└── mcu_common/        — (planned) shared-library for MCU common tasks
```

Non-cFS FSW uses the `freertos_*` / `mcu_*` prefix so static-analysis rules can scope correctly. See [`../docs/REPO_MAP.md`](../docs/REPO_MAP.md) for the full naming convention.

## Build & Test

From the repo root:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure
cppcheck --enable=all --std=c17 apps/
```

## Where to read more

- **Architecture** — [`../docs/architecture/01-orbiter-cfs.md`](../docs/architecture/01-orbiter-cfs.md) (cFS app inventory, gateway apps, topology), [`../docs/architecture/02-smallsat-relay.md`](../docs/architecture/02-smallsat-relay.md) (FreeRTOS relay), [`../docs/architecture/03-subsystem-mcus.md`](../docs/architecture/03-subsystem-mcus.md) (MCU firmware).
- **Coding rules** — [`../.claude/rules/cfs-apps.md`](../.claude/rules/cfs-apps.md) (cFS-specific), [`../.claude/rules/general.md`](../.claude/rules/general.md) (C universal), [`../.claude/rules/security.md`](../.claude/rules/security.md) (MISRA / memory safety).
- **Tests** — [`../.claude/rules/testing.md`](../.claude/rules/testing.md). CMocka; 100 % branch coverage target under `apps/*/fsw/unit-test/`.
- **Compile-time config** — [`../_defs/mission_config.h`](../_defs/mission_config.h), [`../_defs/targets.cmake`](../_defs/targets.cmake).

## Hard rules (non-exhaustive — see `.claude/rules/`)

- No `malloc`/`calloc`/`realloc`/`free` under `apps/`.
- No `printf`/`OS_printf` — runtime messages go through `CFE_EVS_SendEvent` only.
- MISRA C:2012 required rules; deviations need inline justification.
- Every app registers at least one event in `<app>_events.h`.
- New app? Update [`../_defs/targets.cmake`](../_defs/targets.cmake) `MISSION_APPS` + [`../docs/interfaces/apid-registry.md`](../docs/interfaces/apid-registry.md).

Reference implementation: [`sample_app/`](sample_app/).
