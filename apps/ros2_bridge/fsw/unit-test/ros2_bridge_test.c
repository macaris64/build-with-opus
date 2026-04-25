/*
 * ros2_bridge_test.c — CMocka unit tests for ros2_bridge.
 *
 * Tests the APID gate in ROS2_BRIDGE_ProcessUdp.
 * CFE and OSAL calls are intercepted via UNIT_TEST guards.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "ros2_bridge.h"

/* ---------------------------------------------------------------------------
 * CFE / OSAL stub implementations (UNIT_TEST only)
 * --------------------------------------------------------------------------- */

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

int32 OS_SocketOpen(osal_id_t *id, OS_SocketDomain_t D, OS_SocketType_t T)
{
    (void)D; (void)T;
    *id = (osal_id_t)mock();
    return (int32)mock();
}

int32 OS_SocketAddrInit(OS_SockAddr_t *A, OS_SocketDomain_t D)
{
    (void)A; (void)D;
    return (int32)mock();
}

int32 OS_SocketAddrSetPort(OS_SockAddr_t *A, uint16 P)
{
    (void)A; (void)P;
    return (int32)mock();
}

int32 OS_SocketAddrFromString(OS_SockAddr_t *A, const char *S)
{
    (void)A; (void)S;
    return (int32)mock();
}

int32 OS_SocketBind(osal_id_t id, const OS_SockAddr_t *A)
{
    (void)id; (void)A;
    return (int32)mock();
}

int32 OS_SocketRecvFrom(osal_id_t id, void *buf, uint32 len,
                        OS_SockAddr_t *Addr, int32 timeout_ms)
{
    (void)id; (void)buf; (void)len; (void)Addr; (void)timeout_ms;
    return (int32)mock();
}

int32 OS_SocketSendTo(osal_id_t id, const void *buf, uint32 len,
                      const OS_SockAddr_t *Addr)
{
    (void)id; (void)buf; (void)len; (void)Addr;
    return (int32)mock();
}

int32 OS_TaskDelay(uint32 ms)
{
    (void)ms;
    return OS_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * Helper: build a minimal 16-byte CCSDS SPP with given APID
 * --------------------------------------------------------------------------- */
static void build_spp(uint8 *buf, uint16 apid)
{
    memset(buf, 0, 16U);
    buf[0] = (uint8)(0x08U | (uint8)((apid >> 8U) & 0x07U));
    buf[1] = (uint8)(apid & 0xFFU);
    buf[4] = 0x00U;
    buf[5] = 0x09U; /* data length = 10 (secondary header only) */
}

/* ---------------------------------------------------------------------------
 * Test: APID 0x300 (rover_land) accepted and routed to SB
 * --------------------------------------------------------------------------- */
static void test_apid_gate_accepts_0x300(void **state)
{
    (void)state;
    uint8 buf[16];
    int32 result;

    memset(&ROS2_BRIDGE_Data, 0, sizeof(ROS2_BRIDGE_Data));
    build_spp(buf, 0x300U);

    /* expect CFE_SB_TransmitMsg to be called once (routed) */
    expect_function_call(CFE_SB_TransmitMsg);
    will_return(CFE_SB_TransmitMsg, CFE_SUCCESS);

    result = ROS2_BRIDGE_ProcessUdp(buf, 16U);

    assert_int_equal(result, CFE_SUCCESS);
    assert_int_equal(ROS2_BRIDGE_Data.PacketsRouted, 1U);
    assert_int_equal(ROS2_BRIDGE_Data.ApidRejects, 0U);
}

/* ---------------------------------------------------------------------------
 * Test: APID 0x200 (out of range) rejected
 * --------------------------------------------------------------------------- */
static void test_apid_gate_rejects_0x200(void **state)
{
    (void)state;
    uint8 buf[16];
    int32 result;

    memset(&ROS2_BRIDGE_Data, 0, sizeof(ROS2_BRIDGE_Data));
    build_spp(buf, 0x200U);

    /* expect CFE_EVS_SendEvent for out-of-range APID */
    expect_function_call(CFE_EVS_SendEvent);

    result = ROS2_BRIDGE_ProcessUdp(buf, 16U);

    assert_int_equal(result, CFE_SB_BAD_ARGUMENT);
    assert_int_equal(ROS2_BRIDGE_Data.ApidRejects, 1U);
    assert_int_equal(ROS2_BRIDGE_Data.PacketsRouted, 0U);
}

/* ---------------------------------------------------------------------------
 * Test: Packet too short (< 16 bytes) rejected
 * --------------------------------------------------------------------------- */
static void test_apid_gate_rejects_too_short(void **state)
{
    (void)state;
    uint8 buf[15];
    int32 result;

    memset(&ROS2_BRIDGE_Data, 0, sizeof(ROS2_BRIDGE_Data));
    memset(buf, 0, sizeof(buf));

    /* expect CFE_EVS_SendEvent for too-short packet */
    expect_function_call(CFE_EVS_SendEvent);

    result = ROS2_BRIDGE_ProcessUdp(buf, 15U);

    assert_int_equal(result, CFE_SB_BAD_ARGUMENT);
    assert_int_equal(ROS2_BRIDGE_Data.ApidRejects, 1U);
}

/* ---------------------------------------------------------------------------
 * Test runner
 * --------------------------------------------------------------------------- */
int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_apid_gate_accepts_0x300),
        cmocka_unit_test(test_apid_gate_rejects_0x200),
        cmocka_unit_test(test_apid_gate_rejects_too_short),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
