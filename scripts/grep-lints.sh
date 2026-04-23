#!/usr/bin/env bash
# CI grep-lint guards for the Rust workspace. Enforces Q-C8 (BE-only wire
# encoding) and no-unsafe in ccsds_wire. Must exit 0 before any PR that
# touches rust/ is merged.
#
# NOTE: The unwrap() guard is intentionally absent here. Ripgrep cannot
# distinguish inline #[cfg(test)] blocks from production code, so a text grep
# would false-positive on every run. The workspace clippy lint
# `unwrap_used = "deny"` (with test-module #[allow] overrides) is the
# authoritative enforcement; `cargo clippy -- -D warnings` covers this in CI.
set -euo pipefail

FAIL=0

# Q-C8: No little-endian byte conversions outside the designated locus.
# `ccsds_wire` and `cfs_bindings` are the only permitted BE↔LE conversion
# sites; all others are architectural violations.
if rg 'from_le_bytes|to_le_bytes' rust/ -g '!target' -q 2>/dev/null; then
    echo "ERROR: LE byte-order conversions found in rust/ (Q-C8 violation)" >&2
    FAIL=1
fi

# ccsds_wire must not contain unsafe — crate is #![forbid(unsafe_code)].
# Uses a word-boundary pattern to catch unsafe{, unsafe(, unsafe\t, etc.
if rg '\bunsafe\b' rust/ccsds_wire/ -g '!target' -q 2>/dev/null; then
    echo "ERROR: unsafe found in rust/ccsds_wire/ (crate forbids unsafe_code)" >&2
    FAIL=1
fi

# Phase 11: SAMPLE_MISSION is the retired template-era mission prefix. It is
# allowed only in historical references inside IMPLEMENTATION_GUIDE.md and
# docs/REPO_MAP.md. Any other hit is a regression.
if rg 'SAMPLE_MISSION' . \
    -g '!IMPLEMENTATION_GUIDE.md' \
    -g '!docs/REPO_MAP.md' \
    -g '!scripts/grep-lints.sh' \
    -g '!target' \
    -g '!build' \
    -g '!.git' \
    -q 2>/dev/null; then
    echo "ERROR: SAMPLE_MISSION reference found outside historical docs (Phase 11 regression)" >&2
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "All grep-lints passed."
fi
exit "$FAIL"
