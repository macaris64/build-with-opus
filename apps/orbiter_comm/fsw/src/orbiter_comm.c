/*
 * orbiter_comm.c — SAKURA-II Orbiter Communications Application
 *
 * Radio manager: drives AOS VC 0/1/2/3 downlink emission, tracks link budget,
 * processes downlink rate control (TC 0x1983/CC2) and Time Correlation Packets
 * (TC 0x1983/CC3).  Manages a CFDP Class 1 transaction table (max 16 entries).
 *
 * Source of truth: docs/architecture/01-orbiter-cfs.md §3.2 (orbiter_comm);
 *                  docs/architecture/07-comms-stack.md §3 (AOS VC), §5 (CFDP);
 *                  docs/interfaces/ICD-orbiter-ground.md §2.2 (rate budget), §4, §5.
 *
 * MISRA C:2012 compliance target. Deviations documented inline.
 * No dynamic memory allocation. Stack depth statically bounded.
 * [Q-C8] RttProbeId endianness: consumed via cfs_bindings struct; no raw from_be_bytes.
 * [Q-F3] CFDP table and LinkState are radiation-sensitive; anchored to .critical_mem.
 * [Q-F4] TimeSuspect propagation deferred to Phase 43.
 */

#include "orbiter_comm.h"

/* Single application-global state object — static to restrict linkage */
static ORBITER_COMM_Data_t ORBITER_COMM_Data;

/* Phase D: UDP socket handle and address for ground station AOS frame output.
 * Opened once in Init(); used by ORBITER_COMM_EmitAosFrame() during AOS link state.
 * MISRA C:2012 Rule 8.11 deviation: static linkage is intentional (file-scope state). */
static osal_id_t   ORBITER_COMM_GsSockId;
static OS_SockAddr_t ORBITER_COMM_GsAddr;

/* Q-F3: CFDP transaction table is radiation-sensitive; pinned to .critical_mem.
 * MISRA C:2012 Rule 8.11 deviation: static linkage is intentional;
 * section attribute requires file-scope placement outside the struct.
 * See docs/architecture/09-failure-and-radiation.md §5.1 and [Q-F3]. */
static ORBITER_COMM_CfdpTxn_t ORBITER_COMM_CfdpTable[ORBITER_COMM_MAX_CFDP_TXNS]
    __attribute__((section(".critical_mem"))) = {
        {0U, 0U, 0U, ORBITER_COMM_CFDP_STATE_IDLE, {0U, 0U, 0U}}
    };

/* Q-F3: link state (AOS/LOS) is radiation-sensitive; pinned to .critical_mem.
 * MISRA C:2012 Rule 8.11 deviation: static linkage is intentional;
 * section attribute requires file-scope placement outside the struct.
 * See docs/architecture/09-failure-and-radiation.md §5.1 and [Q-F3]. */
static uint8 ORBITER_COMM_LinkState
    __attribute__((section(".critical_mem"))) = ORBITER_COMM_LINK_LOS;

/* Forward declarations */
static int32 ORBITER_COMM_Init(void);
static void  ORBITER_COMM_ProcessCommandPacket(const CFE_SB_Buffer_t *SBBufPtr);
static void  ORBITER_COMM_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr);
static void  ORBITER_COMM_SendHkPacket(void);
static uint8 ORBITER_COMM_CountActiveTxns(void);
static void  ORBITER_COMM_EmitAosFrame(const uint8 *payload, uint16 len);

/* ---------------------------------------------------------------------------
 * ORBITER_COMM_AppMain — Application entry point
 * --------------------------------------------------------------------------- */
void ORBITER_COMM_AppMain(void)
{
    int32            status;
    CFE_SB_Buffer_t *SBBufPtr;

    ORBITER_COMM_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    status = ORBITER_COMM_Init();
    if (status != CFE_SUCCESS)
    {
        ORBITER_COMM_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&ORBITER_COMM_Data.RunStatus) == true)
    {
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, ORBITER_COMM_Data.CmdPipe,
                                      CFE_SB_PEND_FOREVER);
        if (status == CFE_SUCCESS)
        {
            ORBITER_COMM_ProcessCommandPacket(SBBufPtr);
        }
        else
        {
            CFE_EVS_SendEvent(ORBITER_COMM_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "COMM: SB receive error, status=0x%08X",
                              (unsigned int)status);
            ORBITER_COMM_Data.ErrCounter++;
        }
    }

    CFE_ES_ExitApp(ORBITER_COMM_Data.RunStatus);
}

