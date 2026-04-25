/*
 * orbiter_adcs.c — SAKURA-II Orbiter Attitude Determination & Control Application
 *
 * Publishes the on-board attitude quaternion estimate (APID 0x110) and wheel
 * telemetry (APID 0x111; stub zeros until mcu_rwa_gw lands in Phase 35).
 * Accepts a target quaternion TC on ORBITER_ADCS_CMD_MID (0x1982); validates
 * unit-norm within ORBITER_ADCS_QUAT_NORM_TOL before updating the stored target.
 *
 * Source of truth: docs/architecture/01-orbiter-cfs.md
 *                  docs/interfaces/apid-registry.md (TM 0x110–0x11F, TC 0x182)
 *
 * MISRA C:2012 compliance target. Deviations documented inline.
 * No dynamic memory allocation. Stack depth statically bounded.
 * [Q-F3] CurrentQuat is radiation-sensitive; anchored to .critical_mem.
 * [Q-F4] time_suspect propagation deferred to Phase 43.
 */

#include "orbiter_adcs.h"

/* Single application-global state object — static to restrict linkage */
static ORBITER_ADCS_Data_t ORBITER_ADCS_Data;

/* Q-F3: attitude quaternion estimate is radiation-sensitive; pinned to .critical_mem.
 * MISRA C:2012 Rule 8.11 deviation: static linkage is intentional;
 * section attribute requires file-scope placement outside the struct.
 * See docs/architecture/09-failure-and-radiation.md §5.1 and [Q-F3]. */
static ORBITER_ADCS_Quat_t ORBITER_ADCS_CurrentQuat
    __attribute__((section(".critical_mem"))) = {1.0f, 0.0f, 0.0f, 0.0f};

/* Target quaternion — commanded, non-critical; normal static. */
static ORBITER_ADCS_Quat_t ORBITER_ADCS_TargetQuat = {1.0f, 0.0f, 0.0f, 0.0f};

/* Forward declarations */
static int32 ORBITER_ADCS_Init(void);
static void  ORBITER_ADCS_ProcessCommandPacket(const CFE_SB_Buffer_t *SBBufPtr);
static void  ORBITER_ADCS_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr);
static void  ORBITER_ADCS_UpdateWheelData(void);
static void  ORBITER_ADCS_SendHkPacket(void);

/* ---------------------------------------------------------------------------
 * ORBITER_ADCS_AppMain — Application entry point
 * --------------------------------------------------------------------------- */
void ORBITER_ADCS_AppMain(void)
{
    int32            status;
    CFE_SB_Buffer_t *SBBufPtr;

    ORBITER_ADCS_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    status = ORBITER_ADCS_Init();
    if (status != CFE_SUCCESS)
    {
        ORBITER_ADCS_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&ORBITER_ADCS_Data.RunStatus) == true)
    {
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, ORBITER_ADCS_Data.CmdPipe,
                                      CFE_SB_PEND_FOREVER);
        if (status == CFE_SUCCESS)
        {
            ORBITER_ADCS_ProcessCommandPacket(SBBufPtr);
        }
        else
        {
            CFE_EVS_SendEvent(ORBITER_ADCS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "ADCS: SB receive error, status=0x%08X",
                              (unsigned int)status);
            ORBITER_ADCS_Data.ErrCounter++;
        }
    }

    CFE_ES_ExitApp(ORBITER_ADCS_Data.RunStatus);
}

/* ---------------------------------------------------------------------------
 * ORBITER_ADCS_Init — One-time application initialization
 * --------------------------------------------------------------------------- */
static int32 ORBITER_ADCS_Init(void)
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

    status = CFE_SB_CreatePipe(&ORBITER_ADCS_Data.CmdPipe, ORBITER_ADCS_PIPE_DEPTH,
                               "ADCS_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(ORBITER_ADCS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "ADCS: pipe creation failed, status=0x%08X",
                          (unsigned int)status);
        return status;
    }

    /* Subscribe to inbound TC commands */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_ADCS_CMD_MID),
                              ORBITER_ADCS_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Subscribe to HK request (SCH app triggers HK publication) */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_ADCS_HK_MID),
                              ORBITER_ADCS_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Subscribe to RWA gateway HK — stub ingress until Phase 35 */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(MCU_RWA_HK_MID),
                              ORBITER_ADCS_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    ORBITER_ADCS_Data.CmdCounter = 0U;
    ORBITER_ADCS_Data.ErrCounter = 0U;

    CFE_EVS_SendEvent(ORBITER_ADCS_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "ADCS initialized, version %d.%d.%d",
                      ORBITER_ADCS_MAJOR_VERSION, ORBITER_ADCS_MINOR_VERSION,
                      ORBITER_ADCS_REVISION);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * ORBITER_ADCS_ProcessCommandPacket — Top-level MID dispatcher
 * --------------------------------------------------------------------------- */
