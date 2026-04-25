/*
 * sim_adapter_test.c — CMocka unit tests for sim_adapter.
 *
 * CFE and OSAL API calls are intercepted via UNIT_TEST guards; no real cFE
 * library is linked. SIM_ADAPTER_ProcessUdp and SIM_ADAPTER_Crc16 are exposed
 * as non-static under UNIT_TEST so tests can call them directly.
 *
 * Coverage target: 100% branch coverage of sim_adapter.c.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "sim_adapter.h"

/* ---------------------------------------------------------------------------
 * CFE / OSAL stub implementations (UNIT_TEST only)
 * --------------------------------------------------------------------------- */
#ifdef UNIT_TEST

int32 CFE_ES_RegisterApp(void) { return (int32)mock(); }

int32 CFE_EVS_Register(const void *F, uint16 N, uint16 S)
{
    (void)F; (void)N; (void)S;
    return (int32)mock();
}

int32 CFE_SB_CreatePipe(CFE_SB_PipeId_t *P, uint16 D, const char *N)
{
    (void)D; (void)N;
    *P = (CFE_SB_PipeId_t)mock();
    return (int32)mock();
}

int32 CFE_SB_Subscribe(CFE_SB_MsgId_t MsgId, CFE_SB_PipeId_t PipeId)
{
    (void)MsgId; (void)PipeId;
    return (int32)mock();
}

void CFE_EVS_SendEvent(uint16 EventID, uint16 EventType, const char *Spec, ...)
{
    (void)EventID; (void)EventType; (void)Spec;
    function_called();
}

bool CFE_ES_RunLoop(uint32 *RunStatus)
{
    (void)RunStatus;
    return (bool)mock();
}

int32 CFE_SB_ReceiveBuffer(CFE_SB_Buffer_t **BufPtr, CFE_SB_PipeId_t PipeId, int32 TimeOut)
{
    (void)PipeId; (void)TimeOut;
    *BufPtr = (CFE_SB_Buffer_t *)mock();
    return (int32)mock();
}

int32 CFE_SB_TransmitMsg(CFE_MSG_Message_t *MsgPtr, bool Inc)
{
    (void)MsgPtr; (void)Inc;
    function_called();
    return (int32)mock();
}

void CFE_ES_ExitApp(uint32 ExitStatus) { (void)ExitStatus; }

int32 CFE_MSG_GetMsgId(const CFE_MSG_Message_t *M, CFE_SB_MsgId_t *Id)
{
    (void)M;
    *Id = (CFE_SB_MsgId_t)mock();
    return CFE_SUCCESS;
}

int32 CFE_MSG_GetFcnCode(const CFE_MSG_Message_t *M, CFE_MSG_FcnCode_t *C)
{
    (void)M;
    *C = (CFE_MSG_FcnCode_t)mock();
    return CFE_SUCCESS;
}

int32 CFE_MSG_SetMsgId(CFE_MSG_Message_t *MsgPtr, CFE_SB_MsgId_t MsgId)
{
    (void)MsgPtr; (void)MsgId;
    return CFE_SUCCESS;
}

int32 CFE_MSG_SetSize(CFE_MSG_Message_t *MsgPtr, uint32 TotalMsgSize)
{
    (void)MsgPtr; (void)TotalMsgSize;
    return CFE_SUCCESS;
}

CFE_SB_MsgId_Atom_t CFE_SB_MsgIdToValue(CFE_SB_MsgId_t MsgId)
{
    return (CFE_SB_MsgId_Atom_t)MsgId;
}

CFE_SB_MsgId_t CFE_SB_ValueToMsgId(CFE_SB_MsgId_Atom_t MsgIdValue)
{
    return (CFE_SB_MsgId_t)MsgIdValue;
}

int32 OS_SocketOpen(osal_id_t *sock_id, OS_SocketDomain_t Domain, OS_SocketType_t Type)
{
    (void)Domain; (void)Type;
    *sock_id = (osal_id_t)mock();
    return (int32)mock();
}

int32 OS_SocketAddrInit(OS_SockAddr_t *Addr, OS_SocketDomain_t Domain)
{
    (void)Addr; (void)Domain;
    return (int32)mock();
}

