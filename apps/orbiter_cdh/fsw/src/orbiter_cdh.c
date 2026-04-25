/*
 * orbiter_cdh.c — SAKURA-II Orbiter Command & Data Handling Application
 *
 * Aggregates housekeeping from all orbiter mission apps, dispatches mode
 * transitions (SAFE/NOMINAL/EMERGENCY), and applies EVS filter commands.
 * Exercises sakura_add_cfs_app() end-to-end as the first mission cFS app.
 *
 * Source of truth: docs/architecture/01-orbiter-cfs.md
 *                  docs/interfaces/apid-registry.md (TC 0x181, TM 0x101)
 *
 * MISRA C:2012 compliance target. Deviations documented inline.
 * No dynamic memory allocation. Stack depth statically bounded.
 */

#include "orbiter_cdh.h"

/* Single application-global state object — static to restrict linkage */
static ORBITER_CDH_Data_t ORBITER_CDH_Data;

/* Q-F3: mode register is radiation-sensitive; pinned to .critical_mem.
 * MISRA C:2012 Rule 8.11 deviation: static linkage is intentional;
 * section attribute requires file-scope placement outside the struct.
 * See docs/architecture/09-failure-and-radiation.md §5.1 and [Q-F3]. */
static uint8 ORBITER_CDH_CurrentMode
    __attribute__((section(".critical_mem"))) = ORBITER_CDH_MODE_SAFE;

/* Forward declarations */
static int32 ORBITER_CDH_Init(void);
static void  ORBITER_CDH_ProcessCommandPacket(const CFE_SB_Buffer_t *SBBufPtr);
static void  ORBITER_CDH_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr);
static void  ORBITER_CDH_UpdatePeerHk(void);
static void  ORBITER_CDH_SendHkPacket(void);

/* ---------------------------------------------------------------------------
 * ORBITER_CDH_AppMain — Application entry point
 * --------------------------------------------------------------------------- */
void ORBITER_CDH_AppMain(void)
{
    int32            status;
    CFE_SB_Buffer_t *SBBufPtr;

    ORBITER_CDH_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    status = ORBITER_CDH_Init();
    if (status != CFE_SUCCESS)
    {
        ORBITER_CDH_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&ORBITER_CDH_Data.RunStatus) == true)
    {
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, ORBITER_CDH_Data.CmdPipe,
                                      CFE_SB_PEND_FOREVER);
        if (status == CFE_SUCCESS)
        {
            ORBITER_CDH_ProcessCommandPacket(SBBufPtr);
        }
        else
        {
            CFE_EVS_SendEvent(ORBITER_CDH_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "CDH: SB receive error, status=0x%08X",
                              (unsigned int)status);
            ORBITER_CDH_Data.ErrCounter++;
        }
    }

    CFE_ES_ExitApp(ORBITER_CDH_Data.RunStatus);
}

/* ---------------------------------------------------------------------------
 * ORBITER_CDH_Init — One-time application initialization
 * --------------------------------------------------------------------------- */
static int32 ORBITER_CDH_Init(void)
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

    status = CFE_SB_CreatePipe(&ORBITER_CDH_Data.CmdPipe, ORBITER_CDH_PIPE_DEPTH,
                               "CDH_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(ORBITER_CDH_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "CDH: pipe creation failed, status=0x%08X",
                          (unsigned int)status);
        return status;
    }

    /* Subscribe to inbound TC commands */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_CDH_CMD_MID),
                              ORBITER_CDH_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Subscribe to CDH HK request (SCH app sends this to trigger HK publication) */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_CDH_HK_MID),
                              ORBITER_CDH_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Subscribe to peer orbiter app HK TM for aggregation */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(SAMPLE_APP_HK_MID),
                              ORBITER_CDH_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_ADCS_HK_MID),
                              ORBITER_CDH_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_COMM_HK_MID),
                              ORBITER_CDH_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_POWER_HK_MID),
                              ORBITER_CDH_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_PAYLOAD_HK_MID),
                              ORBITER_CDH_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    ORBITER_CDH_Data.CmdCounter     = 0U;
    ORBITER_CDH_Data.ErrCounter     = 0U;
    ORBITER_CDH_Data.PeerHkRcvCount = 0U;

    CFE_EVS_SendEvent(ORBITER_CDH_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "CDH initialized, version %d.%d.%d, mode=SAFE",
                      ORBITER_CDH_MAJOR_VERSION, ORBITER_CDH_MINOR_VERSION,
                      ORBITER_CDH_REVISION);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * ORBITER_CDH_ProcessCommandPacket — Top-level MID dispatcher
 * --------------------------------------------------------------------------- */
