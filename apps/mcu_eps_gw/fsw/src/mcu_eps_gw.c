/*
 * mcu_eps_gw.c — SAKURA-II cFS UART/HDLC Gateway for mcu_eps
 *
 * Polls the simulated UART/HDLC bus for inbound MCU EPS telemetry frames,
 * validates each frame's CRC-16/CCITT-FALSE, and either forwards the SPP
 * to the cFE Software Bus or drops it with an EVS error event.
 * TC commands received on MCU_EPS_CMD_MID are forwarded to the bus stub.
 *
 * Source of truth: docs/interfaces/ICD-mcu-cfs.md §2.3, §3.2, §6.
 *
 * MISRA C:2012 compliance target. Deviations documented inline.
 * No dynamic memory allocation. Stack depth statically bounded.
 * [Q-C8] SPP bytes are never re-encoded; CRC computed only at bus layer.
 * [Q-F3] SilenceCount anchored to .critical_mem.
 * [Q-H4] Bus class = UART/HDLC (definition site for mcu_eps).
 */

#include "mcu_eps_gw.h"
#include <string.h>

static MCU_EPS_GW_Data_t MCU_EPS_GW_Data;

/* Q-F3: bus-silence counter is radiation-sensitive; pinned to .critical_mem.
 * MISRA C:2012 Rule 8.11 deviation: static linkage intentional; section
 * attribute requires file-scope placement.  See [Q-F3] and
 * docs/architecture/09-failure-and-radiation.md §5.1. */
static uint32 MCU_EPS_GW_SilenceCount
    __attribute__((section(".critical_mem"))) = 0U;

/* Forward declarations */
static int32  MCU_EPS_GW_Init(void);
static void   MCU_EPS_GW_PollAndPublish(void);
#ifndef UNIT_TEST
static int32  MCU_EPS_GW_BusPoll(uint8 *FrameBuf, uint16 *FrameLen);
#else
int32 MCU_EPS_GW_BusPoll(uint8 *FrameBuf, uint16 *FrameLen);
#endif
static void   MCU_EPS_GW_BusEmitTc(const CFE_SB_Buffer_t *SBBufPtr);
static int32  MCU_EPS_GW_ValidateHdlcFrame(const uint8 *frame, uint16 len);
static uint16 MCU_EPS_GW_Crc16(const uint8 *data, uint16 len);
static void   MCU_EPS_GW_SendHkPacket(void);

/* ---------------------------------------------------------------------------
 * MCU_EPS_GW_AppMain — Application entry point
 * --------------------------------------------------------------------------- */
void MCU_EPS_GW_AppMain(void)
{
    int32            status;
    CFE_SB_Buffer_t *SBBufPtr;

    MCU_EPS_GW_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    status = MCU_EPS_GW_Init();
    if (status != CFE_SUCCESS)
    {
        MCU_EPS_GW_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&MCU_EPS_GW_Data.RunStatus) == true)
    {
        /* Non-blocking TC drain: forward SB commands to bus stub */
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, MCU_EPS_GW_Data.CmdPipe, 0);
        if (status == CFE_SUCCESS)
        {
            MCU_EPS_GW_BusEmitTc(SBBufPtr);
        }
        else if (status != CFE_SB_PIPE_RD_ERR)
        {
            /* Timeout (no data) is expected; only true errors increment counter */
            (void)status; /* suppress unused-value warning — timeout is nominal */
        }
        else
        {
            CFE_EVS_SendEvent(MCU_EPS_GW_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "EPS_GW: SB receive error 0x%08X",
                              (unsigned int)status);
            MCU_EPS_GW_Data.ErrCounter++;
        }

        /* Bus poll → validate → SB publish or drop */
        MCU_EPS_GW_PollAndPublish();
    }

    CFE_ES_ExitApp(MCU_EPS_GW_Data.RunStatus);
}

/* ---------------------------------------------------------------------------
 * MCU_EPS_GW_Init — One-time application initialization
 * --------------------------------------------------------------------------- */