/* ---------------------------------------------------------------------------
 * ORBITER_COMM_Init — One-time application initialization
 * --------------------------------------------------------------------------- */
static int32 ORBITER_COMM_Init(void)
{
    int32 status;
    uint8 i;

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

    status = CFE_SB_CreatePipe(&ORBITER_COMM_Data.CmdPipe, ORBITER_COMM_PIPE_DEPTH,
                               "COMM_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(ORBITER_COMM_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "COMM: pipe creation failed, status=0x%08X",
                          (unsigned int)status);
        return status;
    }

    /* Subscribe to inbound TC commands */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_COMM_CMD_MID),
                              ORBITER_COMM_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Subscribe to HK request (SCH app triggers HK publication at 1 Hz) */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ORBITER_COMM_HK_MID),
                              ORBITER_COMM_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Initialise application counters */
    ORBITER_COMM_Data.CmdCounter      = 0U;
    ORBITER_COMM_Data.ErrCounter      = 0U;
    ORBITER_COMM_Data.UplinkIdleCount = 0U;

    /* Initialise radiation-anchored link state to LOS until first TC arrives */
    ORBITER_COMM_LinkState = ORBITER_COMM_LINK_LOS;

    /* Initialise CFDP transaction table to IDLE */
    for (i = 0U; i < ORBITER_COMM_MAX_CFDP_TXNS; i++)
    {
        ORBITER_COMM_CfdpTable[i].State         = ORBITER_COMM_CFDP_STATE_IDLE;
        ORBITER_COMM_CfdpTable[i].TransactionId = 0U;
        ORBITER_COMM_CfdpTable[i].BytesReceived = 0U;
        ORBITER_COMM_CfdpTable[i].TotalFileSize = 0U;
    }

    /* Set default per-VC downlink rates from ICD-orbiter-ground.md §2.2 */
    ORBITER_COMM_Data.HkTlm.DownlinkRateKbps[0] = ORBITER_COMM_DEFAULT_RATE_VC0;
    ORBITER_COMM_Data.HkTlm.DownlinkRateKbps[1] = ORBITER_COMM_DEFAULT_RATE_VC1;
    ORBITER_COMM_Data.HkTlm.DownlinkRateKbps[2] = ORBITER_COMM_DEFAULT_RATE_VC2;
    ORBITER_COMM_Data.HkTlm.DownlinkRateKbps[3] = ORBITER_COMM_DEFAULT_RATE_VC3;

    ORBITER_COMM_Data.HkTlm.RttProbeIdEcho = 0U;
    ORBITER_COMM_Data.HkTlm.CfdpBytesTotal = 0U;

    /* Phase D: open UDP socket for ground station AOS frame output.
     * Non-fatal: COMM continues normally if socket init fails — GS output
     * is a diagnostic aid, not a flight-critical path. */
    if (OS_SocketOpen(&ORBITER_COMM_GsSockId, OS_SocketDomain_INET, OS_SocketType_DATAGRAM) == OS_SUCCESS)
    {
        if ((OS_SocketAddrInit(&ORBITER_COMM_GsAddr, OS_SocketDomain_INET)       == OS_SUCCESS) &&
            (OS_SocketAddrFromString(&ORBITER_COMM_GsAddr, ORBITER_COMM_GS_HOST) == OS_SUCCESS) &&
            (OS_SocketAddrSetPort(&ORBITER_COMM_GsAddr, ORBITER_COMM_GS_PORT)    == OS_SUCCESS))
        {
            CFE_EVS_SendEvent(ORBITER_COMM_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "COMM: GS UDP ready %s:%u",
                              ORBITER_COMM_GS_HOST, (unsigned int)ORBITER_COMM_GS_PORT);
        }
    }

    CFE_EVS_SendEvent(ORBITER_COMM_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "COMM initialized, version %d.%d.%d",
                      ORBITER_COMM_MAJOR_VERSION, ORBITER_COMM_MINOR_VERSION,
                      ORBITER_COMM_REVISION);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * ORBITER_COMM_ProcessCommandPacket — Top-level MID dispatcher
 * --------------------------------------------------------------------------- */