static void ORBITER_CDH_ProcessCommandPacket(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    switch (CFE_SB_MsgIdToValue(MsgId))
    {
        case ORBITER_CDH_CMD_MID:
            ORBITER_CDH_ProcessGroundCommand(SBBufPtr);
            break;

        case ORBITER_CDH_HK_MID:
            ORBITER_CDH_SendHkPacket();
            break;

        case SAMPLE_APP_HK_MID:
        case ORBITER_ADCS_HK_MID:
        case ORBITER_COMM_HK_MID:
        case ORBITER_POWER_HK_MID:
        case ORBITER_PAYLOAD_HK_MID:
            ORBITER_CDH_UpdatePeerHk();
            break;

        default:
            CFE_EVS_SendEvent(ORBITER_CDH_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "CDH: unknown MsgId=0x%04X",
                              (unsigned int)CFE_SB_MsgIdToValue(MsgId));
            ORBITER_CDH_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * ORBITER_CDH_ProcessGroundCommand — Command code dispatcher
 * --------------------------------------------------------------------------- */
static void ORBITER_CDH_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t CommandCode = 0U;
    /* MISRA C:2012 Rule 11.3 deviation: cast to command payload struct is
     * required by the cFS SB pattern; size is bounded by pipe creation
     * and the on-wire CCSDS length field validated by cFE before delivery. */
    const ORBITER_CDH_ModeTransCmd_t *ModeCmdPtr;

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &CommandCode);

    switch (CommandCode)
    {
        case ORBITER_CDH_NOOP_CC:
            ORBITER_CDH_Data.CmdCounter++;
            CFE_EVS_SendEvent(ORBITER_CDH_CMD_NOOP_INF_EID,
                              CFE_EVS_EventType_INFORMATION,
                              "CDH: NOOP, version %d.%d.%d",
                              ORBITER_CDH_MAJOR_VERSION,
                              ORBITER_CDH_MINOR_VERSION,
                              ORBITER_CDH_REVISION);
            break;

        case ORBITER_CDH_RESET_CC:
            ORBITER_CDH_Data.CmdCounter     = 0U;
            ORBITER_CDH_Data.ErrCounter     = 0U;
            ORBITER_CDH_Data.PeerHkRcvCount = 0U;
            break;

        case ORBITER_CDH_MODE_TRANSITION_CC:
            /* MISRA C:2012 Rule 11.3 deviation: pointer cast to access
             * mode payload field; see comment above. */
            ModeCmdPtr = (const ORBITER_CDH_ModeTransCmd_t *)(const void *)SBBufPtr;
            if (ModeCmdPtr->Mode > ORBITER_CDH_MODE_MAX)
            {
                CFE_EVS_SendEvent(ORBITER_CDH_MODE_INVALID_ERR_EID,
                                  CFE_EVS_EventType_ERROR,
                                  "CDH: invalid mode %u, transition rejected",
                                  (unsigned int)ModeCmdPtr->Mode);
                ORBITER_CDH_Data.ErrCounter++;
            }
            else
            {
                ORBITER_CDH_CurrentMode = ModeCmdPtr->Mode;
                ORBITER_CDH_Data.CmdCounter++;
                CFE_EVS_SendEvent(ORBITER_CDH_MODE_TRANSITION_INF_EID,
                                  CFE_EVS_EventType_INFORMATION,
                                  "CDH: mode transition → %u",
                                  (unsigned int)ORBITER_CDH_CurrentMode);
            }
            break;

        case ORBITER_CDH_EVS_FILTER_CC:
            ORBITER_CDH_Data.CmdCounter++;
            CFE_EVS_SendEvent(ORBITER_CDH_EVS_FILTER_INF_EID,
                              CFE_EVS_EventType_INFORMATION,
                              "CDH: EVS filter command applied");
            break;

        default:
            CFE_EVS_SendEvent(ORBITER_CDH_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "CDH: unknown command code 0x%02X",
                              (unsigned int)CommandCode);
            ORBITER_CDH_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * ORBITER_CDH_UpdatePeerHk — Record arrival of a peer app HK message
 * --------------------------------------------------------------------------- */
static void ORBITER_CDH_UpdatePeerHk(void)
{
    ORBITER_CDH_Data.PeerHkRcvCount++;
}

/* ---------------------------------------------------------------------------
 * ORBITER_CDH_SendHkPacket — Publish combined HK on ORBITER_CDH_HK_MID
 * --------------------------------------------------------------------------- */
static void ORBITER_CDH_SendHkPacket(void)
{
    ORBITER_CDH_Data.HkTlm.CurrentMode    = ORBITER_CDH_CurrentMode;
    ORBITER_CDH_Data.HkTlm.CmdCounter     = ORBITER_CDH_Data.CmdCounter;
    ORBITER_CDH_Data.HkTlm.ErrCounter     = ORBITER_CDH_Data.ErrCounter;
    ORBITER_CDH_Data.HkTlm.PeerHkRcvCount = ORBITER_CDH_Data.PeerHkRcvCount;

    CFE_SB_TransmitMsg(&ORBITER_CDH_Data.HkTlm.Header, true);
}
