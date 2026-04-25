#ifndef MCU_RWA_GW_H
#define MCU_RWA_GW_H

/*
 * mcu_rwa_gw.h — cFS-side CAN 2.0A gateway for mcu_rwa (APID 0x290–0x29F).
 *
 * Bridges the cFE Software Bus to the simulated CAN 2.0A bus connecting the
 * reaction-wheel assembly MCU.  Inbound CAN frames are validated for correct
 * ISO 15765-2-style fragment sequencing (Single-Frame, First-Frame, Consecutive-
 * Frame per ICD-mcu-cfs.md §2.2); an out-of-sequence Consecutive-Frame causes
 * the in-progress SPP to be discarded with an EVS error event — it is never
 * SB-published.  TC commands received on MCU_RWA_CMD_MID are forwarded to the
 * bus stub (Phase 42 wires real CAN driver).
 *
 * Source of truth: docs/interfaces/ICD-mcu-cfs.md §2.2, §3, §6.
 *                  docs/architecture/03-subsystem-mcus.md §4.2.
 *                  _defs/mids.h  (MCU_RWA_HK_MID = 0x0A90, MCU_RWA_CMD_MID = 0x1A90).
 * [Q-C8]  SPP bytes pass through unmodified.
 * [Q-F3]  SilenceCount and CanInProgress pinned to .critical_mem.
 * [Q-H4]  Bus family = CAN 2.0A (definition site for mcu_rwa bus class).
 * [Q-F4]  time_suspect seam reserved for Phase 43.
 */

#include "cfe.h"
#include "mcu_rwa_gw_events.h"
#include "mcu_rwa_gw_version.h"
#include "mids.h"

/* ── Pipe / timing constants ─────────────────────────────────────────────── */
#define MCU_RWA_GW_PIPE_DEPTH       ((uint16)64U)

/* ── CAN frame size limits ───────────────────────────────────────────────── */
/* CAN 2.0A frame: 1B frame_type + up to 7B payload = 8 bytes max */
#define MCU_RWA_GW_MAX_FRAME_LEN    ((uint16)8U)

/* ── Bus-silence threshold ───────────────────────────────────────────────── */
#define MCU_RWA_GW_SILENCE_CYCLES   ((uint32)3U)

/* ── CAN fragmentation protocol constants (ICD §2.2, ISO 15765-2 pattern) ── */
#define MCU_RWA_GW_CAN_SF           ((uint8)0x00U)  /* Single-Frame */
#define MCU_RWA_GW_CAN_FF_MIN       ((uint8)0x10U)  /* First-Frame low bound */
#define MCU_RWA_GW_CAN_FF_MAX       ((uint8)0x1FU)  /* First-Frame high bound */
#define MCU_RWA_GW_CAN_CF_MIN       ((uint8)0x20U)  /* Consecutive-Frame low bound */
#define MCU_RWA_GW_CAN_CF_MAX       ((uint8)0x2FU)  /* Consecutive-Frame high bound */

/* ── Bus driver return codes ─────────────────────────────────────────────── */
#define MCU_RWA_GW_BUS_NO_DATA      ((int32)1)

/* ── Internal error codes ────────────────────────────────────────────────── */
#define MCU_RWA_GW_ERR_FRAME_FORMAT   ((int32)-10)
#define MCU_RWA_GW_ERR_FRAGMENT_LOST  ((int32)-11)

/* ── Gateway HK telemetry (published to MCU_RWA_HK_MID = 0x0A90) ─────────── */
typedef struct
{
    CFE_MSG_Message_t Header;
    uint32            FramesValid;
    uint32            FramesCorrupt;
    uint32            ErrCounter;
    uint8             BusSilent;
    uint8             TimeSuspect;  /* Q-F4: reserved for Phase 43 */
    uint8             Padding[2];
} MCU_RWA_GW_HkTlm_t;

/* ── Application state ───────────────────────────────────────────────────── */
typedef struct
{
    uint32             RunStatus;
    CFE_SB_PipeId_t    CmdPipe;
    uint32             ErrCounter;
    uint8              FrameBuf[MCU_RWA_GW_MAX_FRAME_LEN];
    uint16             FrameLen;
    MCU_RWA_GW_HkTlm_t HkTlm;
} MCU_RWA_GW_Data_t;

/* ── Entry point ─────────────────────────────────────────────────────────── */
void MCU_RWA_GW_AppMain(void);

#endif /* MCU_RWA_GW_H */
