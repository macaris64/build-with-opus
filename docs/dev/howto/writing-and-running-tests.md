# How-To: Write and Run Tests

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Rules: [`../../../.claude/rules/testing.md`](../../../.claude/rules/testing.md), [`general.md`](../../../.claude/rules/general.md), [`security.md`](../../../.claude/rules/security.md). V&V plan: [`../../mission/verification/V&V-Plan.md`](../../mission/verification/V&V-Plan.md). Reference exemplars: [`apps/sample_app/fsw/unit-test/`](../../../apps/sample_app/fsw/unit-test/), [`ros2_ws/src/rover_teleop/test/`](../../../ros2_ws/src/rover_teleop/test/).

This is a cross-stack signpost for the testing disciplines SAKURA-II expects. Commands live in [`CLAUDE.md`](../../../CLAUDE.md); rules live in [`.claude/rules/testing.md`](../../../.claude/rules/testing.md); gate policy lives in [`V&V-Plan §5`](../../mission/verification/V&V-Plan.md). This guide tells you what to author and in what order.

## 1. When this applies

- Adding unit tests to a new or existing cFS app, ROS 2 node, Rust crate, or Gazebo plugin.
- Running the PR-gate baseline locally.
- Debugging a flake or coverage regression.

## 2. Universal rules

Per [`.claude/rules/testing.md`](../../../.claude/rules/testing.md):

- Every test file contains at least one **failure-path** test (error return, `Err(...)`, panic on invalid input, non-happy-path branch).
- Test names read like sentences: `test_init_returns_error_when_pipe_creation_fails`.
- No shared mutable state across test cases; each test is independent.
- Assert on return values and side effects, **not** on which internal function was called.
- Don't reference implementation details in test names.

## 3. cFS / C (CMocka)

Exemplar: [`apps/sample_app/fsw/unit-test/sample_app_test.c`](../../../apps/sample_app/fsw/unit-test/sample_app_test.c).

### 3.1 Structure

```c
#include <cmocka.h>

static void test_init_returns_error_when_pipe_creation_fails(void ** /*state*/) {
    // arrange: stub CFE_SB_CreatePipe to return failure
    // act: call app init
    // assert: return value == expected cFE error code
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_returns_error_when_pipe_creation_fails),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

### 3.2 Stubbing

Per [`.claude/rules/testing.md`](../../../.claude/rules/testing.md):

- CFE and OSAL stubs compile under `#ifdef UNIT_TEST` — never link against real cFE in unit tests.
- Stubs are thin — no business logic inside a stub.
- Stub at the CFE/OSAL boundary, not inside the app's own functions.

### 3.3 Running

```bash
ctest --test-dir build --output-on-failure
ctest --test-dir build -R sample_app        # single app
```

Target **100 % branch coverage** per [CLAUDE.md §Coding Standards](../../../CLAUDE.md). Measure with `gcov` or `llvm-cov --show-branches`.

### 3.4 Troubleshooting

No tests found: [`../troubleshooting.md §1.2`](../troubleshooting.md). Link errors against real cFE: [`../troubleshooting.md §1.4`](../troubleshooting.md).

## 4. ROS 2 / C++ (gtest)

Exemplar: [`ros2_ws/src/rover_teleop/test/test_teleop.cpp`](../../../ros2_ws/src/rover_teleop/test/test_teleop.cpp).

### 4.1 Structure

Use gtest conventions. Put test files under `<package>/test/` and wire them via `ament_add_gtest` in `CMakeLists.txt`.

### 4.2 Launch tests

For multi-node integration tests, use `launch_testing` + pytest. Drives the launch description from §4.1 with assertions on observed behaviour.

### 4.3 Running

```bash
( cd ros2_ws && colcon test --packages-select <package> )
( cd ros2_ws && colcon test-result --verbose )
```

Coverage target is deferred to Phase C per [V&V-Plan §5.1](../../mission/verification/V&V-Plan.md); aim for unit-test-per-class as a minimum.

## 5. Rust

Exemplars: in-module `#[cfg(test)]` blocks across [`rust/ground_station/src/`](../../../rust/ground_station/), [`rust/cfs_bindings/src/`](../../../rust/cfs_bindings/).

### 5.1 Structure

In-module unit tests:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_returns_error_on_empty_input() {
        assert!(parse(b"").is_err());
    }
}
```

Integration tests under `tests/` at crate root — one file per scenario.

### 5.2 Property tests

Per [`.claude/rules/testing.md`](../../../.claude/rules/testing.md): parsers and encoders that accept unbounded input use `proptest`. Example:

```rust
proptest! {
    #[test]
    fn encode_decode_roundtrips(input in any::<Vec<u8>>()) {
        let encoded = encode(&input);
        let decoded = decode(&encoded).unwrap();
        prop_assert_eq!(decoded, input);
    }
}
```

### 5.3 Running

```bash
cargo test
cargo test -p <crate>
cargo tarpaulin --out Html       # coverage; Linux x86_64 only
```

Target **100 % branch coverage** per [`.claude/rules/testing.md`](../../../.claude/rules/testing.md).

### 5.4 `unwrap` in tests

`unwrap()` is permitted under `#[cfg(test)]` per [`.claude/rules/general.md`](../../../.claude/rules/general.md) — tests are allowed to panic on an invariant being wrong.

## 6. Gazebo plugin tests

Per [05 §7](../../architecture/05-simulation-gazebo.md): minimum gate is a "loads without crash" test exercising `Load()` + `Reset()` + 10 `OnUpdate()` iterations with a synthetic model pointer.

## 7. Scenario-driven validation

For mission-level validation (not unit / integration), author scenarios under `simulation/scenarios/*.yaml` per [V&V-Plan §8](../../mission/verification/V&V-Plan.md). Each scenario:

- Names the ConOps scenario it validates.
- Enumerates fault injections via APID `0x540`–`0x543` only.
- Declares expected observable outcomes.
- Declares a bounded duration.

Replayed via `fault_injector` per [05 §4](../../architecture/05-simulation-gazebo.md) — implementation tracked as an open item.

## 8. PR-gate baseline

Before PR, per [CLAUDE.md §PR & Review Process](../../../CLAUDE.md):

```bash
cmake --build build && ctest --test-dir build --output-on-failure
cargo test && cargo clippy -- -D warnings
( cd ros2_ws && colcon build --symlink-install && colcon test )
```

All four green. See [`../build-runbook.md`](../build-runbook.md) for iteration-optimised variants.

## 9. What this guide does NOT cover

- CI configuration — tracked as a Phase-C open item per [V&V-Plan §7](../../mission/verification/V&V-Plan.md).
- Requirement-ID traceability — Phase C artefact under `mission/requirements/traceability.md`.
- Benchmark harnesses / performance regression — separate discipline, out of scope for this guide.
