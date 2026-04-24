#!/usr/bin/env bash
# MISRA C:2012 cppcheck baseline gate.
#
# Exits 0 when cppcheck is absent (graceful skip for dev machines) or when no
# new findings exist relative to cppcheck-baseline.txt.  Exits 1 on regressions.
#
# Bootstrap mode: if cppcheck-baseline.txt does not yet exist, the script
# writes it from the current tree and exits 0.  Commit the file afterward.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BASELINE="${REPO_ROOT}/cppcheck-baseline.txt"
APPS_DIR="${REPO_ROOT}/apps"

if ! command -v cppcheck &>/dev/null; then
    echo "[cppcheck-gate] cppcheck not found — skipping (install cppcheck to enforce MISRA baseline)"
    exit 0
fi

# cppcheck writes diagnostics to stderr; --error-exitcode=0 so we own the exit
# decision after diffing against the baseline.
CURRENT=$(cppcheck \
    --enable=all \
    --std=c17 \
    --error-exitcode=0 \
    --inline-suppr \
    --suppress=missingIncludeSystem \
    --suppress=missingInclude \
    "${APPS_DIR}" 2>&1 \
    | grep -E '^\[.*\].*\((error|warning|style|performance|portability)\)' \
    | sed "s|${REPO_ROOT}/||g" \
    | sort \
    || true)

if [[ ! -f "${BASELINE}" ]]; then
    printf '%s\n' "${CURRENT}" > "${BASELINE}"
    echo "[cppcheck-gate] Baseline created: ${BASELINE}"
    exit 0
fi

BASELINE_SORTED=$(sort "${BASELINE}")

# comm -13: lines present in CURRENT but absent from BASELINE → regressions
NEW_FINDINGS=$(comm -13 \
    <(echo "${BASELINE_SORTED}") \
    <(echo "${CURRENT}") \
    || true)

if [[ -n "${NEW_FINDINGS}" ]]; then
    echo "[cppcheck-gate] FAILED — new findings introduced:"
    echo "${NEW_FINDINGS}"
    exit 1
fi

echo "[cppcheck-gate] PASSED — no new findings vs baseline"
exit 0
