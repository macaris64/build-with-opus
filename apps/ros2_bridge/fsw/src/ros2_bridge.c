/*
 * ros2_bridge.c — cFS ↔ Space ROS 2 UDP bridge application.
 *
 * Receives CCSDS Space Packets from Space ROS 2 via UDP:5800, validates APID,
 * and routes valid packets onto the cFE Software Bus.  Also subscribes to
 * rover CMD MIDs and forwards TC packets to Space ROS 2 via UDP:5810.
 *
 * Accepted APID ranges:
 *   0x300–0x43F  rover block (land / UAV / cryobot HK)
 *   0x129        Proximity-1 link-state from LinkStateBridge (ROS_GND_LINK_STATE_MID)
 *   0x160        fleet_monitor intra-fleet DDS heartbeat (FLEET_MONITOR_HK_MID)
 *
 * No CRC check: Space ROS 2 TmBridge produces valid CCSDS packets without an
 * appended CRC trailer.  The sim_adapter CRC was a simulation sideband
 * requirement per ICD-sim-fsw.md §4.1, not a CCSDS requirement.
 *
 * MISRA C:2012 compliance target. No dynamic memory allocation.
 * Stack depth statically bounded.
 */

#include "ros2_bridge.h"
#include <string.h>

/* ── File-scope state ────────────────────────────────────────────────────── */
#ifdef UNIT_TEST
ROS2_BRIDGE_AppData_t ROS2_BRIDGE_Data;
#else
static ROS2_BRIDGE_AppData_t ROS2_BRIDGE_Data;
#endif /* UNIT_TEST */

/* MISRA Rule 18.8: fixed-size inbound receive buffer; not a VLA. */
static uint8 frame_buf[ROS2_BRIDGE_FRAME_BUF_SIZE];

/* MISRA Rule 18.8: fixed-size relay buffer for SB transmission; not a VLA.
 * Union ensures alignment matches CFE_MSG_Message_t requirement. */
static union {
    CFE_MSG_Message_t Msg;
    uint8             Raw[ROS2_BRIDGE_FRAME_BUF_SIZE];
} relay_buf;

/* ── Forward declarations ─────────────────────────────────────────────────── */
static int32 ROS2_BRIDGE_Init(void);
static void  ROS2_BRIDGE_RouteToSB(uint16 apid, const uint8 *buf, uint32 len);
static void  ROS2_BRIDGE_ForwardTcToRos(const CFE_SB_Buffer_t *SBBufPtr);
static void  ROS2_BRIDGE_ProcessSBMessage(const CFE_SB_Buffer_t *SBBufPtr);
static void  ROS2_BRIDGE_SendHkPacket(void);
static void  ROS2_BRIDGE_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr);

#ifndef UNIT_TEST
static int32 ROS2_BRIDGE_ProcessUdp(const uint8 *buf, uint32 len);
#endif /* !UNIT_TEST */

/* ---------------------------------------------------------------------------
 * ROS2_BRIDGE_AppMain — Application entry point
 * --------------------------------------------------------------------------- */
void ROS2_BRIDGE_AppMain(void)
{
    int32            status;
    CFE_SB_Buffer_t *SBBufPtr;

    ROS2_BRIDGE_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    status = ROS2_BRIDGE_Init();
    if (status != CFE_SUCCESS)
    {
        ROS2_BRIDGE_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&ROS2_BRIDGE_Data.RunStatus) == true)
    {
        /* Non-blocking UDP receive: check for inbound HK from ROS 2. */
        int32 bytes_recv = OS_SocketRecvFrom(ROS2_BRIDGE_Data.RxSockId,
                                             frame_buf, ROS2_BRIDGE_FRAME_BUF_SIZE,
                                             NULL, 0);
        if (bytes_recv > 0)
        {
            (void)ROS2_BRIDGE_ProcessUdp(frame_buf, (uint32)bytes_recv);
        }

        /* Non-blocking SB receive: dispatch HK trigger, CMD, or TC forward. */
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, ROS2_BRIDGE_Data.CmdPipe,
                                      CFE_SB_POLL);
        if (status == CFE_SUCCESS)
        {
            ROS2_BRIDGE_ProcessSBMessage(SBBufPtr);
        }

        ROS2_BRIDGE_Data.UptimeSeconds++;
        (void)OS_TaskDelay(1U);
    }

    CFE_ES_ExitApp(ROS2_BRIDGE_Data.RunStatus);
}

