#!/usr/bin/env bash
# sitl-smoke.sh — SAKURA-II Phase 40 DoD: SITL integrated binary smoke test.
#
# Wires: compose up → wait healthy → assert HK seen (APID 0x120 in GS log)
#        → assert time_suspect badge set (INGEST-FORBIDDEN-APID 0x541 in GS log
#          or /api/time time_suspect_seen==true after fault_injector fires)
#        → compose down
#
# SYS-REQ-0040, SYS-REQ-0041; Q-F1, Q-F2, Q-C8.
#
# Exit codes: 0 = all checks passed, 1 = any check failed.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COMPOSE_FILE="${REPO_ROOT}/compose.yaml"

TIMEOUT_HEALTHY=${TIMEOUT_HEALTHY:-120}
TIMEOUT_HK=${TIMEOUT_HK:-30}
TIMEOUT_BADGE=${TIMEOUT_BADGE:-30}

PASS=0
FAIL=0

check() {
    local desc="$1" code="$2"
    if [ "${code}" -eq 0 ]; then
        echo "  PASS: ${desc}"
        PASS=$(( PASS + 1 ))
    else
        echo "  FAIL: ${desc}"
        FAIL=$(( FAIL + 1 ))
    fi
}

cleanup() {
    echo ""
    echo "--- sitl-smoke: tearing down compose stack ---"
    docker compose -f "${COMPOSE_FILE}" down --timeout 10 2>/dev/null || true
}
trap cleanup EXIT

echo "━━━ SAKURA-II SITL Smoke Test ━━━"
echo "  Compose: ${COMPOSE_FILE}"
echo ""

# ── Step 1: build + bring up ──────────────────────────────────────────────────
echo "[1/5] docker compose up --build -d"
docker compose -f "${COMPOSE_FILE}" up --build -d

# ── Step 2: wait for ground_station healthy ───────────────────────────────────
echo "[2/5] Waiting for sakura_ground_station healthy (max ${TIMEOUT_HEALTHY}s) ..."
GS_STATUS="missing"
elapsed=0
while [ "${elapsed}" -lt "${TIMEOUT_HEALTHY}" ]; do
    GS_STATUS=$(docker inspect \
        --format='{{.State.Health.Status}}' \
        sakura_ground_station 2>/dev/null || echo "missing")
    if [ "${GS_STATUS}" = "healthy" ]; then break; fi
    sleep 2
    elapsed=$(( elapsed + 2 ))
    printf "  %ds / %ds (status=%s)\r" "${elapsed}" "${TIMEOUT_HEALTHY}" "${GS_STATUS}"
done
echo ""
GS_UP=1
[ "${GS_STATUS}" = "healthy" ] && GS_UP=0
check "sakura_ground_station healthy" "${GS_UP}"

if [ "${GS_UP}" -ne 0 ]; then
    echo "Diagnostics:"
    docker compose -f "${COMPOSE_FILE}" logs ground_station | tail -30
    exit 1
fi

# ── Step 3: poll for orbiter_comm HK (APID 0x120) in GS logs ─────────────────
echo "[3/5] Polling GS log for orbiter_comm HK APID 0x120 (max ${TIMEOUT_HK}s) ..."
HK_SEEN=1
elapsed=0
while [ "${elapsed}" -lt "${TIMEOUT_HK}" ]; do
    if docker logs sakura_ground_station 2>&1 | \
            grep -qE "APID[= ].*0x120|hk.*0x120|0x120.*hk" 2>/dev/null; then
        HK_SEEN=0
        break
    fi
    sleep 1
    elapsed=$(( elapsed + 1 ))
done
check "orbiter_comm HK (APID 0x120) routed to HK sink" "${HK_SEEN}"

# ── Step 4: assert time_suspect badge from APID 0x541 injection ───────────────
# fault_injector fires SCN-OFF-01-clockskew (APID 0x541) once all services are
# healthy. ApidRouter rejects it (Q-F2) and logs INGEST-FORBIDDEN-APID.
# The rejection also sets time_auth.time_suspect_seen = true, surfaced on
# GET /api/time as "time_suspect_seen":true.
echo "[4/5] Polling for time_suspect badge (max ${TIMEOUT_BADGE}s) ..."
BADGE_SEEN=1
elapsed=0
while [ "${elapsed}" -lt "${TIMEOUT_BADGE}" ]; do
    # Primary: log grep (works even without UI server)
    if docker logs sakura_ground_station 2>&1 | \
            grep -q "INGEST-FORBIDDEN-APID.*0x541" 2>/dev/null; then
        BADGE_SEEN=0
        break
    fi
    # Secondary: REST endpoint (requires UI server on :8080)
    if docker exec sakura_ground_station \
            sh -c 'curl -sf http://127.0.0.1:8080/api/time 2>/dev/null | grep -q "time_suspect_seen.*true"' \
            2>/dev/null; then
        BADGE_SEEN=0
        break
    fi
    sleep 1
    elapsed=$(( elapsed + 1 ))
done
check "time_suspect badge set (APID 0x541 clock-skew injection)" "${BADGE_SEEN}"

# ── Step 5: Q-F2 guard — no forbidden APID on HK sink ────────────────────────
# The HK sink must never receive fault-inject APIDs. Check GS log for any
# evidence that 0x540-0x543 reached a positive routing destination.
echo "[5/5] Q-F2 guard: no fault-inject APID on HK sink ..."
QF2_BREACH=0
if docker logs sakura_ground_station 2>&1 | \
        grep -qE "hk.*0x54[0-3]|0x54[0-3].*routed.*hk" 2>/dev/null; then
    QF2_BREACH=1
fi
check "Q-F2: fault-inject APIDs rejected on RF path (not in HK sink)" "${QF2_BREACH}"

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
echo "━━━ Results: ${PASS} passed, ${FAIL} failed ━━━"
if [ "${FAIL}" -gt 0 ]; then
    echo ""
    echo "Diagnostics:"
    docker compose -f "${COMPOSE_FILE}" logs --tail=40
    exit 1
fi
echo "PASS: sitl-smoke.sh exit 0"
# cleanup trap fires compose down
