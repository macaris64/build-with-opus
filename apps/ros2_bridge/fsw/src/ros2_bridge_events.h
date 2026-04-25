#ifndef ROS2_BRIDGE_EVENTS_H
#define ROS2_BRIDGE_EVENTS_H

/* Event ID constants for ros2_bridge. */

#define ROS2_BRIDGE_STARTUP_INF_EID       1U   /* Initialization complete */
#define ROS2_BRIDGE_EID_PACKET_TOO_SHORT  2U   /* UDP datagram < 16 bytes */
#define ROS2_BRIDGE_EID_APID_OUT_OF_RANGE 3U   /* APID outside accepted ranges */
#define ROS2_BRIDGE_EID_SB_TRANSMIT_ERR   4U   /* CFE_SB_TransmitMsg failure */
#define ROS2_BRIDGE_EID_TC_FORWARD_ERR    5U   /* OS_SocketSendTo TC forward failure */
#define ROS2_BRIDGE_EID_INIT_SOCKET_ERR   6U   /* OSAL socket init failure */
#define ROS2_BRIDGE_EID_HK_SEND_ERR       7U   /* HK packet transmit failure */
#define ROS2_BRIDGE_EID_CMD_NOOP_INF      8U   /* NOOP command accepted */
#define ROS2_BRIDGE_EID_CMD_ERR           9U   /* Unknown command code or unexpected MID */

#define ROS2_BRIDGE_EVT_COUNT             9U

#endif /* ROS2_BRIDGE_EVENTS_H */
