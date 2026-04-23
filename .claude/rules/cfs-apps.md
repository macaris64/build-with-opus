---
paths:
  - "apps/**"
---

# cFS Application Development Rules

- `CFE_ES_RegisterApp()` must be the first CFE call made in `AppMain` — before any other service
- Apps communicate only via the Software Bus (`CFE_SB_Subscribe`, `CFE_SB_ReceiveBuffer`); direct function calls between apps are banned
- All Message IDs (MIDs) must be defined in `_defs/` as named macros; never use literal MID values in application source
- Use `CFE_EVS_SendEvent` for all status and error reporting — `printf`, `OS_printf`, and `fprintf` are banned in FSW
- Command dispatch must use a `switch` on the command code with an explicit `default:` branch that increments `ErrCounter`
- Apps must handle `CFE_ES_RUNSTATUS_APP_EXIT`: clean up software bus pipes and release any other resources before returning from `AppMain`
- No direct hardware register access; use OSAL (`OS_TaskCreate`, `OS_TimerCreate`, `OS_MutSemCreate`, etc.) exclusively
- Runtime-configurable data belongs in `CFE_TBL`-managed tables, not in compile-time static arrays
- Software bus pipe depth must be a named constant; never pass a literal integer to `CFE_SB_CreatePipe`
- Return error codes consistently: all initialization functions return `int32_t`; `CFE_SUCCESS` on success, a cFE status code on failure
- Radiation-sensitive static state (cFE TIME internals, mode register, `health` counters) must carry `__attribute__((section(".critical_mem")))` with a MISRA Rule 8.11 deviation comment; see `docs/architecture/09-failure-and-radiation.md §5.1` for the placement rules and `[Q-F3](../../docs/standards/decisions-log.md)` for the authoritative answer
