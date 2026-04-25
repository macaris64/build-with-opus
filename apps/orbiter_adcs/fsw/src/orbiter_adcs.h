#ifndef ORBITER_ADCS_H
#define ORBITER_ADCS_H

/*
 * orbiter_adcs.h — Attitude Determination & Control application for SAKURA-II orbiter.
 *
 * ADCS publishes the on-board attitude quaternion estimate (APID 0x110) and
 * wheel telemetry (APID 0x111; stub zeros until mcu_rwa_gw lands in Phase 35).
 * Inbound TC MID: ORBITER_ADCS_CMD_MID (0x1982). Accepts a target quaternion
 * command; validates unit-norm within ORBITER_ADCS_QUAT_NORM_TOL before storing.
 *
 * Source of truth: docs/architecture/01-orbiter-cfs.md,
 *                  docs/interfaces/apid-registry.md (TM 0x110–0x11F, TC 0x182).
 * Compliance notes: [Q-F3] radiation anchor on CurrentQuat; [Q-F4] time_suspect
 *                   seam reserved for Phase 43.
 */

#include "cfe.h"
#include "orbiter_adcs_events.h"
#include "orbiter_adcs_version.h"
#include "mids.h"
#include <math.h>

/* ── Command codes (TC MID: ORBITER_ADCS_CMD_MID = 0x1982) ──────────────── */
#define ORBITER_ADCS_NOOP_CC              ((CFE_MSG_FcnCode_t)0U)
#define ORBITER_ADCS_RESET_CC             ((CFE_MSG_FcnCode_t)1U)
#define ORBITER_ADCS_SET_TARGET_QUAT_CC   ((CFE_MSG_FcnCode_t)2U)

/* ── Pipe depth ──────────────────────────────────────────────────────────── */
#define ORBITER_ADCS_PIPE_DEPTH  ((uint16)20U)

/* ── Quaternion unit-norm tolerance: |norm² - 1| must be ≤ this value ────── */
#define ORBITER_ADCS_QUAT_NORM_TOL  0.001f

/* ── Quaternion type (w, x, y, z — scalar-first convention) ─────────────── */
typedef struct
{
    float w;
    float x;
    float y;
    float z;
} ORBITER_ADCS_Quat_t;

/* ── SET_TARGET_QUAT command payload ─────────────────────────────────────── */
typedef struct
{
    CFE_MSG_Message_t   Header;   /* CCSDS primary header */
    ORBITER_ADCS_Quat_t Quat;     /* commanded target quaternion */
} ORBITER_ADCS_SetTargetQuatCmd_t;

/* ── Attitude HK telemetry (APID 0x110, MID 0x0910) ─────────────────────── */
typedef struct
{
    CFE_MSG_Message_t   Header;
    ORBITER_ADCS_Quat_t CurrentQuat;  /* on-board attitude estimate */
    ORBITER_ADCS_Quat_t TargetQuat;   /* last accepted target */
    uint32              CmdCounter;
    uint32              ErrCounter;
    /* Q-F4: time_suspect flag reserved; propagation lands in Phase 43. */
    uint8               TimeSuspect;
    uint8               Padding[3];   /* explicit padding (MISRA C:2012 Rule 6.7) */
} ORBITER_ADCS_HkTlm_t;

/* ── Wheel telemetry (APID 0x111, MID 0x0911) — stub zeros until Phase 35 ── */
#define ORBITER_ADCS_NUM_WHEELS  4U

typedef struct
{
    CFE_MSG_Message_t Header;
    float             WheelSpeedRpm[ORBITER_ADCS_NUM_WHEELS]; /* from mcu_rwa_gw (Phase 35) */
} ORBITER_ADCS_WheelTlm_t;

/* ── Application state ───────────────────────────────────────────────────── */
typedef struct
{
    uint32              RunStatus;
    CFE_SB_PipeId_t     CmdPipe;
    uint32              CmdCounter;
    uint32              ErrCounter;
    ORBITER_ADCS_HkTlm_t    HkTlm;
    ORBITER_ADCS_WheelTlm_t WheelTlm;
} ORBITER_ADCS_Data_t;

/* ── Entry point ─────────────────────────────────────────────────────────── */
void ORBITER_ADCS_AppMain(void);

#endif /* ORBITER_ADCS_H */
