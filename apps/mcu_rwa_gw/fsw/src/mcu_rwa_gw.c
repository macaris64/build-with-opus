/*
 * mcu_rwa_gw.c — SAKURA-II cFS CAN 2.0A Gateway for mcu_rwa
 *
 * Polls the simulated CAN bus for inbound MCU RWA telemetry frames,
 * validates CAN fragment sequencing per ICD-mcu-cfs.md §2.2, and either
 * forwards the SPP to the cFE Software Bus or drops it with an EVS error.
 *
 * Source of truth: docs/interfaces/ICD-mcu-cfs.md §2.2, §3.2, §6.
 *
 * MISRA C:2012 compliance target. Deviations documented inline.
 * No dynamic memory allocation. Stack depth statically bounded.
 * [Q-C8] SPP bytes pass through unmodified.
 * [Q-F3] SilenceCount and CanInProgress anchored to .critical_mem.
 * [Q-H4] Bus class = CAN 2.0A (definition site for mcu_rwa).
 */

#include "mcu_rwa_gw.h"

static MCU_RWA_GW_Data_t MCU_RWA_GW_Data;

/* Q-F3: bus-silence counter — radiation-sensitive; pinned to .critical_mem.
 * MISRA C:2012 Rule 8.11 deviation: static linkage intentional; section
 * attribute requires file-scope placement.  See [Q-F3] and
 * docs/architecture/09-failure-and-radiation.md §5.1. */
static uint32 MCU_RWA_GW_SilenceCount
    __attribute__((section(".critical_mem"))) = 0U;

/* Q-F3: CAN fragment-assembly state — radiation-sensitive.
 * 0 = no first-frame received; 1 = first-frame received, expecting consecutive. */
static uint8 MCU_RWA_GW_CanInProgress
    __attribute__((section(".critical_mem"))) = 0U;

/* Forward declarations */
static int32 MCU_RWA_GW_Init(void);
static void  MCU_RWA_GW_PollAndPublish(void);
#ifndef UNIT_TEST
static int32 MCU_RWA_GW_BusPoll(uint8 *FrameBuf, uint16 *FrameLen);
#else
int32 MCU_RWA_GW_BusPoll(uint8 *FrameBuf, uint16 *FrameLen);
#endif
static void  MCU_RWA_GW_BusEmitTc(const CFE_SB_Buffer_t *SBBufPtr);
static int32 MCU_RWA_GW_ValidateCanFrame(const uint8 *frame, uint16 len);
static void  MCU_RWA_GW_SendHkPacket(void);

/* ---------------------------------------------------------------------------
 * MCU_RWA_GW_AppMain — Application entry point
 * --------------------------------------------------------------------------- */
void MCU_RWA_GW_AppMain(void)
{
    int32            status;
    CFE_SB_Buffer_t *SBBufPtr;

    MCU_RWA_GW_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    status = MCU_RWA_GW_Init();
    if (status != CFE_SUCCESS)
    {
        MCU_RWA_GW_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&MCU_RWA_GW_Data.RunStatus) == true)
    {
        /* Non-blocking TC drain */
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, MCU_RWA_GW_Data.CmdPipe, 0);
        if (status == CFE_SUCCESS)
        {
            MCU_RWA_GW_BusEmitTc(SBBufPtr);
        }
        else if (status != CFE_SB_PIPE_RD_ERR)
        {
            (void)status; /* timeout is nominal */
        }
        else
        {
            CFE_EVS_SendEvent(MCU_RWA_GW_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "RWA_GW: SB receive error 0x%08X",
                              (unsigned int)status);
            MCU_RWA_GW_Data.ErrCounter++;
        }

        MCU_RWA_GW_PollAndPublish();
    }

    CFE_ES_ExitApp(MCU_RWA_GW_Data.RunStatus);
}

/* ---------------------------------------------------------------------------
 * MCU_RWA_GW_Init — One-time application initialization
 * --------------------------------------------------------------------------- */
static int32 MCU_RWA_GW_Init(void)
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

    status = CFE_SB_CreatePipe(&MCU_RWA_GW_Data.CmdPipe, MCU_RWA_GW_PIPE_DEPTH,
                               "RWA_GW_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(MCU_RWA_GW_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "RWA_GW: pipe creation failed 0x%08X",
                          (unsigned int)status);
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(MCU_RWA_CMD_MID),
                              MCU_RWA_GW_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    MCU_RWA_GW_Data.ErrCounter              = 0U;
    MCU_RWA_GW_Data.HkTlm.FramesValid       = 0U;
    MCU_RWA_GW_Data.HkTlm.FramesCorrupt     = 0U;
    MCU_RWA_GW_SilenceCount  = 0U;
    MCU_RWA_GW_CanInProgress = 0U;

    CFE_EVS_SendEvent(MCU_RWA_GW_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "RWA_GW initialized v%d.%d.%d",
                      MCU_RWA_GW_MAJOR_VERSION, MCU_RWA_GW_MINOR_VERSION,
                      MCU_RWA_GW_REVISION);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * MCU_RWA_GW_PollAndPublish — Poll CAN bus, validate fragment sequence, publish
 * --------------------------------------------------------------------------- */
