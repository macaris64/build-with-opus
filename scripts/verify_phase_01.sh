#!/usr/bin/env bash
# Phase 01 DoD verifier — runs the Given/When/Then gates from
# IMPLEMENTATION_GUIDE.md:270-273 and reports pass/fail. Exits 0 when
# Phase 01 is green.
set -euo pipefail

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "==> cargo fmt --all -- --check"
cargo fmt --all -- --check

echo "==> cargo build --workspace"
cargo build --workspace

echo "==> cargo clippy --workspace -- -D warnings"
cargo clippy --workspace -- -D warnings

echo "==> negative: cargo build -p _lint_probe MUST fail with unsafe_code + unused_imports"
probe_log="$(mktemp)"
trap 'rm -f "$probe_log"' EXIT
if cargo build --manifest-path rust/_lint_probe/Cargo.toml 2>"$probe_log"; then
    echo "FAIL: _lint_probe built successfully — workspace lint policy is not active." >&2
    cat "$probe_log" >&2
    exit 1
fi

# rustc prints lint names with hyphens in the `-D lint-name` note, so match
# either hyphen or underscore form as well as the human-readable error text.
grep -qE 'unsafe[-_]code|unsafe block'       "$probe_log" || { echo "FAIL: probe stderr missing 'unsafe_code'"    >&2; cat "$probe_log" >&2; exit 1; }
grep -qE 'unused[-_]imports|unused import'   "$probe_log" || { echo "FAIL: probe stderr missing 'unused_imports'" >&2; cat "$probe_log" >&2; exit 1; }

echo "OK: Phase 01 DoD gates all pass."
