/*
 * sample_app.c — SAMPLE cFS Application
 *
 * Demonstrates the canonical cFS application structure:
 *   1. Register with Executive Services
 *   2. Initialize Software Bus pipe and subscriptions
 *   3. Enter the run loop: receive and dispatch command packets
 *   4. Report all status via Event Services (never printf)
 *
 * MISRA C:2012 compliance target. Deviations documented inline.
 * No dynamic memory allocation. Stack depth statically bounded.
 */

#include "sample_app.h"

/* Single application-global state object — static to restrict linkage */
static SAMPLE_APP_Data_t SAMPLE_APP_Data;

/* Forward declarations for internal functions */
static int32 SAMPLE_APP_Init(void);
static void  SAMPLE_APP_ProcessCommandPacket(const CFE_SB_Buffer_t *SBBufPtr);
static void  SAMPLE_APP_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr);

/* ---------------------------------------------------------------------------
 * SAMPLE_APP_AppMain — Application entry point
 * Called by cFE Executive Services after the task is created.
 * Never returns while the app is healthy; exits on RUNSTATUS_APP_EXIT.
 * --------------------------------------------------------------------------- */
void SAMPLE_APP_AppMain(void)
{
    int32            status;
    CFE_SB_Buffer_t *SBBufPtr;

    SAMPLE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    status = SAMPLE_APP_Init();
    if (status != CFE_SUCCESS)
    {
        SAMPLE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&SAMPLE_APP_Data.RunStatus) == true)
    {
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, SAMPLE_APP_Data.CmdPipe, CFE_SB_PEND_FOREVER);

        if (status == CFE_SUCCESS)
        {
            SAMPLE_APP_ProcessCommandPacket(SBBufPtr);
        }
        else
        {
            CFE_EVS_SendEvent(SAMPLE_APP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "SAMPLE APP: SB receive error, status=0x%08X", (unsigned int)status);
            SAMPLE_APP_Data.ErrCounter++;
        }
    }

    CFE_ES_ExitApp(SAMPLE_APP_Data.RunStatus);
}

/* ---------------------------------------------------------------------------
 * SAMPLE_APP_Init — One-time application initialization
 * Returns CFE_SUCCESS or a CFE error code.
 * --------------------------------------------------------------------------- */
static int32 SAMPLE_APP_Init(void)
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

    status = CFE_SB_CreatePipe(&SAMPLE_APP_Data.CmdPipe, SAMPLE_APP_PIPE_DEPTH, "SAMPLE_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(SAMPLE_APP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "SAMPLE APP: pipe creation failed, status=0x%08X", (unsigned int)status);
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(SAMPLE_APP_CMD_MID), SAMPLE_APP_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(SAMPLE_APP_SEND_HK_MID), SAMPLE_APP_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    SAMPLE_APP_Data.CmdCounter = 0U;
    SAMPLE_APP_Data.ErrCounter = 0U;

    CFE_EVS_SendEvent(SAMPLE_APP_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "SAMPLE APP initialized, version %d.%d.%d",
                      SAMPLE_APP_MAJOR_VERSION, SAMPLE_APP_MINOR_VERSION, SAMPLE_APP_REVISION);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * SAMPLE_APP_ProcessCommandPacket — Top-level message dispatcher
 * Routes on message ID; drops unknown MIDs with an error event.
 * --------------------------------------------------------------------------- */
static void SAMPLE_APP_ProcessCommandPacket(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    switch (CFE_SB_MsgIdToValue(MsgId))
    {
        case SAMPLE_APP_CMD_MID:
            SAMPLE_APP_ProcessGroundCommand(SBBufPtr);
            break;

        default:
            CFE_EVS_SendEvent(SAMPLE_APP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "SAMPLE APP: invalid MsgId=0x%04X",
                              (unsigned int)CFE_SB_MsgIdToValue(MsgId));
            SAMPLE_APP_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * SAMPLE_APP_ProcessGroundCommand — Dispatches on command code
 * All unknown command codes increment ErrCounter.
 * --------------------------------------------------------------------------- */
static void SAMPLE_APP_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t CommandCode = 0U;

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &CommandCode);

    switch (CommandCode)
    {
        case SAMPLE_APP_NOOP_CC:
            SAMPLE_APP_Data.CmdCounter++;
            CFE_EVS_SendEvent(SAMPLE_APP_CMD_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "SAMPLE APP: NOOP command, version %d.%d.%d",
                              SAMPLE_APP_MAJOR_VERSION, SAMPLE_APP_MINOR_VERSION,
                              SAMPLE_APP_REVISION);
            break;

        case SAMPLE_APP_RESET_CC:
            SAMPLE_APP_Data.CmdCounter = 0U;
            SAMPLE_APP_Data.ErrCounter = 0U;
            break;

        default:
            CFE_EVS_SendEvent(SAMPLE_APP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "SAMPLE APP: unknown command code 0x%02X", (unsigned int)CommandCode);
            SAMPLE_APP_Data.ErrCounter++;
            break;
    }
}