static int32 MCU_EPS_GW_Init(void)
{
    int32 status;

    status = CFE_ES_RegisterApp();
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_EVS_Register(NULL, 0U, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_CreatePipe(&MCU_EPS_GW_Data.CmdPipe, MCU_EPS_GW_PIPE_DEPTH,
                               "EPS_GW_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(MCU_EPS_GW_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "EPS_GW: pipe creation failed 0x%08X",
                          (unsigned int)status);
        return status;
    }

    /* Subscribe to MCU EPS TC commands (forward to bus) */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(MCU_EPS_CMD_MID),
                              MCU_EPS_GW_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    MCU_EPS_GW_Data.ErrCounter  = 0U;
    MCU_EPS_GW_Data.HkTlm.FramesValid   = 0U;
    MCU_EPS_GW_Data.HkTlm.FramesCorrupt = 0U;
    MCU_EPS_GW_SilenceCount = 0U;

    CFE_EVS_SendEvent(MCU_EPS_GW_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "EPS_GW initialized v%d.%d.%d",
                      MCU_EPS_GW_MAJOR_VERSION, MCU_EPS_GW_MINOR_VERSION,
                      MCU_EPS_GW_REVISION);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * MCU_EPS_GW_PollAndPublish — Poll bus, validate frame, publish or drop
 * --------------------------------------------------------------------------- */
static void MCU_EPS_GW_PollAndPublish(void)
{
    int32 rc;

    MCU_EPS_GW_Data.FrameLen = 0U;
    rc = MCU_EPS_GW_BusPoll(MCU_EPS_GW_Data.FrameBuf, &MCU_EPS_GW_Data.FrameLen);

    if (rc == CFE_SUCCESS)
    {
        if (MCU_EPS_GW_ValidateHdlcFrame(MCU_EPS_GW_Data.FrameBuf,
                                          MCU_EPS_GW_Data.FrameLen) == CFE_SUCCESS)
        {
            MCU_EPS_GW_SilenceCount = 0U;
            MCU_EPS_GW_Data.HkTlm.FramesValid++;
            MCU_EPS_GW_SendHkPacket();
        }
        else
        {
            /* [DoD] Frame corrupt: drop + EVS event, never SB-publish. */
            MCU_EPS_GW_Data.HkTlm.FramesCorrupt++;
            MCU_EPS_GW_Data.ErrCounter++;
            MCU_EPS_GW_SilenceCount++;
            CFE_EVS_SendEvent(MCU_EPS_GW_HDLC_CRC_FAIL_ERR_EID,
                              CFE_EVS_EventType_ERROR,
                              "EPS_GW: HDLC CRC mismatch — frame dropped");
        }
    }
    else if (rc != MCU_EPS_GW_BUS_NO_DATA)
    {
        MCU_EPS_GW_Data.ErrCounter++;
        MCU_EPS_GW_SilenceCount++;
    }
    else
    {
        /* BUS_NO_DATA: nominal quiet cycle */
        MCU_EPS_GW_SilenceCount++;
    }

    /* Declare MCU silent after consecutive cycles without a valid frame */
    if (MCU_EPS_GW_SilenceCount >= MCU_EPS_GW_SILENCE_CYCLES)
    {
        CFE_EVS_SendEvent(MCU_EPS_GW_BUS_SILENT_ERR_EID, CFE_EVS_EventType_ERROR,
                          "EPS_GW: mcu_eps bus silent %u cycles",
                          (unsigned int)MCU_EPS_GW_SilenceCount);
        MCU_EPS_GW_SilenceCount = 0U;
    }
}

/* ---------------------------------------------------------------------------
 * MCU_EPS_GW_BusPoll — Phase 35 stub; returns BUS_NO_DATA always.
 * Phase 42 replaces with real UART/HDLC driver.
 * --------------------------------------------------------------------------- */
#ifndef UNIT_TEST
static int32 MCU_EPS_GW_BusPoll(uint8 *FrameBuf, uint16 *FrameLen)
{
    (void)FrameBuf;
    *FrameLen = 0U;
    return MCU_EPS_GW_BUS_NO_DATA;
}
#endif /* !UNIT_TEST */

/* ---------------------------------------------------------------------------
 * MCU_EPS_GW_BusEmitTc — Phase 35 stub; TC forwarding not yet wired.
 * --------------------------------------------------------------------------- */
static void MCU_EPS_GW_BusEmitTc(const CFE_SB_Buffer_t *SBBufPtr)
{
    (void)SBBufPtr;
    /* Phase 42 stub: UART/HDLC TC emission not yet wired. */
}

/* ---------------------------------------------------------------------------
 * MCU_EPS_GW_Crc16 — CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflect)
 * Per ICD-mcu-cfs.md §2.3 and Q-C9 (HDLC CRC).
 * --------------------------------------------------------------------------- */
static uint16 MCU_EPS_GW_Crc16(const uint8 *data, uint16 len)
{
    uint16 crc = MCU_EPS_GW_CRC16_INIT;
    uint16 i;
    uint8  bit;

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint16)((uint16)data[i] << 8U);
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16)((uint16)(crc << 1U) ^ MCU_EPS_GW_CRC16_POLY);
            }
            else
            {
                crc = (uint16)(crc << 1U);
            }
        }
    }
    return crc;
}

