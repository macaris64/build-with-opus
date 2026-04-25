#ifndef MCU_EPS_GW_H
#define MCU_EPS_GW_H

/*
 * mcu_eps_gw.h — cFS-side UART/HDLC gateway for mcu_eps (APID 0x2A0–0x2AF).
 *
 * Bridges the cFE Software Bus to the simulated UART/HDLC bus that connects
 * to the EPS microcontroller.  Inbound MCU TM frames are validated against
 * CRC-16/CCITT-FALSE (per ICD-mcu-cfs.md §2.3); corrupt frames are dropped
 * with an EVS error event and never SB-published.  TC commands received on
 * MCU_EPS_CMD_MID are forwarded to the bus stub (Phase 42 wires real UART).
 *
 * Source of truth: docs/interfaces/ICD-mcu-cfs.md §2.3, §3, §6.
 *                  docs/architecture/03-subsystem-mcus.md §4.3.
 *                  _defs/mids.h  (MCU_EPS_HK_MID = 0x0AA0, MCU_EPS_CMD_MID = 0x1AA0).
 * [Q-C8]  SPP bytes pass through unmodified; no ad-hoc BE↔LE inside the gateway.
 * [Q-F3]  SilenceCount pinned to .critical_mem.
 * [Q-H4]  Bus family = UART/HDLC (this is the definition site for mcu_eps bus class).
 * [Q-F4]  time_suspect seam reserved for Phase 43.
 */

#include "cfe.h"
#include "mcu_eps_gw_events.h"
#include "mcu_eps_gw_version.h"
#include "mids.h"

/* ── Pipe / timing constants ─────────────────────────────────────────────── */
#define MCU_EPS_GW_PIPE_DEPTH      ((uint16)16U)

/* ── Bus frame size limits ───────────────────────────────────────────────── */
/* 256 B max SPP + HDLC flags (2) + worst-case byte-stuffing doubles payload */
#define MCU_EPS_GW_MAX_FRAME_LEN   ((uint16)514U)
/* Unescaped payload (SPP bytes + 2 CRC bytes): 256 + 2 */
#define MCU_EPS_GW_MAX_UNESCAPED   ((uint16)258U)

/* ── Bus-silence threshold ───────────────────────────────────────────────── */
/* Declare MCU-SILENT after this many consecutive poll cycles with no valid frame */
#define MCU_EPS_GW_SILENCE_CYCLES  ((uint32)3U)

/* ── HDLC framing constants (RFC 1662 / ICD §2.3) ───────────────────────── */
#define MCU_EPS_GW_HDLC_FLAG       ((uint8)0x7EU)
#define MCU_EPS_GW_HDLC_ESC        ((uint8)0x7DU)
#define MCU_EPS_GW_HDLC_ESC_XOR   ((uint8)0x20U)

/* ── CRC-16/CCITT-FALSE constants (ICD §2.3) ────────────────────────────── */
#define MCU_EPS_GW_CRC16_INIT      ((uint16)0xFFFFU)
#define MCU_EPS_GW_CRC16_POLY      ((uint16)0x1021U)

/* ── Bus driver return codes ─────────────────────────────────────────────── */
#define MCU_EPS_GW_BUS_NO_DATA     ((int32)1)   /* non-error: no frame available */

/* ── Internal error codes (returned by validation functions only) ────────── */
#define MCU_EPS_GW_ERR_FRAME_FORMAT  ((int32)-10)
#define MCU_EPS_GW_ERR_FRAME_TOO_LONG ((int32)-11)
#define MCU_EPS_GW_ERR_HDLC_CRC      ((int32)-12)

/* ── Gateway HK telemetry (published to MCU_EPS_HK_MID = 0x0AA0) ────────── */
typedef struct
{
    CFE_MSG_Message_t Header;
    uint32            FramesValid;
    uint32            FramesCorrupt;
    uint32            ErrCounter;
    uint8             BusSilent;   /* 1 = bus declared silent this cycle */
    /* Q-F4: time_suspect reserved for Phase 43 */
    uint8             TimeSuspect;
    uint8             Padding[2];
} MCU_EPS_GW_HkTlm_t;

/* ── Application state ───────────────────────────────────────────────────── */
typedef struct
{
    uint32             RunStatus;
    CFE_SB_PipeId_t    CmdPipe;
    uint32             ErrCounter;
    uint8              FrameBuf[MCU_EPS_GW_MAX_FRAME_LEN];
    uint16             FrameLen;
    MCU_EPS_GW_HkTlm_t HkTlm;
} MCU_EPS_GW_Data_t;

/* ── Entry point ─────────────────────────────────────────────────────────── */
void MCU_EPS_GW_AppMain(void);

#endif /* MCU_EPS_GW_H */
