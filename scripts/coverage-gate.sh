#!/bin/sh
# coverage-gate.sh — Branch-coverage gate for SAKURA-II cFS apps.
#
# Builds each app's _test target with gcov instrumentation, runs the test
# binary, then verifies that gcov reports 100% branch coverage on the app
# source file. Exits 0 on pass, 1 on any failure.
#
# Usage:
#   bash scripts/coverage-gate.sh                   # defaults to sample_app
#   bash scripts/coverage-gate.sh sample_app foo_app
#
# Requirements:
#   cmake, gcc, libcmocka-dev, gcov must be on PATH.
#   Run from repo root or any directory (script resolves REPO_ROOT).
#
# POSIX sh compatible — does not use bash extensions ([[ ]], pipefail,
# process substitution, ${BASH_SOURCE}).

set -e

# ── Resolve repository root ───────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_COV="${REPO_ROOT}/build_cov"

# ── Progress helpers (stderr keeps stdout clean for CI parsers) ───────────────
log()  { printf '[coverage-gate] %s\n' "$1" >&2; }
fail() { printf '[coverage-gate] FAIL: %s\n' "$1" >&2; exit 1; }

# ── Determine apps to check ───────────────────────────────────────────────────
if [ "$#" -eq 0 ]; then
    APP_LIST="sample_app"
else
    APP_LIST="$*"
fi

# ── Prerequisite checks ───────────────────────────────────────────────────────
for tool in cmake gcov; do
    if ! command -v "${tool}" > /dev/null 2>&1; then
        fail "${tool} not found on PATH — install it to run the coverage gate"
    fi
done

# ── Configure (once for all apps) ─────────────────────────────────────────────
log "Configuring ${BUILD_COV} with SAKURA_COVERAGE=ON"
cmake -B "${BUILD_COV}" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DSAKURA_COVERAGE=ON \
      -S "${REPO_ROOT}" \
      > /dev/null

# ── Per-app pass/fail tracking ────────────────────────────────────────────────
OVERALL_PASS=0

for app in ${APP_LIST}; do
    log "--- ${app} ---"

    # Build the _test target
    log "Building ${app}_test"
    cmake --build "${BUILD_COV}" --target "${app}_test" > /dev/null

    # Locate the test binary (Makefile + Ninja generators both place it here)
    TEST_BIN="${BUILD_COV}/apps/${app}/${app}_test"
    if [ ! -x "${TEST_BIN}" ]; then
        log "FAIL: test binary not found at ${TEST_BIN}"
        OVERALL_PASS=1
        continue
    fi

    # Run the test binary to produce .gcda files
    log "Running ${TEST_BIN}"
    if ! "${TEST_BIN}" > /dev/null; then
        log "FAIL: ${app}_test exited non-zero — fix failing tests before checking coverage"
        OVERALL_PASS=1
        continue
    fi

    # Locate the .gcno file produced under CMakeFiles/<app>_test.dir/
    # CMake names the object file after the full source filename including its
    # extension (e.g. sample_app.c.gcno), so search by that exact pattern.
    OBJ_DIR="${BUILD_COV}/apps/${app}/CMakeFiles/${app}_test.dir"
    GCNO_FILE="$(find "${OBJ_DIR}" -path "*/fsw/src/${app}.c.gcno" 2>/dev/null | head -1)"

    if [ -z "${GCNO_FILE}" ]; then
        log "FAIL: no ${app}.c.gcno found under ${OBJ_DIR}"
        log "  Check that -fprofile-arcs was applied to ${app}_test"
        OVERALL_PASS=1
        continue
    fi

    # Run gcov with branch statistics.
    # -b: include branch taken/not-taken counts in the summary
    # -n: suppress .gcov annotation file creation (summary only)
    # Passing the .gcno file directly avoids the --object-directory naming
    # mismatch that occurs when CMake appends ".c" to the object file stem.
    log "Running gcov -b on ${app}.c"
    GCOV_OUT="$(gcov -b -n "${GCNO_FILE}" 2>/dev/null)"

    printf '%s\n' "${GCOV_OUT}" | while IFS= read -r line; do
        log "  ${line}"
    done

    # Parse "Taken at least once" percentage.
    # gcov -b emits a line of the form:
    #   Taken at least once:100.00% of 42
    TAKEN_LINE="$(printf '%s\n' "${GCOV_OUT}" | grep 'Taken at least once:' || true)"

    if [ -z "${TAKEN_LINE}" ]; then
        log "FAIL: gcov produced no 'Taken at least once' line for ${app}.c"
        log "  Possible cause: source has no conditional branches, or gcov version mismatch"
        OVERALL_PASS=1
        continue
    fi

    # Extract the percentage token ("XX.XX%") after the colon.
    BRANCH_PCT="$(printf '%s\n' "${TAKEN_LINE}" | awk -F':' '{print $2}' | awk '{print $1}')"

    if [ "${BRANCH_PCT}" = "100.00%" ]; then
        log "PASS: ${app} branch coverage 100.00%"
    else
        log "FAIL: ${app} branch coverage is ${BRANCH_PCT} (require 100.00%)"
        log "  Run: gcov -b ${GCNO_FILE}"
        log "  to see which branches were not taken."
        OVERALL_PASS=1
    fi
done

# ── Final result ──────────────────────────────────────────────────────────────
if [ "${OVERALL_PASS}" -eq 0 ]; then
    log "All apps passed the coverage gate."
    exit 0
else
    log "One or more apps failed the coverage gate."
    exit 1
fi
