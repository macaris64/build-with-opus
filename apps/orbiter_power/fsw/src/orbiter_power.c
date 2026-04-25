/*
 * orbiter_power.c — SAKURA-II Orbiter Electrical Power System Application
 *
 * Aggregates EPS telemetry from mcu_eps_gw (APID 0x130–0x13F; stub until Phase 35)
 * and enforces safety-interlocked load switching on ORBITER_POWER_CMD_MID (0x1984).
 * A load-switch TC is rejected when InterlockTable[LoadId].ProhibitMask has the bit
 * set for the current power mode; otherwise the switch is applied immediately.
 *
 * Source of truth: docs/architecture/01-orbiter-cfs.md
 *                  docs/interfaces/apid-registry.md (TM 0x130–0x13F, TC 0x184)
 *
 * MISRA C:2012 compliance target. Deviations documented inline.
 * No dynamic memory allocation. Stack depth statically bounded.
 * [Q-F3] CurrentMode and LoadState are radiation-sensitive; anchored to .critical_mem.
 * [Q-F4] time_suspect propagation deferred to Phase 43.
 */

#include "orbiter_power.h"

/* Single application-global state object — static to restrict linkage */
static ORBITER_POWER_Data_t ORBITER_POWER_Data;

/* Q-F3: power mode register is radiation-sensitive; pinned to .critical_mem.
 * MISRA C:2012 Rule 8.11 deviation: static linkage is intentional;
 * section attribute requires file-scope placement outside the struct.
 * See docs/architecture/09-failure-and-radiation.md §5.1 and [Q-F3]. */
static uint8 ORBITER_POWER_CurrentMode
    __attribute__((section(".critical_mem"))) = ORBITER_POWER_MODE_NORMAL;

/* Q-F3: per-load switch state is radiation-sensitive; pinned to .critical_mem.
 * MISRA C:2012 Rule 8.11 deviation: same rationale as above. */
static uint8 ORBITER_POWER_LoadState[ORBITER_POWER_MAX_LOADS]
    __attribute__((section(".critical_mem"))) = {0U, 0U, 0U, 0U};

/*
 * Safety interlock table — one row per load.
 *
 * ProhibitMask bit N set means load switching is prohibited when CurrentMode == N.
 * Load 0: unrestricted in all modes.
 * Load 1: prohibited in SAFE (mode 1).
 * Load 2: prohibited in SAFE (mode 1) and ECLIPSE (mode 2) — high-current draw.
 * Load 3: prohibited in SAFE (mode 1).
 */
static const ORBITER_POWER_InterlockRow_t ORBITER_POWER_InterlockTable[ORBITER_POWER_MAX_LOADS] =
{
    {0U, 0U,                                                           {0U, 0U}},
    {1U, (uint8)(1U << ORBITER_POWER_MODE_SAFE),                       {0U, 0U}},
    {2U, (uint8)((1U << ORBITER_POWER_MODE_SAFE) |
                 (1U << ORBITER_POWER_MODE_ECLIP)),                    {0U, 0U}},
    {3U, (uint8)(1U << ORBITER_POWER_MODE_SAFE),                       {0U, 0U}},
};

/* Forward declarations */
static int32 ORBITER_POWER_Init(void);
static void  ORBITER_POWER_ProcessCommandPacket(const CFE_SB_Buffer_t *SBBufPtr);
static void  ORBITER_POWER_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr);
static void  ORBITER_POWER_ProcessEpsData(void);
static void  ORBITER_POWER_SendHkPacket(void);

/* ---------------------------------------------------------------------------
 * ORBITER_POWER_AppMain — Application entry point
 * --------------------------------------------------------------------------- */
void ORBITER_POWER_AppMain(void)
{
    int32            status;
    CFE_SB_Buffer_t *SBBufPtr;

    ORBITER_POWER_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    status = ORBITER_POWER_Init();
    if (status != CFE_SUCCESS)
    {
        ORBITER_POWER_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&ORBITER_POWER_Data.RunStatus) == true)
    {
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, ORBITER_POWER_Data.CmdPipe,
                                      CFE_SB_PEND_FOREVER);
        if (status == CFE_SUCCESS)
        {
            ORBITER_POWER_ProcessCommandPacket(SBBufPtr);
        }
        else
        {
            CFE_EVS_SendEvent(ORBITER_POWER_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "POWER: SB receive error, status=0x%08X",
                              (unsigned int)status);
            ORBITER_POWER_Data.ErrCounter++;
        }
    }

    CFE_ES_ExitApp(ORBITER_POWER_Data.RunStatus);
}

