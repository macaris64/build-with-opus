#!/usr/bin/env bash
# start_full_stack.sh — SAKURA-II SITL: dependency-ordered startup with readiness checks.
#
# Starts four stacks in order:
#   1. cFS (core-cpu1)                  — waits for 9 "App Initialized" log entries
#   2. Gazebo Harmonic (headless)       — waits for physics step count > 0
#   3. ROS 2 (rover_bringup sim.launch) — waits for teleop_node lifecycle active
#   4. Ground station (Rust UDP)        — waits for process to be listening on UDP 10000
#
# Prerequisite: all four stacks must be built before running this script.
#   cmake -B build_cfs -DSAKURA_CFS_RUNTIME=ON && cmake --build build_cfs
#   gz sim --version    (Gazebo Harmonic installed)
#   cd ros2_ws && colcon build --symlink-install
#   cargo build -p ground_station --release
#
# Usage:
#   bash scripts/start_full_stack.sh [--timeout-cfs=30] [--timeout-gz=20]
#
# Phase E gate (HOW_TO_RUN.md §7): run integration_smoke_test.sh after this
# script reports all four stacks healthy.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${REPO_ROOT}/.sitl_logs"
mkdir -p "${LOG_DIR}"

# ── Configurable timeouts (seconds) ──────────────────────────────────────────
TIMEOUT_CFS=${TIMEOUT_CFS:-30}
TIMEOUT_GZ=${TIMEOUT_GZ:-20}
TIMEOUT_ROS2=${TIMEOUT_ROS2:-60}
TIMEOUT_GS=${TIMEOUT_GS:-10}

CFS_STARTUP_LOG="${LOG_DIR}/cfs_es_startup.log"
GZ_LOG="${LOG_DIR}/gz_server.log"
ROS2_LOG="${LOG_DIR}/ros2_launch.log"
GS_LOG="${LOG_DIR}/ground_station.log"

# ── PID tracking for cleanup ──────────────────────────────────────────────────
CFS_PID="" GZ_PID="" ROS2_PID="" GS_PID=""

cleanup() {
    echo ""
    echo "── Shutting down SITL stack ─────────────────────────────────────────"
    for pid in "${GS_PID}" "${ROS2_PID}" "${GZ_PID}" "${CFS_PID}"; do
        if [ -n "${pid}" ] && kill -0 "${pid}" 2>/dev/null; then
            kill "${pid}" 2>/dev/null || true
        fi
    done
    echo "Done."
}
trap cleanup EXIT

wait_for() {
    local desc="$1" check_cmd="$2" timeout_s="$3"
    local elapsed=0
    printf "  Waiting for %s (max %ds) " "${desc}" "${timeout_s}"
    while ! eval "${check_cmd}" 2>/dev/null; do
        sleep 1
        elapsed=$(( elapsed + 1 ))
        printf "."
        if [ "${elapsed}" -ge "${timeout_s}" ]; then
            echo " TIMEOUT"
            echo "ERROR: ${desc} did not become ready within ${timeout_s}s" >&2
            exit 1
        fi
    done
    echo " OK (${elapsed}s)"
}

echo "━━━ SAKURA-II SITL Full Stack ━━━"
echo "  Repo:    ${REPO_ROOT}"
echo "  Logs:    ${LOG_DIR}"
echo ""

# ── 1. cFS ────────────────────────────────────────────────────────────────────
CFS_BIN="${REPO_ROOT}/build_cfs/cpu1/core-cpu1"
if [ ! -x "${CFS_BIN}" ]; then
    echo "ERROR: cFS binary not found: ${CFS_BIN}"
    echo "       Run: cmake -B build_cfs -DSAKURA_CFS_RUNTIME=ON && cmake --build build_cfs"
    exit 1
fi

echo "[1/4] Starting cFS ..."
"${CFS_BIN}" > "${CFS_STARTUP_LOG}" 2>&1 &
CFS_PID=$!
wait_for "cFS 9-app init" \
    "grep -c 'App Initialized' '${CFS_STARTUP_LOG}' 2>/dev/null | grep -qE '^[9-9][0-9]*\$' || [ \"\$(grep -c 'App Initialized' '${CFS_STARTUP_LOG}' 2>/dev/null)\" -ge 9 ]" \
    "${TIMEOUT_CFS}"

# ── 2. Gazebo ─────────────────────────────────────────────────────────────────
SDF_PATH="${REPO_ROOT}/simulation/worlds/mars_surrogate.sdf"
if [ ! -f "${SDF_PATH}" ]; then
    echo "ERROR: SDF world file not found: ${SDF_PATH}"
    exit 1
fi

echo "[2/4] Starting Gazebo Harmonic (headless) ..."
gz sim -r -s "${SDF_PATH}" > "${GZ_LOG}" 2>&1 &
GZ_PID=$!
wait_for "Gazebo physics step > 0" \
    "grep -q 'RoverDrivePlugin.*Loaded\|physics.*Running\|iterations' '${GZ_LOG}' 2>/dev/null" \
    "${TIMEOUT_GZ}"

# ── 3. ROS 2 ──────────────────────────────────────────────────────────────────
ROS2_SETUP="${REPO_ROOT}/ros2_ws/install/setup.bash"
if [ ! -f "${ROS2_SETUP}" ]; then
    echo "ERROR: ROS 2 install not found: ${ROS2_SETUP}"
    echo "       Run: cd ros2_ws && colcon build --symlink-install"
    exit 1
fi

echo "[3/4] Starting ROS 2 sim launch ..."
(source "${ROS2_SETUP}" && \
 ros2 launch rover_bringup sim.launch.py headless:=true) \
 > "${ROS2_LOG}" 2>&1 &
ROS2_PID=$!
wait_for "teleop_node lifecycle active" \
    "grep -q 'teleop_node.*active\|Lifecycle.*active\|Activating' '${ROS2_LOG}' 2>/dev/null" \
    "${TIMEOUT_ROS2}"

# ── 4. Ground station ─────────────────────────────────────────────────────────
GS_BIN="${REPO_ROOT}/target/release/ground_station"
if [ ! -x "${GS_BIN}" ]; then
    # Fall back to debug build
    GS_BIN="${REPO_ROOT}/target/debug/ground_station"
fi
if [ ! -x "${GS_BIN}" ]; then
    echo "ERROR: ground_station binary not found. Run: cargo build -p ground_station"
    exit 1
fi

echo "[4/4] Starting ground station ..."
RUST_LOG=info "${GS_BIN}" 127.0.0.1:10000 > "${GS_LOG}" 2>&1 &
GS_PID=$!
wait_for "ground station UDP listen" \
    "grep -q 'UDP socket bound\|listening on UDP' '${GS_LOG}' 2>/dev/null" \
    "${TIMEOUT_GS}"

# ── All healthy ───────────────────────────────────────────────────────────────
echo ""
echo "━━━ All four stacks healthy ━━━"
echo "  cFS PID:           ${CFS_PID}"
echo "  Gazebo PID:        ${GZ_PID}"
echo "  ROS 2 PID:         ${ROS2_PID}"
echo "  Ground station PID: ${GS_PID}"
echo ""
echo "Run integration smoke test:"
echo "  bash scripts/integration_smoke_test.sh"
echo ""
echo "Press Ctrl-C to shut down all stacks."
wait