/* ---------------------------------------------------------------------------
 * ROS2_BRIDGE_Init — One-time application initialization
 * --------------------------------------------------------------------------- */
static int32 ROS2_BRIDGE_Init(void)
{
    int32 status;

    status = CFE_ES_RegisterApp();
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_EVS_Register(ROS2_BRIDGE_Data.EventFilters,
                               (uint16)ROS2_BRIDGE_EVT_COUNT,
                               (uint16)CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_CreatePipe(&ROS2_BRIDGE_Data.CmdPipe,
                                (uint16)ROS2_BRIDGE_PIPE_DEPTH,
                                "ROS2_BRIDGE_CMD");
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Subscribe to HK trigger: scheduler sends ROS2_BRIDGE_HK_MID at 1 Hz. */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ROS2_BRIDGE_HK_MID),
                               ROS2_BRIDGE_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Subscribe to ground commands (NOOP, RESET). */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ROS2_BRIDGE_CMD_MID),
                               ROS2_BRIDGE_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Subscribe to rover CMD MIDs so TCs can be forwarded to ROS 2. */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ROVER_LAND_CMD_MID),
                               ROS2_BRIDGE_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ROVER_UAV_CMD_MID),
                               ROS2_BRIDGE_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ROVER_CRYOBOT_CMD_MID),
                               ROS2_BRIDGE_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Open RX socket: bound to port 5800 to receive HK from ROS 2. */
    status = OS_SocketOpen(&ROS2_BRIDGE_Data.RxSockId,
                            OS_SocketDomain_INET, OS_SocketType_DATAGRAM);
    if (status != OS_SUCCESS)
    {
        CFE_EVS_SendEvent(ROS2_BRIDGE_EID_INIT_SOCKET_ERR,
                          CFE_EVS_EventType_ERROR,
                          "ROS2_BRIDGE: RX socket open failed 0x%08X",
                          (unsigned int)status);
        return status;
    }

    status = OS_SocketAddrInit(&ROS2_BRIDGE_Data.RxBindAddr, OS_SocketDomain_INET);
    if (status != OS_SUCCESS)
    {
        return status;
    }

    status = OS_SocketAddrSetPort(&ROS2_BRIDGE_Data.RxBindAddr,
                                   (uint16)ROS2_BRIDGE_RX_PORT);
    if (status != OS_SUCCESS)
    {
        return status;
    }

    status = OS_SocketBind(ROS2_BRIDGE_Data.RxSockId, &ROS2_BRIDGE_Data.RxBindAddr);
    if (status != OS_SUCCESS)
    {
        return status;
    }

    /* Open TX socket: used to send TC packets to ROS 2 on port 5810. */
    status = OS_SocketOpen(&ROS2_BRIDGE_Data.TxSockId,
                            OS_SocketDomain_INET, OS_SocketType_DATAGRAM);
    if (status != OS_SUCCESS)
    {
        CFE_EVS_SendEvent(ROS2_BRIDGE_EID_INIT_SOCKET_ERR,
                          CFE_EVS_EventType_ERROR,
                          "ROS2_BRIDGE: TX socket open failed 0x%08X",
                          (unsigned int)status);
        return status;
    }

    status = OS_SocketAddrInit(&ROS2_BRIDGE_Data.TxRemoteAddr, OS_SocketDomain_INET);
    if (status != OS_SUCCESS)
    {
        return status;
    }

    status = OS_SocketAddrFromString(&ROS2_BRIDGE_Data.TxRemoteAddr,
                                      ROS2_BRIDGE_ROS_HOST);
    if (status != OS_SUCCESS)
    {
        return status;
    }

    status = OS_SocketAddrSetPort(&ROS2_BRIDGE_Data.TxRemoteAddr,
                                   (uint16)ROS2_BRIDGE_TX_PORT);
    if (status != OS_SUCCESS)
    {
        return status;
    }

    /* Initialise counters. */
    ROS2_BRIDGE_Data.PacketsRouted  = 0U;
    ROS2_BRIDGE_Data.ApidRejects    = 0U;
    ROS2_BRIDGE_Data.TcForwarded    = 0U;
    ROS2_BRIDGE_Data.CmdCounter     = 0U;
    ROS2_BRIDGE_Data.ErrCounter     = 0U;
    ROS2_BRIDGE_Data.UptimeSeconds  = 0U;

    /* Initialise HK telemetry message header (APID 0x128). */
    (void)CFE_MSG_Init(CFE_SB_ValueToMsgId(ROS2_BRIDGE_HK_MID),
                       &ROS2_BRIDGE_Data.HkTlm.Header,
                       (CFE_MSG_Size_t)sizeof(ROS2_BRIDGE_HkTlm_t));

    CFE_EVS_SendEvent(ROS2_BRIDGE_STARTUP_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "ROS2_BRIDGE initialized v%d.%d.%d (RX port %u TX port %u)",
                      ROS2_BRIDGE_MAJOR_VERSION,
                      ROS2_BRIDGE_MINOR_VERSION,
                      ROS2_BRIDGE_REVISION,
                      (unsigned int)ROS2_BRIDGE_RX_PORT,
                      (unsigned int)ROS2_BRIDGE_TX_PORT);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * ROS2_BRIDGE_ProcessUdp — Validate and dispatch one inbound UDP datagram.
 *
 * Accepted APIDs:
 *   0x300–0x43F  rover block (land / UAV / cryobot HK)
 *   0x129        Proximity-1 link-state (APID ROS2_BRIDGE_LINK_STATE_APID)
 *   0x160        Fleet-monitor DDS heartbeat (APID ROS2_BRIDGE_FLEET_MONITOR_APID)
 * --------------------------------------------------------------------------- */