/* ---------------------------------------------------------------------------
 * MCU_EPS_GW_ValidateHdlcFrame — Unescape and verify CRC-16/CCITT-FALSE.
 *
 * Frame layout per ICD §2.3:
 *   FLAG(0x7E) | escaped-SPP-bytes | CRC_lo | CRC_hi | FLAG(0x7E)
 * CRC covers the unescaped SPP bytes (not the CRC bytes themselves).
 * CRC is stored LSB-first inside the HDLC frame (HDLC convention).
 * --------------------------------------------------------------------------- */
static int32 MCU_EPS_GW_ValidateHdlcFrame(const uint8 *frame, uint16 len)
{
    /* MISRA C:2012 Rule 18.8: fixed-size local array; not a VLA. */
    uint8  unescaped[MCU_EPS_GW_MAX_UNESCAPED];
    uint16 ulen = 0U;
    uint16 i;
    uint16 crc_computed;
    uint16 crc_received;

    /* Minimum valid frame: FLAG + at least 1 payload byte + 2 CRC bytes + FLAG */
    if (len < 4U)
    {
        return MCU_EPS_GW_ERR_FRAME_FORMAT;
    }
    if (frame[0U] != MCU_EPS_GW_HDLC_FLAG || frame[len - 1U] != MCU_EPS_GW_HDLC_FLAG)
    {
        return MCU_EPS_GW_ERR_FRAME_FORMAT;
    }

    /* Unescape bytes between the two flag bytes (indices 1 .. len-2) */
    for (i = 1U; i < (len - 1U); i++)
    {
        if (frame[i] == MCU_EPS_GW_HDLC_ESC)
        {
            i++;
            if (i >= (len - 1U))
            {
                /* Escape at end of frame — malformed */
                return MCU_EPS_GW_ERR_FRAME_FORMAT;
            }
            if (ulen >= MCU_EPS_GW_MAX_UNESCAPED)
            {
                return MCU_EPS_GW_ERR_FRAME_TOO_LONG;
            }
            unescaped[ulen] = frame[i] ^ MCU_EPS_GW_HDLC_ESC_XOR;
            ulen++;
        }
        else
        {
            if (ulen >= MCU_EPS_GW_MAX_UNESCAPED)
            {
                return MCU_EPS_GW_ERR_FRAME_TOO_LONG;
            }
            unescaped[ulen] = frame[i];
            ulen++;
        }
    }

    /* Need at least 3 bytes: 1 payload + 2 CRC */
    if (ulen < 3U)
    {
        return MCU_EPS_GW_ERR_FRAME_FORMAT;
    }

    /* CRC covers unescaped[0 .. ulen-3]; last two bytes are the CRC (LSB-first) */
    crc_computed = MCU_EPS_GW_Crc16(unescaped, (uint16)(ulen - 2U));
    crc_received = (uint16)((uint16)unescaped[ulen - 2U] |
                            ((uint16)unescaped[ulen - 1U] << 8U));

    if (crc_computed != crc_received)
    {
        return MCU_EPS_GW_ERR_HDLC_CRC;
    }

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * MCU_EPS_GW_SendHkPacket — Publish gateway HK to MCU_EPS_HK_MID (0x0AA0)
 * --------------------------------------------------------------------------- */
static void MCU_EPS_GW_SendHkPacket(void)
{
    MCU_EPS_GW_Data.HkTlm.ErrCounter   = MCU_EPS_GW_Data.ErrCounter;
    MCU_EPS_GW_Data.HkTlm.BusSilent    = 0U;
    /* Q-F4: time_suspect always 0 until Phase 43 propagation lands. */
    MCU_EPS_GW_Data.HkTlm.TimeSuspect  = 0U;

    CFE_SB_TransmitMsg(&MCU_EPS_GW_Data.HkTlm.Header, true);
}