static void MCU_RWA_GW_PollAndPublish(void)
{
    int32 rc;

    MCU_RWA_GW_Data.FrameLen = 0U;
    rc = MCU_RWA_GW_BusPoll(MCU_RWA_GW_Data.FrameBuf, &MCU_RWA_GW_Data.FrameLen);

    if (rc == CFE_SUCCESS)
    {
        if (MCU_RWA_GW_ValidateCanFrame(MCU_RWA_GW_Data.FrameBuf,
                                         MCU_RWA_GW_Data.FrameLen) == CFE_SUCCESS)
        {
            MCU_RWA_GW_SilenceCount = 0U;
            MCU_RWA_GW_Data.HkTlm.FramesValid++;
            MCU_RWA_GW_SendHkPacket();
        }
        else
        {
            /* [DoD] Frame corrupt: drop + EVS event, never SB-publish. */
            MCU_RWA_GW_CanInProgress = 0U; /* reset assembly state */
            MCU_RWA_GW_Data.HkTlm.FramesCorrupt++;
            MCU_RWA_GW_Data.ErrCounter++;
            MCU_RWA_GW_SilenceCount++;
            CFE_EVS_SendEvent(MCU_RWA_GW_CAN_FRAGMENT_LOST_ERR_EID,
                              CFE_EVS_EventType_ERROR,
                              "RWA_GW: CAN fragment sequence error — frame dropped");
        }
    }
    else if (rc != MCU_RWA_GW_BUS_NO_DATA)
    {
        MCU_RWA_GW_Data.ErrCounter++;
        MCU_RWA_GW_SilenceCount++;
    }
    else
    {
        MCU_RWA_GW_SilenceCount++;
    }

    if (MCU_RWA_GW_SilenceCount >= MCU_RWA_GW_SILENCE_CYCLES)
    {
        CFE_EVS_SendEvent(MCU_RWA_GW_BUS_SILENT_ERR_EID, CFE_EVS_EventType_ERROR,
                          "RWA_GW: mcu_rwa bus silent %u cycles",
                          (unsigned int)MCU_RWA_GW_SilenceCount);
        MCU_RWA_GW_SilenceCount = 0U;
    }
}

/* ---------------------------------------------------------------------------
 * MCU_RWA_GW_BusPoll — Phase 35 stub; real CAN driver lands in Phase 42.
 * --------------------------------------------------------------------------- */
#ifndef UNIT_TEST
static int32 MCU_RWA_GW_BusPoll(uint8 *FrameBuf, uint16 *FrameLen)
{
    (void)FrameBuf;
    *FrameLen = 0U;
    return MCU_RWA_GW_BUS_NO_DATA;
}
#endif /* !UNIT_TEST */

/* ---------------------------------------------------------------------------
 * MCU_RWA_GW_BusEmitTc — Phase 35 stub; CAN TC emission not yet wired.
 * --------------------------------------------------------------------------- */
static void MCU_RWA_GW_BusEmitTc(const CFE_SB_Buffer_t *SBBufPtr)
{
    (void)SBBufPtr;
}

/* ---------------------------------------------------------------------------
 * MCU_RWA_GW_ValidateCanFrame — Check CAN fragment sequence (ICD §2.2)
 *
 * frame_type byte (frame[0]):
 *   0x00       = Single-Frame  — complete SPP in one frame
 *   0x10..0x1F = First-Frame   — start of multi-frame SPP
 *   0x20..0x2F = Consecutive-Frame — continuation; invalid before First-Frame
 *
 * State tracked in MCU_RWA_GW_CanInProgress (.critical_mem).
 * --------------------------------------------------------------------------- */
static int32 MCU_RWA_GW_ValidateCanFrame(const uint8 *frame, uint16 len)
{
    uint8 ft;

    if (len < 1U)
    {
        return MCU_RWA_GW_ERR_FRAME_FORMAT;
    }

    ft = frame[0U];

    if (ft == MCU_RWA_GW_CAN_SF)
    {
        /* Single-Frame: valid regardless of current assembly state */
        MCU_RWA_GW_CanInProgress = 0U;
        return CFE_SUCCESS;
    }

    if (ft >= MCU_RWA_GW_CAN_FF_MIN && ft <= MCU_RWA_GW_CAN_FF_MAX)
    {
        /* First-Frame: begin multi-frame assembly */
        MCU_RWA_GW_CanInProgress = 1U;
        return CFE_SUCCESS;
    }

    if (ft >= MCU_RWA_GW_CAN_CF_MIN && ft <= MCU_RWA_GW_CAN_CF_MAX)
    {
        /* Consecutive-Frame: valid only if a First-Frame was received */
        if (MCU_RWA_GW_CanInProgress == 0U)
        {
            return MCU_RWA_GW_ERR_FRAGMENT_LOST;
        }
        return CFE_SUCCESS;
    }

    return MCU_RWA_GW_ERR_FRAME_FORMAT;
}

/* ---------------------------------------------------------------------------
 * MCU_RWA_GW_SendHkPacket — Publish gateway HK to MCU_RWA_HK_MID (0x0A90)
 * --------------------------------------------------------------------------- */
static void MCU_RWA_GW_SendHkPacket(void)
{
    MCU_RWA_GW_Data.HkTlm.ErrCounter  = MCU_RWA_GW_Data.ErrCounter;
    MCU_RWA_GW_Data.HkTlm.BusSilent   = 0U;
    MCU_RWA_GW_Data.HkTlm.TimeSuspect = 0U;

    CFE_SB_TransmitMsg(&MCU_RWA_GW_Data.HkTlm.Header, true);
}
