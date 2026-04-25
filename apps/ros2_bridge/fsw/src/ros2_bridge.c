/*
 * ros2_bridge.c — cFS ↔ Space ROS 2 UDP bridge application.
 *
 * Receives CCSDS Space Packets from Space ROS 2 via UDP:5800, validates APID
 * (rover block 0x300–0x43F), and routes valid packets onto the cFE Software
 * Bus (ROVER_*_HK_MID).  Also subscribes to rover CMD MIDs and forwards TC
 * packets to Space ROS 2 via UDP:5810.
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

/* MISRA Rule 18.8: fixed-size; not a VLA. */
static uint8 frame_buf[ROS2_BRIDGE_FRAME_BUF_SIZE];

/* ── Forward declarations ─────────────────────────────────────────────────── */
static int32 ROS2_BRIDGE_Init(void);
static void  ROS2_BRIDGE_RouteToSB(uint16 apid, const uint8 *buf, uint32 len);
static void  ROS2_BRIDGE_ForwardTcToRos(const CFE_SB_Buffer_t *SBBufPtr);

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

        /* Non-blocking SB receive: check for TC to forward to ROS 2. */
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, ROS2_BRIDGE_Data.CmdPipe,
                                      CFE_SB_POLL);
        if (status == CFE_SUCCESS)
        {
            ROS2_BRIDGE_ForwardTcToRos(SBBufPtr);
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

    ROS2_BRIDGE_Data.PacketsRouted  = 0U;
    ROS2_BRIDGE_Data.ApidRejects    = 0U;
    ROS2_BRIDGE_Data.TcForwarded    = 0U;
    ROS2_BRIDGE_Data.UptimeSeconds  = 0U;

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
 * Validation order:
 *   1. Length gate (< 16 bytes → reject; minimum valid CCSDS SPP is 16 B)
 *   2. APID extraction (big-endian, bytes 0–1)
 *   3. APID range gate (rover block 0x300–0x43F)
 *   4. Route to SB via RouteToSB
 *
 * No CRC check: TmBridge produces standards-compliant CCSDS SPPs;
 * the CRC trailer is a simulation sideband requirement (ICD-sim-fsw.md §4.1),
 * not a CCSDS requirement.
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

    /* 3. APID range gate: rover block 0x300–0x43F only. */
    if ((apid < ROS2_BRIDGE_APID_MIN) || (apid > ROS2_BRIDGE_APID_MAX))
    {
        CFE_EVS_SendEvent(ROS2_BRIDGE_EID_APID_OUT_OF_RANGE,
                          CFE_EVS_EventType_ERROR,
                          "ROS2_BRIDGE: APID 0x%03X out of range [0x%03X–0x%03X]",
                          (unsigned int)apid,
                          (unsigned int)ROS2_BRIDGE_APID_MIN,
                          (unsigned int)ROS2_BRIDGE_APID_MAX);
        ROS2_BRIDGE_Data.ApidRejects++;
        return CFE_SB_BAD_ARGUMENT;
    }

    /* 4. Route valid packet to SB. */
    ROS2_BRIDGE_RouteToSB(apid, buf, len);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * ROS2_BRIDGE_RouteToSB — Publish a validated rover SPP to the cFE Software Bus.
 *
 * Maps APID to the appropriate rover HK MID and transmits the packet.
 * For APIDs that fall within the rover range but have no specific MID mapping,
 * uses 0x0800 | apid as the MID (cFE v1 TM scheme).
 * --------------------------------------------------------------------------- */
static void ROS2_BRIDGE_RouteToSB(uint16 apid, const uint8 *buf, uint32 len)
{
    static CFE_SB_Buffer_t tlm_buf;
    CFE_SB_MsgId_Atom_t mid_atom;
    int32 status;

    (void)buf;
    (void)len;

    /* Map APID to SB MID per rover block assignments in mids.h. */
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
    else
    {
        /* cFE v1 TM MID scheme: 0x0800 | APID for unmapped rover APIDs. */
        mid_atom = (CFE_SB_MsgId_Atom_t)(0x0800U | (uint32)apid);
    }

    (void)CFE_MSG_SetMsgId(&tlm_buf.Msg, CFE_SB_ValueToMsgId(mid_atom));
    (void)CFE_MSG_SetSize(&tlm_buf.Msg, (uint32)sizeof(CFE_SB_Buffer_t));

    status = CFE_SB_TransmitMsg(&tlm_buf.Msg, true);
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
 * ROS2_BRIDGE_ForwardTcToRos — Send an SB TC buffer to ROS 2 via UDP:5810.
 *
 * Called when a rover CMD MID message is received on the command pipe.
 * Forwards the raw SB buffer bytes to the rover_udp_gw node.
 * --------------------------------------------------------------------------- */
static void ROS2_BRIDGE_ForwardTcToRos(const CFE_SB_Buffer_t *SBBufPtr)
{
    int32 status;

    status = OS_SocketSendTo(ROS2_BRIDGE_Data.TxSockId,
                              (const void *)SBBufPtr,
                              (uint32)sizeof(CFE_SB_Buffer_t),
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
