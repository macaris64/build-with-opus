#!/usr/bin/env bash
# CI grep-lint guards for the Rust workspace. Enforces Q-C8 (BE-only wire
# encoding), no-unwrap in production code, and no-unsafe in ccsds_wire.
# Must exit 0 before any PR that touches rust/ is merged.
set -euo pipefail

FAIL=0

# Q-C8: No little-endian byte conversions outside the designated locus.
if rg 'from_le_bytes|to_le_bytes' rust/ -g '!target' -q 2>/dev/null; then
    echo "ERROR: LE byte-order conversions found in rust/ (Q-C8 violation)" >&2
    FAIL=1
fi

# No unwrap() in non-test production code.
if rg '\.unwrap\(\)' rust/ -g '!target' -g '!*/tests/*' -g '!*_test.rs' -q 2>/dev/null; then
    echo "ERROR: unwrap() found in non-test Rust code" >&2
    FAIL=1
fi

# ccsds_wire must not contain unsafe (crate is #![forbid(unsafe_code)]).
if rg 'unsafe ' rust/ccsds_wire/ -g '!target' -q 2>/dev/null; then
    echo "ERROR: unsafe found in ccsds_wire" >&2
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "All grep-lints passed."
fi
exit "$FAIL"
