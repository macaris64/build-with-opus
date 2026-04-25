#ifndef SIM_ADAPTER_H
#define SIM_ADAPTER_H

/*
 * sim_adapter.h — SITL-only cFS application: UDP fault-SPP → Software Bus adapter.
 *
 * Receives CCSDS Space Packets from the fault_injector via UDP port 5700,
 * validates APID (0x500–0x57F) and CRC-16/CCITT-FALSE, then routes valid
 * packets onto the cFE Software Bus as SIM_INJECT_HK_MID messages.
 *
 * Entire implementation is excluded from flight builds by the
 * CFS_FLIGHT_BUILD compile-time guard (ICD-sim-fsw.md §5.2).
 */

#ifndef CFS_FLIGHT_BUILD

#include "cfe.h"
#include "sim_adapter_events.h"
#include "sim_adapter_version.h"
#include "mids.h"

/* ── Constants ───────────────────────────────────────────────────────────── */
#define SIM_ADAPTER_PIPE_DEPTH      10U
#define SIM_ADAPTER_UDP_PORT        5700U
#define SIM_ADAPTER_FRAME_BUF_SIZE  1024U   /* max UDP datagram; no VLA (MISRA Rule 18.8) */
#define SIM_ADAPTER_APID_MIN        0x500U
#define SIM_ADAPTER_APID_MAX        0x57FU
#define SIM_ADAPTER_CRC16_POLY      0x1021U /* CRC-16/CCITT-FALSE polynomial */
#define SIM_ADAPTER_CRC16_INIT      0xFFFFU /* CRC-16/CCITT-FALSE initial value */

/* ── Application state ───────────────────────────────────────────────────── */
typedef struct {
    CFE_EVS_BinFilter_t EventFilters[SIM_ADAPTER_EVT_COUNT];
    CFE_SB_PipeId_t     CmdPipe;
    osal_id_t           UdpSockId;
    OS_SockAddr_t       UdpBindAddr;
    uint32              PacketsRouted;
    uint32              CrcMismatches;
    uint32              ApidRejects;
    uint32              RunStatus;
    uint32              UptimeSeconds;
} SIM_ADAPTER_AppData_t;

/* ── Entry point ─────────────────────────────────────────────────────────── */
void SIM_ADAPTER_AppMain(void);

/* ── Unit-test surface (non-static under UNIT_TEST) ─────────────────────── */
#ifdef UNIT_TEST
extern SIM_ADAPTER_AppData_t SIM_ADAPTER_Data;
int32  SIM_ADAPTER_ProcessUdp(const uint8 *buf, uint32 len);
uint16 SIM_ADAPTER_Crc16(const uint8 *data, uint16 len);
#endif /* UNIT_TEST */

#endif /* !CFS_FLIGHT_BUILD */

#endif /* SIM_ADAPTER_H */
