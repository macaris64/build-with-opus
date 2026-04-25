/*
 * orbiter_payload.c — SAKURA-II Orbiter Science Payload Manager Application
 *
 * Controls payload power (on/off) and science mode selection via
 * ORBITER_PAYLOAD_CMD_MID (0x1985).  Science mode changes are rejected while
 * the payload bus is unpowered — protecting instruments from partial-power
 * configuration sequences.  Aggregates payload TM from mcu_payload_gw
 * (APID 0x140–0x15F; stub zeros until Phase 35).
 *
 * Source of truth: docs/architecture/01-orbiter-cfs.md
 *                  docs/interfaces/apid-registry.md (TM 0x140–0x15F, TC 0x185)
 *
 * MISRA C:2012 compliance target. Deviations documented inline.
 * No dynamic memory allocation. Stack depth statically bounded.
 * [Q-F3] PowerState and ScienceMode are radiation-sensitive; anchored to .critical_mem.
 * [Q-F4] time_suspect propagation deferred to Phase 43.
 */

#include "orbiter_payload.h"

/* Single application-global state object — static to restrict linkage */
static ORBITER_PAYLOAD_Data_t ORBITER_PAYLOAD_Data;

/* Q-F3: payload power state is radiation-sensitive; pinned to .critical_mem.
 * MISRA C:2012 Rule 8.11 deviation: static linkage is intentional;
 * section attribute requires file-scope placement outside the struct.
 * See docs/architecture/09-failure-and-radiation.md §5.1 and [Q-F3]. */
static uint8 ORBITER_PAYLOAD_PowerState
    __attribute__((section(".critical_mem"))) = 0U;

/* Q-F3: active science mode index is radiation-sensitive; pinned to .critical_mem.
 * MISRA C:2012 Rule 8.11 deviation: same rationale as above. */
static uint8 ORBITER_PAYLOAD_ScienceMode
    __attribute__((section(".critical_mem"))) = 0U;

/* Forward declarations */
static int32 ORBITER_PAYLOAD_Init(void);
static void  ORBITER_PAYLOAD_ProcessCommandPacket(const CFE_SB_Buffer_t *SBBufPtr);
static void  ORBITER_PAYLOAD_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr);
static void  ORBITER_PAYLOAD_ProcessPayloadData(void);
static void  ORBITER_PAYLOAD_SendHkPacket(void);

/* ---------------------------------------------------------------------------
 * ORBITER_PAYLOAD_AppMain — Application entry point
 * --------------------------------------------------------------------------- */
void ORBITER_PAYLOAD_AppMain(void)
{
    int32            status;
    CFE_SB_Buffer_t *SBBufPtr;

    ORBITER_PAYLOAD_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    status = ORBITER_PAYLOAD_Init();
    if (status != CFE_SUCCESS)
    {
        ORBITER_PAYLOAD_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&ORBITER_PAYLOAD_Data.RunStatus) == true)
    {
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, ORBITER_PAYLOAD_Data.CmdPipe,
                                      CFE_SB_PEND_FOREVER);
        if (status == CFE_SUCCESS)
        {
            ORBITER_PAYLOAD_ProcessCommandPacket(SBBufPtr);
        }
        else
        {
            CFE_EVS_SendEvent(ORBITER_PAYLOAD_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "PAYLOAD: SB receive error, status=0x%08X",
                              (unsigned int)status);
            ORBITER_PAYLOAD_Data.ErrCounter++;
        }
    }

    CFE_ES_ExitApp(ORBITER_PAYLOAD_Data.RunStatus);
}

/* ---------------------------------------------------------------------------
 * ORBITER_PAYLOAD_Init — One-time application initialization
 * --------------------------------------------------------------------------- */