int32 OS_SocketAddrSetPort(OS_SockAddr_t *Addr, uint16 PortNum)
{
    (void)Addr; (void)PortNum;
    return (int32)mock();
}

int32 OS_SocketBind(osal_id_t sock_id, const OS_SockAddr_t *Addr)
{
    (void)sock_id; (void)Addr;
    return (int32)mock();
}

int32 OS_SocketRecvFrom(osal_id_t sock_id, void *buf, uint32 buflen,
                        OS_SockAddr_t *RemoteAddr, int32 timeout_ms)
{
    (void)sock_id; (void)buf; (void)buflen; (void)RemoteAddr; (void)timeout_ms;
    return (int32)mock();
}

#endif /* UNIT_TEST */

/* ---------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------- */

/* CRC-16/CCITT-FALSE canonical check vector (ASCII "123456789"). */
static const uint8 CRC_VECTOR[] = {
    0x31U, 0x32U, 0x33U, 0x34U, 0x35U, 0x36U, 0x37U, 0x38U, 0x39U
};
#define CRC_VECTOR_LEN  ((uint16)9U)
#define CRC_VECTOR_EXPECTED  ((uint16)0x29B1U)

/* Minimal valid datagram: 16-byte header (APID=0x540) + 0-byte payload + 2-byte CRC.
 * APID 0x540: buf[0]=0x05, buf[1]=0x40.
 * CRC of 0 bytes = init = 0xFFFF → buf[16]=0xFF, buf[17]=0xFF. */
static const uint8 VALID_PKT[] = {
    0x05U, 0x40U,                               /* APID = 0x540 */
    0x00U, 0x00U, 0x00U, 0x00U,                 /* primary header bytes 2–5 */
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,   /* secondary header bytes 0–5 */
    0x00U, 0x00U, 0x00U, 0x00U,                 /* secondary header bytes 6–9 */
    0xFFU, 0xFFU                                 /* CRC of 0 payload bytes = 0xFFFF */
};
#define VALID_PKT_LEN  ((uint32)18U)

/* Same as VALID_PKT but with incorrect CRC bytes. */
static const uint8 BAD_CRC_PKT[] = {
    0x05U, 0x40U,
    0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U   /* wrong CRC — correct would be 0xFFFF */
};
#define BAD_CRC_PKT_LEN  ((uint32)18U)

/* 18-byte datagram with APID 0x600 (out of range: 0x06 | 0x00). */
static const uint8 OOR_APID_PKT[] = {
    0x06U, 0x00U,   /* APID = 0x600 — outside [0x500, 0x57F] */
    0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U,
    0xFFU, 0xFFU
};
#define OOR_APID_PKT_LEN  ((uint32)18U)

/* 10-byte datagram — too short to pass the length gate. */
static const uint8 SHORT_PKT[] = {
    0x05U, 0x40U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U
};
#define SHORT_PKT_LEN  ((uint32)10U)

/* Reset SIM_ADAPTER_Data before tests that inspect counters. */
static int reset_data(void **state)
{
    (void)state;
    memset(&SIM_ADAPTER_Data, 0, sizeof(SIM_ADAPTER_Data));
    return 0;
}

/* ---------------------------------------------------------------------------
 * Test 1: CRC-16/CCITT-FALSE known vector
 *
 * Crc16("123456789", 9) must equal 0x29B1.
 * This exercises the entire CRC loop and verifies polynomial / init constants.
 * --------------------------------------------------------------------------- */
static void test_crc_known_vector(void **state)
{
    (void)state;
    uint16 result = SIM_ADAPTER_Crc16(CRC_VECTOR, CRC_VECTOR_LEN);
    assert_int_equal((int)result, (int)CRC_VECTOR_EXPECTED);
}

/* ---------------------------------------------------------------------------
 * Test 2: Short datagram rejected before APID extraction
 *
 * Given:  10-byte datagram (< 18)
 * When:   SIM_ADAPTER_ProcessUdp called
 * Then:   EID_PACKET_TOO_SHORT event; ApidRejects incremented; no SB transmit
 * --------------------------------------------------------------------------- */
