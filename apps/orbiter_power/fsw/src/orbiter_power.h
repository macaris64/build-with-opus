#ifndef ORBITER_POWER_H
#define ORBITER_POWER_H

/*
 * orbiter_power.h — Electrical Power System arbiter for SAKURA-II orbiter.
 *
 * Aggregates EPS telemetry from mcu_eps_gw (APID 0x130–0x13F; stub zeros until
 * Phase 35) and enforces safety-interlocked load switching on TC MID
 * ORBITER_POWER_CMD_MID (APID 0x184).  Switching is rejected when the interlock
 * table row for the requested load marks the current power mode as prohibited.
 *
 * Source of truth: docs/architecture/01-orbiter-cfs.md,
 *                  docs/interfaces/apid-registry.md (TM 0x130–0x13F, TC 0x184).
 * Compliance notes: [Q-F3] CurrentMode and LoadState pinned to .critical_mem;
 *                   [Q-F4] time_suspect seam reserved for Phase 43.
 */

#include "cfe.h"
#include "orbiter_power_events.h"
#include "orbiter_power_version.h"
#include "mids.h"

/* ── Command codes (TC MID: ORBITER_POWER_CMD_MID = 0x1984) ─────────────── */
#define ORBITER_POWER_NOOP_CC              ((CFE_MSG_FcnCode_t)0U)
#define ORBITER_POWER_RESET_CC             ((CFE_MSG_FcnCode_t)1U)
#define ORBITER_POWER_LOAD_SWITCH_CC       ((CFE_MSG_FcnCode_t)2U)
#define ORBITER_POWER_SET_POWER_MODE_CC    ((CFE_MSG_FcnCode_t)3U)

/* ── Pipe depth ──────────────────────────────────────────────────────────── */
#define ORBITER_POWER_PIPE_DEPTH  ((uint16)20U)

/* ── Power mode identifiers ──────────────────────────────────────────────── */
#define ORBITER_POWER_MODE_NORMAL  ((uint8)0U)
#define ORBITER_POWER_MODE_SAFE    ((uint8)1U)
#define ORBITER_POWER_MODE_ECLIP   ((uint8)2U)
#define ORBITER_POWER_MODE_COUNT   ((uint8)3U)

/* ── Load switch constants ───────────────────────────────────────────────── */
#define ORBITER_POWER_MAX_LOADS    ((uint8)4U)
#define ORBITER_POWER_LOAD_OFF     ((uint8)0U)
#define ORBITER_POWER_LOAD_ON      ((uint8)1U)

/* ── Safety interlock table entry ───────────────────────────────────────── */
/*
 * ProhibitMask is a bitmask over power mode IDs:
 *   bit N is set  → switching load is prohibited when CurrentMode == N
 *   bit N is clear → switching allowed in mode N
 */
typedef struct
{
    uint8 LoadId;
    uint8 ProhibitMask;
    uint8 Padding[2]; /* explicit padding (MISRA C:2012 Rule 6.7) */
} ORBITER_POWER_InterlockRow_t;

/* ── LOAD_SWITCH command payload ─────────────────────────────────────────── */
typedef struct
{
    CFE_MSG_Message_t Header;
    uint8             LoadId;
    uint8             Action; /* ORBITER_POWER_LOAD_OFF or ORBITER_POWER_LOAD_ON */
    uint8             Padding[2];
} ORBITER_POWER_LoadSwitchCmd_t;

/* ── SET_POWER_MODE command payload ──────────────────────────────────────── */
typedef struct
{
    CFE_MSG_Message_t Header;
    uint8             ModeId;
    uint8             Padding[3];
} ORBITER_POWER_SetPowerModeCmd_t;

/* ── EPS HK telemetry (APID 0x130, MID 0x0930) ──────────────────────────── */
typedef struct
{
    CFE_MSG_Message_t Header;
    uint8             CurrentMode;
    uint8             LoadState[ORBITER_POWER_MAX_LOADS]; /* per-load ON/OFF state */
    uint32            CmdCounter;
    uint32            ErrCounter;
    /* Q-F4: time_suspect flag reserved; propagation lands in Phase 43. */
    uint8             TimeSuspect;
    uint8             Padding[3];
} ORBITER_POWER_HkTlm_t;

/* ── Application state ───────────────────────────────────────────────────── */
typedef struct
{
    uint32              RunStatus;
    CFE_SB_PipeId_t     CmdPipe;
    uint32              CmdCounter;
    uint32              ErrCounter;
    ORBITER_POWER_HkTlm_t HkTlm;
} ORBITER_POWER_Data_t;

/* ── Entry point ─────────────────────────────────────────────────────────── */
void ORBITER_POWER_AppMain(void);

#endif /* ORBITER_POWER_H */
