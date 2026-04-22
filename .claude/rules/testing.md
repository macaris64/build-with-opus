---
paths:
  - "**/*_test.c"
  - "**/*_test.cpp"
  - "**/*_test.rs"
  - "tests/**"
  - "**/test/**"
  - "**/unit-test/**"
---

# Testing Conventions

## C / CMocka
- Unit tests use CMocka (`#include <cmocka.h>`); group test functions with `cmocka_unit_test` in a `CMUnitTest` array
- CFE and OSAL stubs are compiled with `#ifdef UNIT_TEST` guards — never link against the real cFE library in unit tests
- Stubs (`__wrap_` or `#ifdef UNIT_TEST` replacements) must be thin; no business logic inside stubs
- Stub at the OSAL/CFE boundary — do not intercept calls inside the app's own utility functions
- 100% branch coverage is the target; measure with `gcov` or `llvm-cov --show-branches`

## Rust
- Use `#[test]` in `#[cfg(test)]` modules for unit tests; integration tests go in `tests/` at crate root
- 100% branch coverage goal — measure with `cargo tarpaulin --engine llvm --out Html`
- Property tests with `proptest` for any parser or encoder that accepts unbounded input

## All Languages
- Every test file must contain at least one failure-path test: error return code, `Err(...)`, panic on invalid input, or non-happy-path branch
- Test names are descriptive: `test_init_returns_error_when_pipe_creation_fails` (snake_case, reads like a sentence)
- Never share mutable state across test cases; each test must be independent
- Do not reference implementation details in test names — test observable behavior, not internal function calls
- Assert on return values and side effects, not on which internal function was called
