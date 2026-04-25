#ifndef SIM_ADAPTER_EVENTS_H
#define SIM_ADAPTER_EVENTS_H

/* Event ID constants for sim_adapter.
 * Source of truth: ICD-sim-fsw.md §6. */

#define SIM_ADAPTER_STARTUP_INF_EID         1U   /* Initialization complete */
#define SIM_ADAPTER_EID_PACKET_TOO_SHORT    2U   /* UDP datagram < 18 bytes */
#define SIM_ADAPTER_EID_BAD_HEADER          3U   /* Primary header version error */
#define SIM_ADAPTER_EID_APID_OUT_OF_RANGE   4U   /* APID outside 0x500–0x57F */
#define SIM_ADAPTER_EID_CRC_MISMATCH        5U   /* CRC-16/CCITT-FALSE mismatch */
#define SIM_ADAPTER_EID_FAULT_APPLIED_INF_EID 6U /* Fault SPP routed to SB */
#define SIM_ADAPTER_EID_UNKNOWN_APID        7U   /* APID in range but unregistered */
#define SIM_ADAPTER_EID_INIT_SOCKET_ERR     8U   /* OSAL UDP socket init failure */
#define SIM_ADAPTER_EID_SB_TRANSMIT_ERR     9U   /* CFE_SB_TransmitMsg failure */

#define SIM_ADAPTER_EVT_COUNT               9U

#endif /* SIM_ADAPTER_EVENTS_H */
