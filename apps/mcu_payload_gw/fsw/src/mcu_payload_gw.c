/*
 * mcu_payload_gw.c — SAKURA-II cFS SpaceWire Gateway for mcu_payload
 *
 * Polls the simulated SpaceWire bus for inbound MCU payload telemetry packets,
 * validates the SpW end-of-packet marker, and either forwards the SPP to the
 * cFE Software Bus or drops it with an EVS error event on EEP receipt.
 *
 * Source of truth: docs/interfaces/ICD-mcu-cfs.md §2.1, §3.2, §6.
 *
 * MISRA C:2012 compliance target. Deviations documented inline.
 * No dynamic memory allocation. Stack depth statically bounded.
 * [Q-C8] SPP bytes pass through unmodified.
 * [Q-F3] SilenceCount anchored to .critical_mem.
 * [Q-H4] Bus class = SpaceWire (definition site for mcu_payload).
 */

#include "mcu_payload_gw.h"

static MCU_PAYLOAD_GW_Data_t MCU_PAYLOAD_GW_Data;

/* Q-F3: bus-silence counter — radiation-sensitive; pinned to .critical_mem.
 * MISRA C:2012 Rule 8.11 deviation: static linkage intentional; section
 * attribute requires file-scope placement.  See [Q-F3] and
 * docs/architecture/09-failure-and-radiation.md §5.1. */
static uint32 MCU_PAYLOAD_GW_SilenceCount
    __attribute__((section(".critical_mem"))) = 0U;

/* Forward declarations */
static int32 MCU_PAYLOAD_GW_Init(void);
static void  MCU_PAYLOAD_GW_PollAndPublish(void);
#ifndef UNIT_TEST
static int32 MCU_PAYLOAD_GW_BusPoll(uint8 *FrameBuf, uint16 *FrameLen);
#else
/* Under UNIT_TEST the bus-poll stub is provided by the test driver. */
int32 MCU_PAYLOAD_GW_BusPoll(uint8 *FrameBuf, uint16 *FrameLen);
#endif
static void  MCU_PAYLOAD_GW_BusEmitTc(const CFE_SB_Buffer_t *SBBufPtr);
static int32 MCU_PAYLOAD_GW_ValidateSpwFrame(const uint8 *frame, uint16 len);
static void  MCU_PAYLOAD_GW_SendHkPacket(void);

/* ---------------------------------------------------------------------------
 * MCU_PAYLOAD_GW_AppMain — Application entry point
 * --------------------------------------------------------------------------- */
void MCU_PAYLOAD_GW_AppMain(void)
{
    int32            status;
    CFE_SB_Buffer_t *SBBufPtr;

    MCU_PAYLOAD_GW_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    status = MCU_PAYLOAD_GW_Init();
    if (status != CFE_SUCCESS)
    {
        MCU_PAYLOAD_GW_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&MCU_PAYLOAD_GW_Data.RunStatus) == true)
    {
        /* Non-blocking TC drain */
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, MCU_PAYLOAD_GW_Data.CmdPipe, 0);
        if (status == CFE_SUCCESS)
        {
            MCU_PAYLOAD_GW_BusEmitTc(SBBufPtr);
        }
        else if (status != CFE_SB_PIPE_RD_ERR)
        {
            (void)status; /* timeout is nominal */
        }
        else
        {
            CFE_EVS_SendEvent(MCU_PAYLOAD_GW_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "PAYLOAD_GW: SB receive error 0x%08X",
                              (unsigned int)status);
            MCU_PAYLOAD_GW_Data.ErrCounter++;
        }

        MCU_PAYLOAD_GW_PollAndPublish();
    }

    CFE_ES_ExitApp(MCU_PAYLOAD_GW_Data.RunStatus);
}

/* ---------------------------------------------------------------------------
 * MCU_PAYLOAD_GW_Init — One-time application initialization
 * --------------------------------------------------------------------------- */
static int32 MCU_PAYLOAD_GW_Init(void)
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

    status = CFE_SB_CreatePipe(&MCU_PAYLOAD_GW_Data.CmdPipe,
                               MCU_PAYLOAD_GW_PIPE_DEPTH,
                               "PAYLOAD_GW_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(MCU_PAYLOAD_GW_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "PAYLOAD_GW: pipe creation failed 0x%08X",
                          (unsigned int)status);
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(MCU_PAYLOAD_CMD_MID),
                              MCU_PAYLOAD_GW_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    MCU_PAYLOAD_GW_Data.ErrCounter              = 0U;
    MCU_PAYLOAD_GW_Data.HkTlm.FramesValid       = 0U;
    MCU_PAYLOAD_GW_Data.HkTlm.FramesCorrupt     = 0U;
    MCU_PAYLOAD_GW_SilenceCount = 0U;

    CFE_EVS_SendEvent(MCU_PAYLOAD_GW_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "PAYLOAD_GW initialized v%d.%d.%d",
                      MCU_PAYLOAD_GW_MAJOR_VERSION, MCU_PAYLOAD_GW_MINOR_VERSION,
                      MCU_PAYLOAD_GW_REVISION);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * MCU_PAYLOAD_GW_PollAndPublish — Poll SpW bus, validate EOP/EEP, publish
 * --------------------------------------------------------------------------- */
