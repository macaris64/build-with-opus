#ifndef MCU_PAYLOAD_GW_H
#define MCU_PAYLOAD_GW_H

/*
 * mcu_payload_gw.h — cFS-side SpaceWire gateway for mcu_payload (APID 0x280–0x28F).
 *
 * Bridges the cFE Software Bus to the simulated SpaceWire bus connecting the
 * science payload MCU.  Inbound SpW packets are validated for the EOP/EEP end
 * marker per ICD-mcu-cfs.md §2.1: an EEP marker (0x01) indicates a bus-level
 * error and the packet is dropped with an EVS error event — never SB-published.
 * TC commands received on MCU_PAYLOAD_CMD_MID are forwarded to the bus stub
 * (Phase 42 wires real SpW driver).
 *
 * Source of truth: docs/interfaces/ICD-mcu-cfs.md §2.1, §3, §6.
 *                  docs/architecture/03-subsystem-mcus.md §4.1.
 *                  _defs/mids.h  (MCU_PAYLOAD_HK_MID=0x0A80, MCU_PAYLOAD_CMD_MID=0x1A80).
 * [Q-C8]  SPP bytes pass through unmodified.
 * [Q-F3]  SilenceCount pinned to .critical_mem.
 * [Q-H4]  Bus family = SpaceWire (definition site for mcu_payload bus class).
 * [Q-F4]  time_suspect seam reserved for Phase 43.
 */

#include "cfe.h"
#include "mcu_payload_gw_events.h"
#include "mcu_payload_gw_version.h"
#include "mids.h"

/* ── Pipe / timing constants ─────────────────────────────────────────────── */
#define MCU_PAYLOAD_GW_PIPE_DEPTH       ((uint16)64U)

/* ── SpW frame size limit (max SPP 256 B + 1 B end marker) ──────────────── */
#define MCU_PAYLOAD_GW_MAX_FRAME_LEN    ((uint16)257U)

/* ── Bus-silence threshold ───────────────────────────────────────────────── */
#define MCU_PAYLOAD_GW_SILENCE_CYCLES   ((uint32)3U)

/* ── SpaceWire end-of-packet markers (ECSS-E-ST-50-12C, ICD §2.1) ────────── */
#define MCU_PAYLOAD_GW_SPW_EOP          ((uint8)0x00U)  /* normal end-of-packet */
#define MCU_PAYLOAD_GW_SPW_EEP          ((uint8)0x01U)  /* error end-of-packet */

/* ── Bus driver return codes ─────────────────────────────────────────────── */
#define MCU_PAYLOAD_GW_BUS_NO_DATA      ((int32)1)

/* ── Internal error codes ────────────────────────────────────────────────── */
#define MCU_PAYLOAD_GW_ERR_FRAME_FORMAT ((int32)-10)
#define MCU_PAYLOAD_GW_ERR_EEP          ((int32)-11)

/* ── Gateway HK telemetry (published to MCU_PAYLOAD_HK_MID = 0x0A80) ──────── */
typedef struct
{
    CFE_MSG_Message_t Header;
    uint32            FramesValid;
    uint32            FramesCorrupt;
    uint32            ErrCounter;
    uint8             BusSilent;
    uint8             TimeSuspect;  /* Q-F4: reserved for Phase 43 */
    uint8             Padding[2];
} MCU_PAYLOAD_GW_HkTlm_t;

/* ── Application state ───────────────────────────────────────────────────── */
typedef struct
{
    uint32                  RunStatus;
    CFE_SB_PipeId_t         CmdPipe;
    uint32                  ErrCounter;
    uint8                   FrameBuf[MCU_PAYLOAD_GW_MAX_FRAME_LEN];
    uint16                  FrameLen;
    MCU_PAYLOAD_GW_HkTlm_t  HkTlm;
} MCU_PAYLOAD_GW_Data_t;

/* ── Entry point ─────────────────────────────────────────────────────────── */
void MCU_PAYLOAD_GW_AppMain(void);

#endif /* MCU_PAYLOAD_GW_H */
