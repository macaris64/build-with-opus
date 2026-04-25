#ifndef ORBITER_CDH_H
#define ORBITER_CDH_H

/*
 * orbiter_cdh.h — Command & Data Handling application for SAKURA-II orbiter.
 *
 * CDH aggregates housekeeping from all peer orbiter apps, dispatches mode
 * transitions, and applies EVS filter commands. TC MID: ORBITER_CDH_CMD_MID
 * (0x1981). Combined HK TM published on ORBITER_CDH_HK_MID (0x0901).
 *
 * Source of truth: docs/architecture/01-orbiter-cfs.md,
 *                  docs/interfaces/apid-registry.md.
 */

#include "cfe.h"
#include "orbiter_cdh_events.h"
#include "orbiter_cdh_version.h"
#include "mids.h"

/* ── Command codes (TC MID: ORBITER_CDH_CMD_MID = 0x1981) ───────────────── */
#define ORBITER_CDH_NOOP_CC              ((CFE_MSG_FcnCode_t)0U)
#define ORBITER_CDH_RESET_CC             ((CFE_MSG_FcnCode_t)1U)
#define ORBITER_CDH_MODE_TRANSITION_CC   ((CFE_MSG_FcnCode_t)2U)
#define ORBITER_CDH_EVS_FILTER_CC        ((CFE_MSG_FcnCode_t)3U)

/* ── Orbiter mode values ─────────────────────────────────────────────────── */
#define ORBITER_CDH_MODE_SAFE       ((uint8)0U)
#define ORBITER_CDH_MODE_NOMINAL    ((uint8)1U)
#define ORBITER_CDH_MODE_EMERGENCY  ((uint8)2U)
#define ORBITER_CDH_MODE_MAX        ((uint8)2U)  /* highest valid mode value */

/* ── Pipe depth — sized to hold 7 subscriptions with burst headroom ──────── */
#define ORBITER_CDH_PIPE_DEPTH  ((uint16)20U)

/* ── Mode transition command payload ─────────────────────────────────────── */
typedef struct
{
    CFE_MSG_Message_t Header;   /* CCSDS primary header */
    uint8             Mode;     /* target mode: SAFE=0, NOMINAL=1, EMERGENCY=2 */
    uint8             Padding[3]; /* explicit padding (MISRA C:2012 Rule 6.7) */
} ORBITER_CDH_ModeTransCmd_t;

/* ── Aggregated housekeeping telemetry ───────────────────────────────────── */
typedef struct
{
    CFE_MSG_Message_t Header;        /* CCSDS primary header */
    uint8             CurrentMode;   /* current orbiter mode */
    uint8             Padding[3];    /* explicit padding (MISRA C:2012 Rule 6.7) */
    uint32            CmdCounter;
    uint32            ErrCounter;
    uint32            PeerHkRcvCount; /* total peer HK messages received */
} ORBITER_CDH_HkTlm_t;

/* ── Application state ───────────────────────────────────────────────────── */
typedef struct
{
    uint32            RunStatus;
    CFE_SB_PipeId_t   CmdPipe;
    uint32            CmdCounter;
    uint32            ErrCounter;
    uint32            PeerHkRcvCount;
    ORBITER_CDH_HkTlm_t HkTlm;
} ORBITER_CDH_Data_t;

/* ── Entry point ─────────────────────────────────────────────────────────── */
void ORBITER_CDH_AppMain(void);

#endif /* ORBITER_CDH_H */
