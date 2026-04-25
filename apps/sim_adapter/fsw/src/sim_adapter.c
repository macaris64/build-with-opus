/*
 * sim_adapter.c — SAKURA-II cFS SITL UDP–to–Software-Bus adapter.
 *
 * Polls a UDP socket on port 5700 for CCSDS Space Packets from the
 * fault_injector, validates APID (ICD-sim-fsw.md §4.1) and
 * CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), then routes valid
 * packets to the cFE Software Bus via SIM_INJECT_HK_MID.
 *
 * Entire body guarded by #ifndef CFS_FLIGHT_BUILD — zero symbols in
 * flight image (ICD-sim-fsw.md §5.2).
 *
 * MISRA C:2012 compliance target. No dynamic memory allocation.
 * Stack depth statically bounded.
 */

#ifndef CFS_FLIGHT_BUILD

#include "sim_adapter.h"
#include <string.h>

/* ── File-scope state ────────────────────────────────────────────────────── */
#ifdef UNIT_TEST
/* Non-static so unit tests can inspect counters directly. */
SIM_ADAPTER_AppData_t SIM_ADAPTER_Data;
#else
static SIM_ADAPTER_AppData_t SIM_ADAPTER_Data;
#endif /* UNIT_TEST */

/* MISRA Rule 18.8: fixed-size; not a VLA. */
static uint8 frame_buf[SIM_ADAPTER_FRAME_BUF_SIZE];

/* ── Forward declarations ─────────────────────────────────────────────────── */
static int32 SIM_ADAPTER_Init(void);
static void  SIM_ADAPTER_RouteToSB(uint16 apid, const uint8 *payload,
                                    uint32 payload_len);

#ifndef UNIT_TEST
static int32  SIM_ADAPTER_ProcessUdp(const uint8 *buf, uint32 len);
static uint16 SIM_ADAPTER_Crc16(const uint8 *data, uint16 len);
#endif /* !UNIT_TEST */

/* ---------------------------------------------------------------------------
 * SIM_ADAPTER_AppMain — Application entry point
 * --------------------------------------------------------------------------- */
void SIM_ADAPTER_AppMain(void)
{
    int32 status;

    SIM_ADAPTER_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    status = SIM_ADAPTER_Init();
    if (status != CFE_SUCCESS)
    {
        SIM_ADAPTER_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&SIM_ADAPTER_Data.RunStatus) == true)
    {
        /* Non-blocking poll: returns byte count (>0) or negative error code. */
        int32 bytes_recv = OS_SocketRecvFrom(SIM_ADAPTER_Data.UdpSockId,
                                       frame_buf, SIM_ADAPTER_FRAME_BUF_SIZE,
                                       NULL, 0);
        if (bytes_recv > 0)
        {
            (void)SIM_ADAPTER_ProcessUdp(frame_buf, (uint32)bytes_recv);
        }

        SIM_ADAPTER_Data.UptimeSeconds++;
    }

    CFE_ES_ExitApp(SIM_ADAPTER_Data.RunStatus);
}

/* ---------------------------------------------------------------------------
 * SIM_ADAPTER_Init — One-time application initialization
 * --------------------------------------------------------------------------- */