static void ORBITER_COMM_ProcessCommandPacket(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    switch (CFE_SB_MsgIdToValue(MsgId))
    {
        case ORBITER_COMM_CMD_MID:
            ORBITER_COMM_ProcessGroundCommand(SBBufPtr);
            break;

        case ORBITER_COMM_HK_MID:
            ORBITER_COMM_SendHkPacket();
            break;

        default:
            CFE_EVS_SendEvent(ORBITER_COMM_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "COMM: unknown MsgId=0x%04X",
                              (unsigned int)CFE_SB_MsgIdToValue(MsgId));
            ORBITER_COMM_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * ORBITER_COMM_ProcessGroundCommand — Command code dispatcher
 * --------------------------------------------------------------------------- */
static void ORBITER_COMM_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t CommandCode = 0U;
    /* MISRA C:2012 Rule 11.3 deviation: cast to command payload struct is
     * required by the cFS SB pattern; size is bounded by the on-wire CCSDS
     * length field validated by cFE before delivery. */
    const ORBITER_COMM_SetDownlinkRateCmd_t *RateCmdPtr;
    const ORBITER_COMM_ProcessTcpCmd_t      *TcpCmdPtr;

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &CommandCode);

    /* Any received TC resets the uplink idle counter and asserts AOS.
     * This covers NOOP, RESET, SET_DOWNLINK_RATE, PROCESS_TCP, and unknown CCs. */
    ORBITER_COMM_Data.UplinkIdleCount = 0U;
    if (ORBITER_COMM_LinkState != ORBITER_COMM_LINK_AOS)
    {
        ORBITER_COMM_LinkState = ORBITER_COMM_LINK_AOS;
        CFE_EVS_SendEvent(ORBITER_COMM_LINK_STATE_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "COMM: link state AOS");
    }

    switch (CommandCode)
    {
        case ORBITER_COMM_NOOP_CC:
            ORBITER_COMM_Data.CmdCounter++;
            CFE_EVS_SendEvent(ORBITER_COMM_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "COMM: NOOP, version %d.%d.%d",
                              ORBITER_COMM_MAJOR_VERSION,
                              ORBITER_COMM_MINOR_VERSION,
                              ORBITER_COMM_REVISION);
            break;

        case ORBITER_COMM_RESET_CC:
            ORBITER_COMM_Data.CmdCounter = 0U;
            ORBITER_COMM_Data.ErrCounter = 0U;
            break;

        case ORBITER_COMM_SET_DOWNLINK_RATE_CC:
            /* MISRA C:2012 Rule 11.3 deviation: pointer cast to access
             * rate command payload; see comment above. */
            RateCmdPtr = (const ORBITER_COMM_SetDownlinkRateCmd_t *)(const void *)SBBufPtr;

            if (RateCmdPtr->VcId > ORBITER_COMM_MAX_VC_ID)
            {
                CFE_EVS_SendEvent(ORBITER_COMM_VC_INVALID_ERR_EID,
                                  CFE_EVS_EventType_ERROR,
                                  "COMM: invalid VC ID %u (max %u)",
                                  (unsigned int)RateCmdPtr->VcId,
                                  (unsigned int)ORBITER_COMM_MAX_VC_ID);
                ORBITER_COMM_Data.ErrCounter++;
            }
            else if (RateCmdPtr->RateKbps > ORBITER_COMM_MAX_RATE_KBPS)
            {
                CFE_EVS_SendEvent(ORBITER_COMM_RATE_INVALID_ERR_EID,
                                  CFE_EVS_EventType_ERROR,
                                  "COMM: rate %u kbps exceeds ceiling %u kbps",
                                  (unsigned int)RateCmdPtr->RateKbps,
                                  (unsigned int)ORBITER_COMM_MAX_RATE_KBPS);
                ORBITER_COMM_Data.ErrCounter++;
            }
            else
            {
                /* Bounds check on VcId is already enforced above; access is safe. */
                ORBITER_COMM_Data.HkTlm.DownlinkRateKbps[RateCmdPtr->VcId] =
                    RateCmdPtr->RateKbps;
                ORBITER_COMM_Data.CmdCounter++;
                CFE_EVS_SendEvent(ORBITER_COMM_RATE_SET_INF_EID,
                                  CFE_EVS_EventType_INFORMATION,
                                  "COMM: VC %u rate set to %u kbps",
                                  (unsigned int)RateCmdPtr->VcId,
                                  (unsigned int)RateCmdPtr->RateKbps);
            }
            break;

        case ORBITER_COMM_PROCESS_TCP_CC:
            /* MISRA C:2012 Rule 11.3 deviation: pointer cast to access
             * TCP command payload; see comment above. */
            TcpCmdPtr = (const ORBITER_COMM_ProcessTcpCmd_t *)(const void *)SBBufPtr;

            /* Echo the RTT probe ID in the next HK packet for ground-side RTT measurement.
             * Q-C8: RttProbeId is stored as-is from the wire buffer; cfs_bindings
             * resolves the big-endian field before the Rust ground station reads the echo. */
            ORBITER_COMM_Data.HkTlm.RttProbeIdEcho = TcpCmdPtr->RttProbeId;
            ORBITER_COMM_Data.CmdCounter++;
            CFE_EVS_SendEvent(ORBITER_COMM_TCP_ACCEPTED_INF_EID,
                              CFE_EVS_EventType_INFORMATION,
                              "COMM: TCP accepted, stratum=%u probe=0x%08X",
                              (unsigned int)TcpCmdPtr->NtpStratum,
                              (unsigned int)TcpCmdPtr->RttProbeId);
            break;

        default:
            CFE_EVS_SendEvent(ORBITER_COMM_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "COMM: unknown command code 0x%02X",
                              (unsigned int)CommandCode);
            ORBITER_COMM_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * ORBITER_COMM_CountActiveTxns — Count CFDP transactions in ACTIVE state
 * --------------------------------------------------------------------------- */
static uint8 ORBITER_COMM_CountActiveTxns(void)
{
    uint8 count = 0U;
    uint8 i;

    for (i = 0U; i < ORBITER_COMM_MAX_CFDP_TXNS; i++)
    {
        if (ORBITER_COMM_CfdpTable[i].State == ORBITER_COMM_CFDP_STATE_ACTIVE)
        {
            count++;
        }
    }
    return count;
}

/* ---------------------------------------------------------------------------
 * ORBITER_COMM_EmitAosFrame — Send payload to ground station via UDP (Phase D)
 *
 * Wraps OS_SocketSendTo so the emission is bounded and auditable.
 * Called only when ORBITER_COMM_LinkState == ORBITER_COMM_LINK_AOS.
 * --------------------------------------------------------------------------- */
static void ORBITER_COMM_EmitAosFrame(const uint8 *payload, uint16 len)
{
    if (len > (uint16)ORBITER_COMM_AOS_FRAME_LEN)
    {
        return; /* bounds guard: never send more than one AOS frame */
    }
    (void)OS_SocketSendTo(ORBITER_COMM_GsSockId, payload, (uint32)len, &ORBITER_COMM_GsAddr);
}

/* ---------------------------------------------------------------------------
 * ORBITER_COMM_SendHkPacket — Publish comm HK telemetry and update LOS state
 * --------------------------------------------------------------------------- */
static void ORBITER_COMM_SendHkPacket(void)
{
    /* Advance uplink idle counter; declare LOS when threshold crossed.
     * Counter does not wrap: saturate at max uint32 to avoid rollover to 0. */
    if (ORBITER_COMM_Data.UplinkIdleCount < UINT32_MAX)
    {
        ORBITER_COMM_Data.UplinkIdleCount++;
    }

    if ((ORBITER_COMM_Data.UplinkIdleCount >= ORBITER_COMM_LOS_TIMEOUT_CYCLES) &&
        (ORBITER_COMM_LinkState != ORBITER_COMM_LINK_LOS))
    {
        ORBITER_COMM_LinkState = ORBITER_COMM_LINK_LOS;
        CFE_EVS_SendEvent(ORBITER_COMM_LINK_STATE_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "COMM: link state LOS");
    }

    ORBITER_COMM_Data.HkTlm.LinkState       = ORBITER_COMM_LinkState;
    ORBITER_COMM_Data.HkTlm.CfdpActiveTxns  = ORBITER_COMM_CountActiveTxns();
    ORBITER_COMM_Data.HkTlm.CmdCounter      = ORBITER_COMM_Data.CmdCounter;
    ORBITER_COMM_Data.HkTlm.ErrCounter      = ORBITER_COMM_Data.ErrCounter;
    /* Q-F4: TimeSuspect always 0 until Phase 43 propagation lands. */
    ORBITER_COMM_Data.HkTlm.TimeSuspect     = 0U;

    CFE_SB_TransmitMsg(&ORBITER_COMM_Data.HkTlm.Header, true);

    /* Phase D: forward HK snapshot to ground station when link is live */
    if (ORBITER_COMM_LinkState == ORBITER_COMM_LINK_AOS)
    {
        ORBITER_COMM_EmitAosFrame((const uint8 *)&ORBITER_COMM_Data.HkTlm,
                                  (uint16)sizeof(ORBITER_COMM_Data.HkTlm));
    }
}