/* ---------------------------------------------------------------------------
 * ORBITER_POWER_Init — One-time application initialization
 * --------------------------------------------------------------------------- */
static int32 ORBITER_POWER_Init(void)
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

    status = CFE_SB_CreatePipe(&ORBITER_POWER_Data.CmdPipe, ORBITER_POWER_PIPE_DEPTH,
                               "POWER_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(ORBITER_POWER_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "POWER: pipe creation failed, status=0x%08X",
                          (unsigned int)status);
        return status;
    }

    /* Subscribe to inbound TC commands */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_POWER_CMD_MID),
                              ORBITER_POWER_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Subscribe to HK request (SCH app triggers HK publication) */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_POWER_HK_MID),
                              ORBITER_POWER_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Subscribe to EPS gateway HK — stub ingress until Phase 35 */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(MCU_EPS_HK_MID),
                              ORBITER_POWER_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    ORBITER_POWER_Data.CmdCounter = 0U;
    ORBITER_POWER_Data.ErrCounter = 0U;

    CFE_EVS_SendEvent(ORBITER_POWER_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "POWER initialized, version %d.%d.%d",
                      ORBITER_POWER_MAJOR_VERSION, ORBITER_POWER_MINOR_VERSION,
                      ORBITER_POWER_REVISION);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * ORBITER_POWER_ProcessCommandPacket — Top-level MID dispatcher
 * --------------------------------------------------------------------------- */