#ifdef UNIT_TEST
int32 ROS2_BRIDGE_ProcessUdp(const uint8 *buf, uint32 len)
#else
static int32 ROS2_BRIDGE_ProcessUdp(const uint8 *buf, uint32 len)
#endif /* UNIT_TEST */
{
    uint16 apid;

    /* 1. Length gate: 6-byte primary header + 10-byte secondary header = 16 B minimum. */
    if (len < 16U)
    {
        CFE_EVS_SendEvent(ROS2_BRIDGE_EID_PACKET_TOO_SHORT,
                          CFE_EVS_EventType_ERROR,
                          "ROS2_BRIDGE: datagram too short (%u bytes)",
                          (unsigned int)len);
        ROS2_BRIDGE_Data.ApidRejects++;
        return CFE_SB_BAD_ARGUMENT;
    }

    /* 2. APID from CCSDS primary header bytes 0–1 (big-endian, 11-bit field). */
    apid = (uint16)(((uint16)(buf[0U] & 0x07U) << 8U) | (uint16)buf[1U]);

    /* 3. Accept rover block OR explicit orbiter-block SPPs from ROS 2. */
    if (((apid >= (uint16)ROS2_BRIDGE_APID_MIN) && (apid <= (uint16)ROS2_BRIDGE_APID_MAX)) ||
        (apid == (uint16)ROS2_BRIDGE_LINK_STATE_APID) ||
        (apid == (uint16)ROS2_BRIDGE_FLEET_MONITOR_APID))
    {
        ROS2_BRIDGE_RouteToSB(apid, buf, len);
        return CFE_SUCCESS;
    }

    CFE_EVS_SendEvent(ROS2_BRIDGE_EID_APID_OUT_OF_RANGE,
                      CFE_EVS_EventType_ERROR,
                      "ROS2_BRIDGE: APID 0x%03X not in any accepted range",
                      (unsigned int)apid);
    ROS2_BRIDGE_Data.ApidRejects++;
    return CFE_SB_BAD_ARGUMENT;
}

