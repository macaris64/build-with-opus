#ifndef ROS2_BRIDGE_EVENTS_H
#define ROS2_BRIDGE_EVENTS_H

/* Event ID constants for ros2_bridge. */

#define ROS2_BRIDGE_STARTUP_INF_EID       1U   /* Initialization complete */
#define ROS2_BRIDGE_EID_PACKET_TOO_SHORT  2U   /* UDP datagram < 16 bytes */
#define ROS2_BRIDGE_EID_APID_OUT_OF_RANGE 3U   /* APID outside 0x300–0x43F */
#define ROS2_BRIDGE_EID_SB_TRANSMIT_ERR   4U   /* CFE_SB_TransmitMsg failure */
#define ROS2_BRIDGE_EID_TC_FORWARD_ERR    5U   /* OS_SocketSendTo TC forward failure */
#define ROS2_BRIDGE_EID_INIT_SOCKET_ERR   6U   /* OSAL socket init failure */

#define ROS2_BRIDGE_EVT_COUNT             6U

#endif /* ROS2_BRIDGE_EVENTS_H */
