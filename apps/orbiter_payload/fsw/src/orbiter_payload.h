#ifndef ORBITER_PAYLOAD_H
#define ORBITER_PAYLOAD_H

/*
 * orbiter_payload.h — Science payload manager for SAKURA-II orbiter.
 *
 * Controls payload power (on/off) and science mode selection on TC MID
 * ORBITER_PAYLOAD_CMD_MID (APID 0x185).  Science mode changes are rejected
 * while the payload bus is unpowered.  Aggregates payload TM from
 * mcu_payload_gw (APID 0x140–0x15F; stub zeros until Phase 35).
 *
 * Source of truth: docs/architecture/01-orbiter-cfs.md,
 *                  docs/interfaces/apid-registry.md (TM 0x140–0x15F, TC 0x185).
 * Compliance notes: [Q-F3] PowerState and ScienceMode pinned to .critical_mem;
 *                   [Q-F4] time_suspect seam reserved for Phase 43.
 */

#include "cfe.h"
#include "orbiter_payload_events.h"
#include "orbiter_payload_version.h"
#include "mids.h"

/* ── Command codes (TC MID: ORBITER_PAYLOAD_CMD_MID = 0x1985) ───────────── */
#define ORBITER_PAYLOAD_NOOP_CC             ((CFE_MSG_FcnCode_t)0U)
#define ORBITER_PAYLOAD_RESET_CC            ((CFE_MSG_FcnCode_t)1U)
#define ORBITER_PAYLOAD_SET_POWER_CC        ((CFE_MSG_FcnCode_t)2U)
#define ORBITER_PAYLOAD_SET_SCIENCE_MODE_CC ((CFE_MSG_FcnCode_t)3U)

/* ── Pipe depth ──────────────────────────────────────────────────────────── */
#define ORBITER_PAYLOAD_PIPE_DEPTH  ((uint16)20U)

/* ── Science mode constants ──────────────────────────────────────────────── */
#define ORBITER_PAYLOAD_MAX_SCIENCE_MODES  ((uint8)4U)

/* ── SET_POWER command payload ───────────────────────────────────────────── */
typedef struct
{
    CFE_MSG_Message_t Header;
    uint8             PowerOn; /* 0 = OFF, 1 = ON; other values invalid */
    uint8             Padding[3];
} ORBITER_PAYLOAD_SetPowerCmd_t;

/* ── SET_SCIENCE_MODE command payload ────────────────────────────────────── */
typedef struct
{
    CFE_MSG_Message_t Header;
    uint8             ScienceMode; /* 0–(ORBITER_PAYLOAD_MAX_SCIENCE_MODES-1) */
    uint8             Padding[3];
} ORBITER_PAYLOAD_SetScienceModeCmd_t;

/* ── Payload HK telemetry (APID 0x140, MID 0x0940) ──────────────────────── */
typedef struct
{
    CFE_MSG_Message_t Header;
    uint8             PowerState;   /* 0=OFF, 1=ON */
    uint8             ScienceMode;  /* active science mode index */
    uint32            CmdCounter;
    uint32            ErrCounter;
    /* Q-F4: time_suspect flag reserved; propagation lands in Phase 43. */
    uint8             TimeSuspect;
    uint8             Padding[1];
} ORBITER_PAYLOAD_HkTlm_t;

/* ── Application state ───────────────────────────────────────────────────── */
typedef struct
{
    uint32                 RunStatus;
    CFE_SB_PipeId_t        CmdPipe;
    uint32                 CmdCounter;
    uint32                 ErrCounter;
    ORBITER_PAYLOAD_HkTlm_t HkTlm;
} ORBITER_PAYLOAD_Data_t;

/* ── Entry point ─────────────────────────────────────────────────────────── */
void ORBITER_PAYLOAD_AppMain(void);

#endif /* ORBITER_PAYLOAD_H */
