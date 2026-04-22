---
# No paths field — loads every session alongside CLAUDE.md
---

# General Coding Conventions

## C / Flight Software
- Never use `malloc`, `calloc`, `realloc`, or `free` under `apps/` — no heap allocation in FSW
- Never use `printf` or `OS_printf` in FSW; all runtime messages go through `CFE_EVS_SendEvent`
- All C functions must have an explicit return type; non-void functions must return `int32_t` error codes
- MISRA C:2012 required rules apply; every deviation requires an inline justification comment:
  `/* MISRA C:2012 Rule X.Y deviation: <reason> */`
- No variable-length arrays (MISRA Rule 18.8); stack depth must be statically bounded at compile time
- Bounds-check all array indices before use; use `snprintf`/`strncpy` with explicit lengths, never `sprintf`/`strcpy`/`strcat`

## C++ / ROS 2 & Simulation
- Never use `std::cout` or `printf` in `ros2_ws/` — log via `RCLCPP_INFO`/`RCLCPP_ERROR` and `get_logger()` only
- Callbacks (subscription, timer, service) must not block — offload to a separate thread if work is non-trivial

## Rust
- All `unsafe` blocks require a `// SAFETY:` comment explaining the invariant being upheld
- Never use `unwrap()` on `Result` or `Option` outside of `#[cfg(test)]` — use `?` or explicit match
- `#![deny(clippy::all)]` at every crate root; suppression requires a justification comment

## All Languages
- Prefer early returns over deep nesting; max 3 levels of indentation
- All magic numbers and magic strings must become named constants (`#define` in C, `const` in Rust/C++)
- Comments explain the *why* (regulatory constraints, timing invariants, hardware errata), not the *what*
- No global mutable state in Rust; use `Arc<Mutex<T>>` or message-passing channels instead