static int32 SIM_ADAPTER_Init(void)
{
    int32 status;

    status = CFE_ES_RegisterApp();
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_EVS_Register(SIM_ADAPTER_Data.EventFilters,
                               (uint16)SIM_ADAPTER_EVT_COUNT,
                               (uint16)CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_CreatePipe(&SIM_ADAPTER_Data.CmdPipe,
                                (uint16)SIM_ADAPTER_PIPE_DEPTH,
                                "SIM_ADAPTER_CMD");
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(SIM_ADAPTER_CMD_MID),
                               SIM_ADAPTER_Data.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = OS_SocketOpen(&SIM_ADAPTER_Data.UdpSockId,
                            OS_SocketDomain_INET, OS_SocketType_DATAGRAM);
    if (status != OS_SUCCESS)
    {
        CFE_EVS_SendEvent(SIM_ADAPTER_EID_INIT_SOCKET_ERR,
                          CFE_EVS_EventType_ERROR,
                          "SIM_ADAPTER: UDP socket open failed 0x%08X",
                          (unsigned int)status);
        return status;
    }

    status = OS_SocketAddrInit(&SIM_ADAPTER_Data.UdpBindAddr,
                                OS_SocketDomain_INET);
    if (status != OS_SUCCESS)
    {
        return status;
    }

    status = OS_SocketAddrSetPort(&SIM_ADAPTER_Data.UdpBindAddr,
                                   (uint16)SIM_ADAPTER_UDP_PORT);
    if (status != OS_SUCCESS)
    {
        return status;
    }

    status = OS_SocketBind(SIM_ADAPTER_Data.UdpSockId,
                            &SIM_ADAPTER_Data.UdpBindAddr);
    if (status != OS_SUCCESS)
    {
        return status;
    }

    SIM_ADAPTER_Data.PacketsRouted  = 0U;
    SIM_ADAPTER_Data.CrcMismatches  = 0U;
    SIM_ADAPTER_Data.ApidRejects    = 0U;
    SIM_ADAPTER_Data.UptimeSeconds  = 0U;

    CFE_EVS_SendEvent(SIM_ADAPTER_STARTUP_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "SIM_ADAPTER initialized v%d.%d.%d (UDP port %u)",
                      SIM_ADAPTER_MAJOR_VERSION,
                      SIM_ADAPTER_MINOR_VERSION,
                      SIM_ADAPTER_REVISION,
                      (unsigned int)SIM_ADAPTER_UDP_PORT);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * SIM_ADAPTER_Crc16 — CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflect)
 *
 * Identical algorithm to MCU_EPS_GW_Crc16 and spp_crc16() in fault_injector.
 * Canonical check: Crc16("123456789", 9) == 0x29B1.
 * Per ICD-sim-fsw.md §4.1 and Q-C9.
 * --------------------------------------------------------------------------- */
#ifdef UNIT_TEST
uint16 SIM_ADAPTER_Crc16(const uint8 *data, uint16 len)
#else
static uint16 SIM_ADAPTER_Crc16(const uint8 *data, uint16 len)
#endif /* UNIT_TEST */
{
    uint16 crc = SIM_ADAPTER_CRC16_INIT;
    uint16 i;
    uint8  bit;

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint16)((uint16)data[i] << 8U);
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16)((uint16)(crc << 1U) ^ SIM_ADAPTER_CRC16_POLY);
            }
            else
            {
                crc = (uint16)(crc << 1U);
            }
        }
    }
    return crc;
}

/* ---------------------------------------------------------------------------
 * SIM_ADAPTER_ProcessUdp — Validate and dispatch one UDP datagram.
 *
 * Validation order per ICD-sim-fsw.md §4.1:
 *   1. Length gate (< 18 bytes → reject)
 *   2. APID extraction (big-endian, bytes 0–1)
 *   3. APID range gate (0x500–0x57F)
 *   4. CRC-16/CCITT-FALSE over payload bytes (buf[16..len-3])
 *   5. Route valid packet to SB
 * --------------------------------------------------------------------------- */