static int32 ORBITER_PAYLOAD_Init(void)
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

    status = CFE_SB_CreatePipe(&ORBITER_PAYLOAD_Data.CmdPipe, ORBITER_PAYLOAD_PIPE_DEPTH,
                               "PAYLOAD_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(ORBITER_PAYLOAD_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "PAYLOAD: pipe creation failed, status=0x%08X",
                          (unsigned int)status);
        return status;
    }

    /* Subscribe to inbound TC commands */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_PAYLOAD_CMD_MID),
                              ORBITER_PAYLOAD_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Subscribe to HK request (SCH app triggers HK publication) */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_PAYLOAD_HK_MID),
                              ORBITER_PAYLOAD_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Subscribe to payload gateway HK — stub ingress until Phase 35 */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(MCU_PAYLOAD_HK_MID),
                              ORBITER_PAYLOAD_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    ORBITER_PAYLOAD_Data.CmdCounter = 0U;
    ORBITER_PAYLOAD_Data.ErrCounter = 0U;

    CFE_EVS_SendEvent(ORBITER_PAYLOAD_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "PAYLOAD initialized, version %d.%d.%d",
                      ORBITER_PAYLOAD_MAJOR_VERSION, ORBITER_PAYLOAD_MINOR_VERSION,
                      ORBITER_PAYLOAD_REVISION);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * ORBITER_PAYLOAD_ProcessCommandPacket — Top-level MID dispatcher
 * --------------------------------------------------------------------------- */