/* ---------------------------------------------------------------------------
 * ROS2_BRIDGE_RouteToSB — Publish a validated SPP to the cFE Software Bus.
 *
 * Copies the inbound CCSDS bytes into a properly-aligned relay buffer, maps
 * APID to SB MID, overwrites the CCSDS primary-header fields, and transmits.
 * --------------------------------------------------------------------------- */
static void ROS2_BRIDGE_RouteToSB(uint16 apid, const uint8 *buf, uint32 len)
{
    CFE_SB_MsgId_Atom_t mid_atom;
    int32               status;

    /* Bounds check: reject datagrams that exceed the relay buffer. */
    if (len > (uint32)ROS2_BRIDGE_FRAME_BUF_SIZE)
    {
        CFE_EVS_SendEvent(ROS2_BRIDGE_EID_SB_TRANSMIT_ERR,
                          CFE_EVS_EventType_ERROR,
                          "ROS2_BRIDGE: datagram too large (%u B) APID 0x%03X",
                          (unsigned int)len,
                          (unsigned int)apid);
        ROS2_BRIDGE_Data.ApidRejects++;
        return;
    }

    /* Copy authentic CCSDS bytes (secondary header + payload preserved). */
    (void)memcpy(relay_buf.Raw, buf, (size_t)len);

    /* Map APID to SB MID per mids.h allocations. */
    if (apid == (uint16)0x300U)
    {
        mid_atom = ROVER_LAND_HK_MID;
    }
    else if (apid == (uint16)0x3C0U)
    {
        mid_atom = ROVER_UAV_HK_MID;
    }
    else if (apid == (uint16)0x400U)
    {
        mid_atom = ROVER_CRYOBOT_HK_MID;
    }
    else if (apid == (uint16)ROS2_BRIDGE_LINK_STATE_APID)
    {
        mid_atom = ROS_GND_LINK_STATE_MID;
    }
    else if (apid == (uint16)ROS2_BRIDGE_FLEET_MONITOR_APID)
    {
        mid_atom = FLEET_MONITOR_HK_MID;
    }
    else
    {
        /* cFE v1 TM MID scheme: 0x0800 | APID for unmapped rover APIDs. */
        mid_atom = (CFE_SB_MsgId_Atom_t)(0x0800U | (uint32)apid);
    }

    /* MISRA C:2012 Rule 11.3 deviation: union member access for type-safe cast;
     * relay_buf.Msg provides correct alignment for CFE_MSG_Message_t operations. */
    (void)CFE_MSG_SetMsgId(&relay_buf.Msg, CFE_SB_ValueToMsgId(mid_atom));
    (void)CFE_MSG_SetSize(&relay_buf.Msg, (CFE_MSG_Size_t)len);

    status = CFE_SB_TransmitMsg(&relay_buf.Msg, true);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(ROS2_BRIDGE_EID_SB_TRANSMIT_ERR,
                          CFE_EVS_EventType_ERROR,
                          "ROS2_BRIDGE: SB transmit failed 0x%08X APID 0x%03X",
                          (unsigned int)status,
                          (unsigned int)apid);
        return;
    }

    ROS2_BRIDGE_Data.PacketsRouted++;
}

/* ---------------------------------------------------------------------------
 * ROS2_BRIDGE_ProcessSBMessage — MID dispatcher for inbound SB messages.
 * --------------------------------------------------------------------------- */