static void MCU_PAYLOAD_GW_PollAndPublish(void)
{
    int32 rc;

    MCU_PAYLOAD_GW_Data.FrameLen = 0U;
    rc = MCU_PAYLOAD_GW_BusPoll(MCU_PAYLOAD_GW_Data.FrameBuf,
                                  &MCU_PAYLOAD_GW_Data.FrameLen);

    if (rc == CFE_SUCCESS)
    {
        if (MCU_PAYLOAD_GW_ValidateSpwFrame(MCU_PAYLOAD_GW_Data.FrameBuf,
                                             MCU_PAYLOAD_GW_Data.FrameLen) == CFE_SUCCESS)
        {
            MCU_PAYLOAD_GW_SilenceCount = 0U;
            MCU_PAYLOAD_GW_Data.HkTlm.FramesValid++;
            MCU_PAYLOAD_GW_SendHkPacket();
        }
        else
        {
            /* [DoD] EEP or malformed frame: drop + EVS event, never SB-publish. */
            MCU_PAYLOAD_GW_Data.HkTlm.FramesCorrupt++;
            MCU_PAYLOAD_GW_Data.ErrCounter++;
            MCU_PAYLOAD_GW_SilenceCount++;
            CFE_EVS_SendEvent(MCU_PAYLOAD_GW_EEP_ERR_EID, CFE_EVS_EventType_ERROR,
                              "PAYLOAD_GW: SpW EEP received — packet dropped");
        }
    }
    else if (rc != MCU_PAYLOAD_GW_BUS_NO_DATA)
    {
        MCU_PAYLOAD_GW_Data.ErrCounter++;
        MCU_PAYLOAD_GW_SilenceCount++;
    }
    else
    {
        MCU_PAYLOAD_GW_SilenceCount++;
    }

    if (MCU_PAYLOAD_GW_SilenceCount >= MCU_PAYLOAD_GW_SILENCE_CYCLES)
    {
        CFE_EVS_SendEvent(MCU_PAYLOAD_GW_BUS_SILENT_ERR_EID, CFE_EVS_EventType_ERROR,
                          "PAYLOAD_GW: mcu_payload bus silent %u cycles",
                          (unsigned int)MCU_PAYLOAD_GW_SilenceCount);
        MCU_PAYLOAD_GW_SilenceCount = 0U;
    }
}

/* ---------------------------------------------------------------------------
 * MCU_PAYLOAD_GW_BusPoll — Phase 35 stub; real SpW driver lands in Phase 42.
 * --------------------------------------------------------------------------- */
#ifndef UNIT_TEST
static int32 MCU_PAYLOAD_GW_BusPoll(uint8 *FrameBuf, uint16 *FrameLen)
{
    (void)FrameBuf;
    *FrameLen = 0U;
    return MCU_PAYLOAD_GW_BUS_NO_DATA;
}
#endif /* !UNIT_TEST */

/* ---------------------------------------------------------------------------
 * MCU_PAYLOAD_GW_BusEmitTc — Phase 35 stub; SpW TC emission not yet wired.
 * --------------------------------------------------------------------------- */
static void MCU_PAYLOAD_GW_BusEmitTc(const CFE_SB_Buffer_t *SBBufPtr)
{
    (void)SBBufPtr;
}

/* ---------------------------------------------------------------------------
 * MCU_PAYLOAD_GW_ValidateSpwFrame — Check SpW end-of-packet marker (ICD §2.1)
 *
 * The last byte of a SpW packet is the end marker:
 *   0x00 = EOP (normal) — accept
 *   0x01 = EEP (error)  — discard, emit event
 * --------------------------------------------------------------------------- */
static int32 MCU_PAYLOAD_GW_ValidateSpwFrame(const uint8 *frame, uint16 len)
{
    if (len < 1U)
    {
        return MCU_PAYLOAD_GW_ERR_FRAME_FORMAT;
    }
    if (frame[len - 1U] == MCU_PAYLOAD_GW_SPW_EEP)
    {
        return MCU_PAYLOAD_GW_ERR_EEP;
    }
    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * MCU_PAYLOAD_GW_SendHkPacket — Publish gateway HK to MCU_PAYLOAD_HK_MID
 * --------------------------------------------------------------------------- */
static void MCU_PAYLOAD_GW_SendHkPacket(void)
{
    MCU_PAYLOAD_GW_Data.HkTlm.ErrCounter  = MCU_PAYLOAD_GW_Data.ErrCounter;
    MCU_PAYLOAD_GW_Data.HkTlm.BusSilent   = 0U;
    MCU_PAYLOAD_GW_Data.HkTlm.TimeSuspect = 0U;

    CFE_SB_TransmitMsg(&MCU_PAYLOAD_GW_Data.HkTlm.Header, true);
}