static void ORBITER_POWER_ProcessCommandPacket(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    switch (CFE_SB_MsgIdToValue(MsgId))
    {
        case ORBITER_POWER_CMD_MID:
            ORBITER_POWER_ProcessGroundCommand(SBBufPtr);
            break;

        case ORBITER_POWER_HK_MID:
            ORBITER_POWER_SendHkPacket();
            break;

        case MCU_EPS_HK_MID:
            /* Phase 35 soft dependency: update EPS data from gateway.
             * Until mcu_eps_gw lands, this is a no-op that accepts the message. */
            ORBITER_POWER_ProcessEpsData();
            break;

        default:
            CFE_EVS_SendEvent(ORBITER_POWER_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "POWER: unknown MsgId=0x%04X",
                              (unsigned int)CFE_SB_MsgIdToValue(MsgId));
            ORBITER_POWER_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * ORBITER_POWER_ProcessGroundCommand — Command code dispatcher
 * --------------------------------------------------------------------------- */
static void ORBITER_POWER_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t CommandCode = 0U;
    /* MISRA C:2012 Rule 11.3 deviation: cast to command payload struct is
     * required by the cFS SB pattern; size is bounded by the on-wire CCSDS
     * length field validated by cFE before delivery. */
    const ORBITER_POWER_LoadSwitchCmd_t    *SwitchCmdPtr;
    const ORBITER_POWER_SetPowerModeCmd_t  *ModeCmdPtr;
    uint8 ProhibitBit;

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &CommandCode);

    switch (CommandCode)
    {
        case ORBITER_POWER_NOOP_CC:
            ORBITER_POWER_Data.CmdCounter++;
            CFE_EVS_SendEvent(ORBITER_POWER_CMD_NOOP_INF_EID,
                              CFE_EVS_EventType_INFORMATION,
                              "POWER: NOOP, version %d.%d.%d",
                              ORBITER_POWER_MAJOR_VERSION,
                              ORBITER_POWER_MINOR_VERSION,
                              ORBITER_POWER_REVISION);
            break;

        case ORBITER_POWER_RESET_CC:
            ORBITER_POWER_Data.CmdCounter = 0U;
            ORBITER_POWER_Data.ErrCounter = 0U;
            break;

        case ORBITER_POWER_LOAD_SWITCH_CC:
            /* MISRA C:2012 Rule 11.3 deviation: pointer cast to load-switch
             * payload; see comment above. */
            SwitchCmdPtr = (const ORBITER_POWER_LoadSwitchCmd_t *)(const void *)SBBufPtr;

            /* Bounds-check: reject unknown load IDs before table access. */
            if (SwitchCmdPtr->LoadId >= ORBITER_POWER_MAX_LOADS)
            {
                CFE_EVS_SendEvent(ORBITER_POWER_LOAD_SWITCH_BOUNDS_ERR_EID,
                                  CFE_EVS_EventType_ERROR,
                                  "POWER: load switch rejected, LoadId=%u out of range",
                                  (unsigned int)SwitchCmdPtr->LoadId);
                ORBITER_POWER_Data.ErrCounter++;
                break;
            }

            /* Safety interlock: reject if current mode is prohibited for this load. */
            ProhibitBit = (uint8)(ORBITER_POWER_InterlockTable[SwitchCmdPtr->LoadId].ProhibitMask
                                  & (uint8)(1U << ORBITER_POWER_CurrentMode));
            if (ProhibitBit != 0U)
            {
                CFE_EVS_SendEvent(ORBITER_POWER_LOAD_SWITCH_PROHIBITED_ERR_EID,
                                  CFE_EVS_EventType_ERROR,
                                  "POWER: load switch prohibited, LoadId=%u mode=%u",
                                  (unsigned int)SwitchCmdPtr->LoadId,
                                  (unsigned int)ORBITER_POWER_CurrentMode);
                ORBITER_POWER_Data.ErrCounter++;
                break;
            }

            ORBITER_POWER_LoadState[SwitchCmdPtr->LoadId] = SwitchCmdPtr->Action;
            ORBITER_POWER_Data.CmdCounter++;
            CFE_EVS_SendEvent(ORBITER_POWER_LOAD_SWITCH_INF_EID,
                              CFE_EVS_EventType_INFORMATION,
                              "POWER: load %u switched %s",
                              (unsigned int)SwitchCmdPtr->LoadId,
                              (SwitchCmdPtr->Action == ORBITER_POWER_LOAD_ON) ? "ON" : "OFF");
            break;

        case ORBITER_POWER_SET_POWER_MODE_CC:
            /* MISRA C:2012 Rule 11.3 deviation: pointer cast to mode payload. */
            ModeCmdPtr = (const ORBITER_POWER_SetPowerModeCmd_t *)(const void *)SBBufPtr;

            if (ModeCmdPtr->ModeId >= ORBITER_POWER_MODE_COUNT)
            {
                CFE_EVS_SendEvent(ORBITER_POWER_MODE_INVALID_ERR_EID,
                                  CFE_EVS_EventType_ERROR,
                                  "POWER: invalid mode %u rejected",
                                  (unsigned int)ModeCmdPtr->ModeId);
                ORBITER_POWER_Data.ErrCounter++;
                break;
            }

            ORBITER_POWER_CurrentMode = ModeCmdPtr->ModeId;
            ORBITER_POWER_Data.CmdCounter++;
            CFE_EVS_SendEvent(ORBITER_POWER_MODE_SET_INF_EID,
                              CFE_EVS_EventType_INFORMATION,
                              "POWER: mode set to %u",
                              (unsigned int)ORBITER_POWER_CurrentMode);
            break;

        default:
            CFE_EVS_SendEvent(ORBITER_POWER_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "POWER: unknown command code 0x%02X",
                              (unsigned int)CommandCode);
            ORBITER_POWER_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * ORBITER_POWER_ProcessEpsData — Ingest EPS gateway HK (stub until Phase 35)
 *
 * Phase 35 will populate load current/voltage readings from MCU_EPS_HK_MID.
 * Until then this is a deliberate no-op.
 * --------------------------------------------------------------------------- */
static void ORBITER_POWER_ProcessEpsData(void)
{
    /* Intentional no-op: mcu_eps_gw not yet present (Phase 35 soft dependency). */
}

/* ---------------------------------------------------------------------------
 * ORBITER_POWER_SendHkPacket — Publish EPS HK telemetry
 * --------------------------------------------------------------------------- */
static void ORBITER_POWER_SendHkPacket(void)
{
    uint8 i;

    ORBITER_POWER_Data.HkTlm.CurrentMode = ORBITER_POWER_CurrentMode;
    for (i = 0U; i < ORBITER_POWER_MAX_LOADS; i++)
    {
        ORBITER_POWER_Data.HkTlm.LoadState[i] = ORBITER_POWER_LoadState[i];
    }
    ORBITER_POWER_Data.HkTlm.CmdCounter  = ORBITER_POWER_Data.CmdCounter;
    ORBITER_POWER_Data.HkTlm.ErrCounter  = ORBITER_POWER_Data.ErrCounter;
    /* Q-F4: time_suspect always 0 until Phase 43 propagation lands. */
    ORBITER_POWER_Data.HkTlm.TimeSuspect = 0U;

    CFE_SB_TransmitMsg(&ORBITER_POWER_Data.HkTlm.Header, true);
}
