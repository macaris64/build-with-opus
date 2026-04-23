#!/usr/bin/env bash
# Phase 01 Definition-of-Done verifier.
#
# Re-runs the four Given/When/Then gates from
# IMPLEMENTATION_GUIDE.md:270-273 locally and in CI. The first three are
# positive gates; the fourth is a NEGATIVE gate against rust/_lint_probe/
# (a nested-workspace fixture whose build MUST fail once the Phase 01 lint
# block is active). Exits 0 iff every gate passes.
#
# Usage: bash scripts/verify_phase_01.sh
set -euo pipefail

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

PROBE_MANIFEST="$REPO_ROOT/rust/_lint_probe/Cargo.toml"

stage() { printf '\n==> %s\n' "$1"; }
fail()  { printf 'FAIL: %s\n' "$1" >&2; [[ $# -ge 2 ]] && cat "$2" >&2; exit 1; }

[[ -f "$PROBE_MANIFEST" ]] || fail "_lint_probe fixture is missing at $PROBE_MANIFEST"

stage "1/4  cargo fmt --all -- --check"
cargo fmt --all -- --check

stage "2/4  cargo build --workspace"
cargo build --workspace

stage "3/4  cargo clippy --workspace -- -D warnings"
cargo clippy --workspace -- -D warnings

stage "4/4  negative: rust/_lint_probe MUST fail (unsafe_code, unused_imports, panic, unwrap_used)"
probe_log="$(mktemp)"
trap 'rm -f "$probe_log"' EXIT
if cargo clippy --manifest-path "$PROBE_MANIFEST" -- -D warnings 2>"$probe_log"; then
    fail "_lint_probe built successfully — workspace lint policy is not active." "$probe_log"
fi
# rustc/clippy print lint names with hyphens in the `-D lint-name` note, so
# accept either hyphen or underscore form as well as the human-readable text.
grep -qE 'unsafe[-_]code|usage of an .unsafe. block'             "$probe_log" || fail "probe stderr missing 'unsafe_code'"  "$probe_log"
grep -qE 'unused[-_]imports|unused import'                       "$probe_log" || fail "probe stderr missing 'unused_imports'" "$probe_log"
grep -qE 'clippy::panic|`panic` should not be present'           "$probe_log" || fail "probe stderr missing 'clippy::panic'"  "$probe_log"
grep -qE 'clippy::unwrap[-_]used|used .unwrap\(\). on'           "$probe_log" || fail "probe stderr missing 'clippy::unwrap_used'" "$probe_log"

printf '\nOK: Phase 01 DoD gates (4/4) all pass.\n'