static void ORBITER_PAYLOAD_ProcessCommandPacket(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    switch (CFE_SB_MsgIdToValue(MsgId))
    {
        case ORBITER_PAYLOAD_CMD_MID:
            ORBITER_PAYLOAD_ProcessGroundCommand(SBBufPtr);
            break;

        case ORBITER_PAYLOAD_HK_MID:
            ORBITER_PAYLOAD_SendHkPacket();
            break;

        case MCU_PAYLOAD_HK_MID:
            /* Phase 35 soft dependency: update payload data from gateway.
             * Until mcu_payload_gw lands, this is a no-op that accepts the message. */
            ORBITER_PAYLOAD_ProcessPayloadData();
            break;

        default:
            CFE_EVS_SendEvent(ORBITER_PAYLOAD_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "PAYLOAD: unknown MsgId=0x%04X",
                              (unsigned int)CFE_SB_MsgIdToValue(MsgId));
            ORBITER_PAYLOAD_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * ORBITER_PAYLOAD_ProcessGroundCommand — Command code dispatcher
 * --------------------------------------------------------------------------- */
static void ORBITER_PAYLOAD_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t CommandCode = 0U;
    /* MISRA C:2012 Rule 11.3 deviation: cast to command payload struct is
     * required by the cFS SB pattern; size is bounded by the on-wire CCSDS
     * length field validated by cFE before delivery. */
    const ORBITER_PAYLOAD_SetPowerCmd_t       *PowerCmdPtr;
    const ORBITER_PAYLOAD_SetScienceModeCmd_t *ModeCmdPtr;

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &CommandCode);

    switch (CommandCode)
    {
        case ORBITER_PAYLOAD_NOOP_CC:
            ORBITER_PAYLOAD_Data.CmdCounter++;
            CFE_EVS_SendEvent(ORBITER_PAYLOAD_CMD_NOOP_INF_EID,
                              CFE_EVS_EventType_INFORMATION,
                              "PAYLOAD: NOOP, version %d.%d.%d",
                              ORBITER_PAYLOAD_MAJOR_VERSION,
                              ORBITER_PAYLOAD_MINOR_VERSION,
                              ORBITER_PAYLOAD_REVISION);
            break;

        case ORBITER_PAYLOAD_RESET_CC:
            ORBITER_PAYLOAD_Data.CmdCounter = 0U;
            ORBITER_PAYLOAD_Data.ErrCounter = 0U;
            break;

        case ORBITER_PAYLOAD_SET_POWER_CC:
            /* MISRA C:2012 Rule 11.3 deviation: pointer cast to power payload. */
            PowerCmdPtr = (const ORBITER_PAYLOAD_SetPowerCmd_t *)(const void *)SBBufPtr;

            /* Only 0 (OFF) and 1 (ON) are valid; reject all other values. */
            if (PowerCmdPtr->PowerOn > 1U)
            {
                CFE_EVS_SendEvent(ORBITER_PAYLOAD_POWER_INVALID_ERR_EID,
                                  CFE_EVS_EventType_ERROR,
                                  "PAYLOAD: invalid PowerOn=%u rejected",
                                  (unsigned int)PowerCmdPtr->PowerOn);
                ORBITER_PAYLOAD_Data.ErrCounter++;
                break;
            }

            ORBITER_PAYLOAD_PowerState = PowerCmdPtr->PowerOn;
            ORBITER_PAYLOAD_Data.CmdCounter++;
            CFE_EVS_SendEvent(ORBITER_PAYLOAD_POWER_SET_INF_EID,
                              CFE_EVS_EventType_INFORMATION,
                              "PAYLOAD: power %s",
                              (ORBITER_PAYLOAD_PowerState == 1U) ? "ON" : "OFF");
            break;

        case ORBITER_PAYLOAD_SET_SCIENCE_MODE_CC:
            /* MISRA C:2012 Rule 11.3 deviation: pointer cast to science-mode payload. */
            ModeCmdPtr = (const ORBITER_PAYLOAD_SetScienceModeCmd_t *)(const void *)SBBufPtr;

            /* Power-guard: science mode cannot be configured while payload is off. */
            if (ORBITER_PAYLOAD_PowerState == 0U)
            {
                CFE_EVS_SendEvent(ORBITER_PAYLOAD_SCIENCE_MODE_POWER_ERR_EID,
                                  CFE_EVS_EventType_ERROR,
                                  "PAYLOAD: science mode rejected, payload not powered");
                ORBITER_PAYLOAD_Data.ErrCounter++;
                break;
            }

            if (ModeCmdPtr->ScienceMode >= ORBITER_PAYLOAD_MAX_SCIENCE_MODES)
            {
                CFE_EVS_SendEvent(ORBITER_PAYLOAD_SCIENCE_MODE_INVALID_ERR_EID,
                                  CFE_EVS_EventType_ERROR,
                                  "PAYLOAD: invalid science mode %u rejected",
                                  (unsigned int)ModeCmdPtr->ScienceMode);
                ORBITER_PAYLOAD_Data.ErrCounter++;
                break;
            }

            ORBITER_PAYLOAD_ScienceMode = ModeCmdPtr->ScienceMode;
            ORBITER_PAYLOAD_Data.CmdCounter++;
            CFE_EVS_SendEvent(ORBITER_PAYLOAD_SCIENCE_MODE_SET_INF_EID,
                              CFE_EVS_EventType_INFORMATION,
                              "PAYLOAD: science mode set to %u",
                              (unsigned int)ORBITER_PAYLOAD_ScienceMode);
            break;

        default:
            CFE_EVS_SendEvent(ORBITER_PAYLOAD_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "PAYLOAD: unknown command code 0x%02X",
                              (unsigned int)CommandCode);
            ORBITER_PAYLOAD_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * ORBITER_PAYLOAD_ProcessPayloadData — Ingest payload gateway HK (stub until Phase 35)
 *
 * Phase 35 will populate instrument status from MCU_PAYLOAD_HK_MID payload.
 * Until then this is a deliberate no-op.
 * --------------------------------------------------------------------------- */
static void ORBITER_PAYLOAD_ProcessPayloadData(void)
{
    /* Intentional no-op: mcu_payload_gw not yet present (Phase 35 soft dependency). */
}

/* ---------------------------------------------------------------------------
 * ORBITER_PAYLOAD_SendHkPacket — Publish payload HK telemetry
 * --------------------------------------------------------------------------- */
static void ORBITER_PAYLOAD_SendHkPacket(void)
{
    ORBITER_PAYLOAD_Data.HkTlm.PowerState   = ORBITER_PAYLOAD_PowerState;
    ORBITER_PAYLOAD_Data.HkTlm.ScienceMode  = ORBITER_PAYLOAD_ScienceMode;
    ORBITER_PAYLOAD_Data.HkTlm.CmdCounter   = ORBITER_PAYLOAD_Data.CmdCounter;
    ORBITER_PAYLOAD_Data.HkTlm.ErrCounter   = ORBITER_PAYLOAD_Data.ErrCounter;
    /* Q-F4: time_suspect always 0 until Phase 43 propagation lands. */
    ORBITER_PAYLOAD_Data.HkTlm.TimeSuspect  = 0U;

    CFE_SB_TransmitMsg(&ORBITER_PAYLOAD_Data.HkTlm.Header, true);
}