static void test_short_packet_rejected(void **state)
{
    (void)state;
    expect_function_call(CFE_EVS_SendEvent);  /* EID_PACKET_TOO_SHORT */
    (void)SIM_ADAPTER_ProcessUdp(SHORT_PKT, SHORT_PKT_LEN);
    assert_int_equal((int)SIM_ADAPTER_Data.ApidRejects, 1);
}

/* ---------------------------------------------------------------------------
 * Test 3: APID outside sideband block rejected
 *
 * Given:  18-byte datagram, APID=0x600 (outside 0x500–0x57F)
 * When:   SIM_ADAPTER_ProcessUdp called
 * Then:   EID_APID_OUT_OF_RANGE event; ApidRejects incremented; no SB transmit
 * --------------------------------------------------------------------------- */
static void test_apid_out_of_range_rejected(void **state)
{
    (void)state;
    expect_function_call(CFE_EVS_SendEvent);  /* EID_APID_OUT_OF_RANGE */
    (void)SIM_ADAPTER_ProcessUdp(OOR_APID_PKT, OOR_APID_PKT_LEN);
    assert_int_equal((int)SIM_ADAPTER_Data.ApidRejects, 1);
}

/* ---------------------------------------------------------------------------
 * Test 4: CRC mismatch causes packet to be dropped
 *
 * Given:  18-byte datagram, APID=0x540, stored CRC=0x0000 (wrong)
 * When:   SIM_ADAPTER_ProcessUdp called
 * Then:   EID_CRC_MISMATCH event; CrcMismatches incremented; no SB transmit
 * --------------------------------------------------------------------------- */
static void test_crc_mismatch_rejected(void **state)
{
    (void)state;
    expect_function_call(CFE_EVS_SendEvent);  /* EID_CRC_MISMATCH */
    (void)SIM_ADAPTER_ProcessUdp(BAD_CRC_PKT, BAD_CRC_PKT_LEN);
    assert_int_equal((int)SIM_ADAPTER_Data.CrcMismatches, 1);
}

/* ---------------------------------------------------------------------------
 * Test 5: Valid packet is routed to the Software Bus
 *
 * Given:  18-byte datagram, APID=0x540, CRC=0xFFFF (correct for 0 payload bytes)
 * When:   SIM_ADAPTER_ProcessUdp called
 * Then:   CFE_SB_TransmitMsg called once; PacketsRouted == 1
 * --------------------------------------------------------------------------- */
static void test_valid_packet_routed(void **state)
{
    (void)state;
    expect_function_call(CFE_SB_TransmitMsg);
    will_return(CFE_SB_TransmitMsg, CFE_SUCCESS);
    expect_function_call(CFE_EVS_SendEvent);  /* EID_FAULT_APPLIED_INF_EID */
    (void)SIM_ADAPTER_ProcessUdp(VALID_PKT, VALID_PKT_LEN);
    assert_int_equal((int)SIM_ADAPTER_Data.PacketsRouted, 1);
}

/* ---------------------------------------------------------------------------
 * Test 6: OS_SocketOpen failure causes Init to return an error
 *
 * Given:  OS_SocketOpen returns a negative error code
 * When:   SIM_ADAPTER_AppMain called
 * Then:   EID_INIT_SOCKET_ERR event; RunLoop entered with APP_ERROR → exits immediately
 * --------------------------------------------------------------------------- */
static void test_init_socket_failure(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(OS_SocketOpen,      0);          /* socket id (unused on failure) */
    will_return(OS_SocketOpen,      (int32)-1);  /* OS_ERR_SOCKET_CLOSED */
    expect_function_call(CFE_EVS_SendEvent);  /* EID_INIT_SOCKET_ERR */
    will_return(CFE_ES_RunLoop, false);
    SIM_ADAPTER_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test runner
 * --------------------------------------------------------------------------- */
int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_crc_known_vector),
        cmocka_unit_test_setup(test_short_packet_rejected,        reset_data),
        cmocka_unit_test_setup(test_apid_out_of_range_rejected,   reset_data),
        cmocka_unit_test_setup(test_crc_mismatch_rejected,        reset_data),
        cmocka_unit_test_setup(test_valid_packet_routed,          reset_data),
        cmocka_unit_test(test_init_socket_failure),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
