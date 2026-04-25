#ifndef ROS2_BRIDGE_H
#define ROS2_BRIDGE_H

/*
 * ros2_bridge.h — cFS ↔ Space ROS 2 UDP bridge application.
 *
 * Bridges rover HK packets from Space ROS 2 (UDP:5800) onto the cFE Software
 * Bus (ROVER_*_HK_MID), and forwards rover TC packets from the SB back to
 * ROS 2 (UDP:5810).
 *
 * Link 2 (Space ROS → Ground): ROS 2 → UDP:5800 → ros2_bridge →
 *   ROVER_*_HK_MID → orbiter_comm → UDP:10000 → ground station
 * Link 3 (cFS → Space ROS TC): ground station → UDP TC → orbiter_comm →
 *   ROVER_*_CMD_MID → ros2_bridge → UDP:5810 → ROS 2
 *
 * MISRA C:2012 compliance target. No dynamic memory allocation.
 * Stack depth statically bounded.
 */

#include "cfe.h"
#include "ros2_bridge_events.h"
#include "ros2_bridge_version.h"
#include "mids.h"

/* ── Constants ─────────────────────────────────────────────────────────────── */
#define ROS2_BRIDGE_RX_PORT        5800U           /* inbound CCSDS SPPs from ROS 2 */
#define ROS2_BRIDGE_TX_PORT        5810U           /* outbound TC SPPs to ROS 2 */
#define ROS2_BRIDGE_ROS_HOST       "127.0.0.1"
#define ROS2_BRIDGE_APID_MIN       0x300U          /* first rover APID */
#define ROS2_BRIDGE_APID_MAX       0x43FU          /* last rover APID */
#define ROS2_BRIDGE_LINK_STATE_APID    0x129U      /* Prx-1 link-state SPP from ROS 2 */
#define ROS2_BRIDGE_FLEET_MONITOR_APID 0x160U      /* fleet_monitor heartbeat from ROS 2 */
#define ROS2_BRIDGE_PIPE_DEPTH     20U
#define ROS2_BRIDGE_FRAME_BUF_SIZE 1024U           /* max UDP datagram; MISRA Rule 18.8 */

/* ── Command codes (TC MID: ROS2_BRIDGE_CMD_MID = 0x1928) ────────────────── */
#define ROS2_BRIDGE_NOOP_CC  ((CFE_MSG_FcnCode_t)0U)
#define ROS2_BRIDGE_RESET_CC ((CFE_MSG_FcnCode_t)1U)

/* ── HK telemetry packet (APID 0x128, MID ROS2_BRIDGE_HK_MID = 0x0928) ───── */
typedef struct {
    CFE_MSG_Message_t Header;
    uint32            PacketsRouted;
    uint32            ApidRejects;
    uint32            TcForwarded;
    uint32            UptimeSeconds;
    uint32            CmdCounter;
    uint32            ErrCounter;
} ROS2_BRIDGE_HkTlm_t;

/* ── Application state ─────────────────────────────────────────────────────── */
typedef struct {
    CFE_EVS_BinFilter_t  EventFilters[ROS2_BRIDGE_EVT_COUNT];
    CFE_SB_PipeId_t      CmdPipe;
    osal_id_t            RxSockId;       /* receives HK SPPs from ROS 2 */
    OS_SockAddr_t        RxBindAddr;
    osal_id_t            TxSockId;       /* sends TC packets to ROS 2 */
    OS_SockAddr_t        TxRemoteAddr;
    uint32               PacketsRouted;
    uint32               ApidRejects;
    uint32               TcForwarded;
    uint32               CmdCounter;
    uint32               ErrCounter;
    uint32               RunStatus;
    uint32               UptimeSeconds;
    ROS2_BRIDGE_HkTlm_t HkTlm;
} ROS2_BRIDGE_AppData_t;

/* ── Entry point ──────────────────────────────────────────────────────────── */
void ROS2_BRIDGE_AppMain(void);

/* ── Unit-test surface (non-static under UNIT_TEST) ─────────────────────── */
#ifdef UNIT_TEST
extern ROS2_BRIDGE_AppData_t ROS2_BRIDGE_Data;
int32 ROS2_BRIDGE_ProcessUdp(const uint8 *buf, uint32 len);
#endif /* UNIT_TEST */

#endif /* ROS2_BRIDGE_H */