#ifdef UNIT_TEST
int32 SIM_ADAPTER_ProcessUdp(const uint8 *buf, uint32 len)
#else
static int32 SIM_ADAPTER_ProcessUdp(const uint8 *buf, uint32 len)
#endif /* UNIT_TEST */
{
    uint16 apid;
    uint16 crc_computed;
    uint16 crc_stored;

    /* 1. Length gate: need at least 16-byte header + 2-byte CRC = 18 bytes. */
    if (len < 18U)
    {
        CFE_EVS_SendEvent(SIM_ADAPTER_EID_PACKET_TOO_SHORT,
                          CFE_EVS_EventType_ERROR,
                          "SIM_ADAPTER: datagram too short (%u bytes)",
                          (unsigned int)len);
        SIM_ADAPTER_Data.ApidRejects++;
        return CFE_SB_BAD_ARGUMENT;
    }

    /* 2. APID from CCSDS primary header bytes 0–1 (big-endian, 11-bit field). */
    apid = (uint16)(((uint16)(buf[0U] & 0x07U) << 8U) | (uint16)buf[1U]);

    /* 3. APID range gate: sim sideband block 0x500–0x57F only. */
    if ((apid < SIM_ADAPTER_APID_MIN) || (apid > SIM_ADAPTER_APID_MAX))
    {
        CFE_EVS_SendEvent(SIM_ADAPTER_EID_APID_OUT_OF_RANGE,
                          CFE_EVS_EventType_ERROR,
                          "SIM_ADAPTER: APID 0x%03X out of range [0x%03X–0x%03X]",
                          (unsigned int)apid,
                          (unsigned int)SIM_ADAPTER_APID_MIN,
                          (unsigned int)SIM_ADAPTER_APID_MAX);
        SIM_ADAPTER_Data.ApidRejects++;
        return CFE_SB_BAD_ARGUMENT;
    }

    /* 4. CRC-16/CCITT-FALSE over payload bytes (buf[16..len-3]).
     * Payload length = len - 18 (16-byte header + 2-byte CRC trailer).
     * Stored CRC is big-endian at buf[len-2..len-1]. */
    crc_computed = SIM_ADAPTER_Crc16(buf + 16U, (uint16)(len - 18U));
    crc_stored   = (uint16)(((uint16)buf[len - 2U] << 8U) | (uint16)buf[len - 1U]);

    if (crc_computed != crc_stored)
    {
        CFE_EVS_SendEvent(SIM_ADAPTER_EID_CRC_MISMATCH,
                          CFE_EVS_EventType_ERROR,
                          "SIM_ADAPTER: CRC mismatch APID 0x%03X (got 0x%04X, expected 0x%04X)",
                          (unsigned int)apid,
                          (unsigned int)crc_stored,
                          (unsigned int)crc_computed);
        SIM_ADAPTER_Data.CrcMismatches++;
        return CFE_SB_BAD_ARGUMENT;
    }

    /* 5. Route valid payload to SB. */
    SIM_ADAPTER_RouteToSB(apid, buf + 16U, len - 18U);

    return CFE_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * SIM_ADAPTER_RouteToSB — Publish a validated fault SPP to the cFE Software Bus.
 *
 * Stub routing target is SIM_INJECT_HK_MID; a future phase maps each APID to
 * its own MID once the full fault-injection table is wired (ICD-sim-fsw.md §5).
 * --------------------------------------------------------------------------- */
static void SIM_ADAPTER_RouteToSB(uint16 apid, const uint8 *payload,
                                   uint32 payload_len)
{
    static CFE_SB_Buffer_t tlm_buf;
    int32 status;

    (void)payload;
    (void)payload_len;

    (void)CFE_MSG_SetMsgId(&tlm_buf.Msg, CFE_SB_ValueToMsgId(SIM_INJECT_HK_MID));
    (void)CFE_MSG_SetSize(&tlm_buf.Msg, (uint32)sizeof(CFE_SB_Buffer_t));

    status = CFE_SB_TransmitMsg(&tlm_buf.Msg, true);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(SIM_ADAPTER_EID_SB_TRANSMIT_ERR,
                          CFE_EVS_EventType_ERROR,
                          "SIM_ADAPTER: SB transmit failed 0x%08X APID 0x%03X",
                          (unsigned int)status,
                          (unsigned int)apid);
        return;
    }

    SIM_ADAPTER_Data.PacketsRouted++;
    CFE_EVS_SendEvent(SIM_ADAPTER_EID_FAULT_APPLIED_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "SIM_ADAPTER: fault SPP routed APID 0x%03X total %u",
                      (unsigned int)apid,
                      (unsigned int)SIM_ADAPTER_Data.PacketsRouted);
}

#endif /* !CFS_FLIGHT_BUILD */