static void ROS2_BRIDGE_ProcessSBMessage(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    switch (CFE_SB_MsgIdToValue(MsgId))
    {
        case ROS2_BRIDGE_HK_MID:
            ROS2_BRIDGE_SendHkPacket();
            break;

        case ROS2_BRIDGE_CMD_MID:
            ROS2_BRIDGE_ProcessGroundCommand(SBBufPtr);
            break;

        case ROVER_LAND_CMD_MID:
        case ROVER_UAV_CMD_MID:
        case ROVER_CRYOBOT_CMD_MID:
            ROS2_BRIDGE_ForwardTcToRos(SBBufPtr);
            break;

        default:
            CFE_EVS_SendEvent(ROS2_BRIDGE_EID_CMD_ERR,
                              CFE_EVS_EventType_ERROR,
                              "ROS2_BRIDGE: unexpected MsgId 0x%04X",
                              (unsigned int)CFE_SB_MsgIdToValue(MsgId));
            ROS2_BRIDGE_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * ROS2_BRIDGE_SendHkPacket — Publish HK telemetry on ROS2_BRIDGE_HK_MID.
 * --------------------------------------------------------------------------- */
static void ROS2_BRIDGE_SendHkPacket(void)
{
    int32 status;

    ROS2_BRIDGE_Data.HkTlm.PacketsRouted = ROS2_BRIDGE_Data.PacketsRouted;
    ROS2_BRIDGE_Data.HkTlm.ApidRejects   = ROS2_BRIDGE_Data.ApidRejects;
    ROS2_BRIDGE_Data.HkTlm.TcForwarded   = ROS2_BRIDGE_Data.TcForwarded;
    ROS2_BRIDGE_Data.HkTlm.UptimeSeconds = ROS2_BRIDGE_Data.UptimeSeconds;
    ROS2_BRIDGE_Data.HkTlm.CmdCounter    = ROS2_BRIDGE_Data.CmdCounter;
    ROS2_BRIDGE_Data.HkTlm.ErrCounter    = ROS2_BRIDGE_Data.ErrCounter;

    status = CFE_SB_TransmitMsg(&ROS2_BRIDGE_Data.HkTlm.Header, true);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(ROS2_BRIDGE_EID_HK_SEND_ERR,
                          CFE_EVS_EventType_ERROR,
                          "ROS2_BRIDGE: HK transmit failed 0x%08X",
                          (unsigned int)status);
    }
}

/* ---------------------------------------------------------------------------
 * ROS2_BRIDGE_ProcessGroundCommand — TC command code dispatcher.
 * --------------------------------------------------------------------------- */
static void ROS2_BRIDGE_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t cc = 0U;

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &cc);

    switch (cc)
    {
        case ROS2_BRIDGE_NOOP_CC:
            ROS2_BRIDGE_Data.CmdCounter++;
            CFE_EVS_SendEvent(ROS2_BRIDGE_EID_CMD_NOOP_INF,
                              CFE_EVS_EventType_INFORMATION,
                              "ROS2_BRIDGE: NOOP accepted");
            break;

        case ROS2_BRIDGE_RESET_CC:
            ROS2_BRIDGE_Data.CmdCounter    = 0U;
            ROS2_BRIDGE_Data.ErrCounter    = 0U;
            ROS2_BRIDGE_Data.PacketsRouted = 0U;
            ROS2_BRIDGE_Data.ApidRejects   = 0U;
            ROS2_BRIDGE_Data.TcForwarded   = 0U;
            break;

        default:
            CFE_EVS_SendEvent(ROS2_BRIDGE_EID_CMD_ERR,
                              CFE_EVS_EventType_ERROR,
                              "ROS2_BRIDGE: unknown command code 0x%02X",
                              (unsigned int)cc);
            ROS2_BRIDGE_Data.ErrCounter++;
            break;
    }
}

/* ---------------------------------------------------------------------------
 * ROS2_BRIDGE_ForwardTcToRos — Send an SB TC buffer to ROS 2 via UDP:5810.
 * --------------------------------------------------------------------------- */
static void ROS2_BRIDGE_ForwardTcToRos(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_Size_t MsgSize = 0U;
    int32          status;

    (void)CFE_MSG_GetSize(&SBBufPtr->Msg, &MsgSize);

    status = OS_SocketSendTo(ROS2_BRIDGE_Data.TxSockId,
                              (const void *)SBBufPtr,
                              (uint32)MsgSize,
                              &ROS2_BRIDGE_Data.TxRemoteAddr);
    if (status < 0)
    {
        CFE_EVS_SendEvent(ROS2_BRIDGE_EID_TC_FORWARD_ERR,
                          CFE_EVS_EventType_ERROR,
                          "ROS2_BRIDGE: TC forward failed 0x%08X",
                          (unsigned int)status);
        return;
    }

    ROS2_BRIDGE_Data.TcForwarded++;
}