static void ORBITER_ADCS_ProcessCommandPacket(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    switch (CFE_SB_MsgIdToValue(MsgId))
    {
        case ORBITER_ADCS_CMD_MID:
            ORBITER_ADCS_ProcessGroundCommand(SBBufPtr);
            break;

        case ORBITER_ADCS_HK_MID:
            ORBITER_ADCS_SendHkPacket();
            break;

        case MCU_RWA_HK_MID:
            /* Phase 35 soft dependency: update wheel data from gateway.
             * Until mcu_rwa_gw lands, this is a no-op that accepts the message. */
            ORBITER_ADCS_UpdateWheelData();
            break;

        default:
            CFE_EVS_SendEvent(ORBITER_ADCS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "ADCS: unknown MsgId=0x%04X",
                              (unsigned int)CFE_SB_MsgIdToValue(MsgId));
            ORBITER_ADCS_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * ORBITER_ADCS_ProcessGroundCommand — Command code dispatcher
 * --------------------------------------------------------------------------- */
static void ORBITER_ADCS_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t CommandCode = 0U;
    /* MISRA C:2012 Rule 11.3 deviation: cast to command payload struct is
     * required by the cFS SB pattern; size is bounded by the on-wire CCSDS
     * length field validated by cFE before delivery. */
    const ORBITER_ADCS_SetTargetQuatCmd_t *QuatCmdPtr;
    float norm_sq;

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &CommandCode);

    switch (CommandCode)
    {
        case ORBITER_ADCS_NOOP_CC:
            ORBITER_ADCS_Data.CmdCounter++;
            CFE_EVS_SendEvent(ORBITER_ADCS_CMD_NOOP_INF_EID,
                              CFE_EVS_EventType_INFORMATION,
                              "ADCS: NOOP, version %d.%d.%d",
                              ORBITER_ADCS_MAJOR_VERSION,
                              ORBITER_ADCS_MINOR_VERSION,
                              ORBITER_ADCS_REVISION);
            break;

        case ORBITER_ADCS_RESET_CC:
            ORBITER_ADCS_Data.CmdCounter = 0U;
            ORBITER_ADCS_Data.ErrCounter = 0U;
            break;

        case ORBITER_ADCS_SET_TARGET_QUAT_CC:
            /* MISRA C:2012 Rule 11.3 deviation: pointer cast to access
             * quaternion payload; see comment above. */
            QuatCmdPtr = (const ORBITER_ADCS_SetTargetQuatCmd_t *)(const void *)SBBufPtr;

            /* Compute norm squared and validate unit-norm within tolerance.
             * MISRA C:2012 Rule 14.4 deviation: fabsf is a standard C99/C17
             * library function; floating-point arithmetic is required for
             * attitude computation and is justified for this domain. */
            norm_sq = (QuatCmdPtr->Quat.w * QuatCmdPtr->Quat.w)
                    + (QuatCmdPtr->Quat.x * QuatCmdPtr->Quat.x)
                    + (QuatCmdPtr->Quat.y * QuatCmdPtr->Quat.y)
                    + (QuatCmdPtr->Quat.z * QuatCmdPtr->Quat.z);

            if (fabsf(norm_sq - 1.0f) > ORBITER_ADCS_QUAT_NORM_TOL)
            {
                CFE_EVS_SendEvent(ORBITER_ADCS_QUAT_INVALID_ERR_EID,
                                  CFE_EVS_EventType_ERROR,
                                  "ADCS: non-unit quaternion rejected (norm_sq=%.4f)",
                                  (double)norm_sq);
                ORBITER_ADCS_Data.ErrCounter++;
            }
            else
            {
                ORBITER_ADCS_TargetQuat = QuatCmdPtr->Quat;
                ORBITER_ADCS_Data.CmdCounter++;
                CFE_EVS_SendEvent(ORBITER_ADCS_QUAT_ACCEPTED_INF_EID,
                                  CFE_EVS_EventType_INFORMATION,
                                  "ADCS: target quaternion accepted");
            }
            break;

        default:
            CFE_EVS_SendEvent(ORBITER_ADCS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "ADCS: unknown command code 0x%02X",
                              (unsigned int)CommandCode);
            ORBITER_ADCS_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * ORBITER_ADCS_UpdateWheelData — Ingest RWA gateway HK (stub until Phase 35)
 *
 * Phase 35 will populate ORBITER_ADCS_Data.WheelTlm.WheelSpeedRpm[] from
 * the MCU_RWA_HK_MID payload. Until then this is a deliberate no-op.
 * --------------------------------------------------------------------------- */
static void ORBITER_ADCS_UpdateWheelData(void)
{
    /* Intentional no-op: mcu_rwa_gw not yet present (Phase 35 soft dependency).
     * Wheel speeds remain at stub zeros. */
}

/* ---------------------------------------------------------------------------
 * ORBITER_ADCS_SendHkPacket — Publish attitude HK and wheel telemetry
 * --------------------------------------------------------------------------- */
static void ORBITER_ADCS_SendHkPacket(void)
{
    uint32 i;

    /* Populate attitude HK */
    ORBITER_ADCS_Data.HkTlm.CurrentQuat  = ORBITER_ADCS_CurrentQuat;
    ORBITER_ADCS_Data.HkTlm.TargetQuat   = ORBITER_ADCS_TargetQuat;
    ORBITER_ADCS_Data.HkTlm.CmdCounter   = ORBITER_ADCS_Data.CmdCounter;
    ORBITER_ADCS_Data.HkTlm.ErrCounter   = ORBITER_ADCS_Data.ErrCounter;
    /* Q-F4: time_suspect always 0 until Phase 43 propagation lands. */
    ORBITER_ADCS_Data.HkTlm.TimeSuspect  = 0U;

    CFE_SB_TransmitMsg(&ORBITER_ADCS_Data.HkTlm.Header, true);

    /* Populate wheel telemetry (stub zeros until Phase 35) */
    for (i = 0U; i < ORBITER_ADCS_NUM_WHEELS; i++)
    {
        ORBITER_ADCS_Data.WheelTlm.WheelSpeedRpm[i] =
            ORBITER_ADCS_Data.WheelTlm.WheelSpeedRpm[i]; /* retain Phase-35 update */
    }

    CFE_SB_TransmitMsg(&ORBITER_ADCS_Data.WheelTlm.Header, true);
}
