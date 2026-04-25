/*
 * mcu_payload_gw_test.c — CMocka unit tests for MCU_PAYLOAD_GW
 *
 * RED test (primary DoD requirement):
 *   test_eep_frame_dropped_not_published
 *       — SpW EEP marker → MCU_PAYLOAD_GW_EEP_ERR_EID event, no TransmitMsg
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "mcu_payload_gw.h"

/* ---------------------------------------------------------------------------
 * CFE stubs
 * --------------------------------------------------------------------------- */
#ifdef UNIT_TEST

int32 CFE_ES_RegisterApp(void)            { return (int32)mock(); }

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
    (void)M; *Id = (CFE_SB_MsgId_t)mock(); return CFE_SUCCESS;
}

int32 CFE_MSG_GetFcnCode(const CFE_MSG_Message_t *M, CFE_MSG_FcnCode_t *C)
{
    (void)M; *C = (CFE_MSG_FcnCode_t)mock(); return CFE_SUCCESS;
}

CFE_SB_MsgId_Atom_t CFE_SB_MsgIdToValue(CFE_SB_MsgId_t MsgId)
{
    return (CFE_SB_MsgId_Atom_t)MsgId;
}

CFE_SB_MsgId_t CFE_SB_ValueToMsgId(CFE_SB_MsgId_Atom_t MsgIdValue)
{
    return (CFE_SB_MsgId_t)MsgIdValue;
}

int32 MCU_PAYLOAD_GW_BusPoll(uint8 *FrameBuf, uint16 *FrameLen)
{
    uint16  len = (uint16)mock();
    uint8  *src = (uint8 *)mock();
    int32   ret = (int32)mock();
    *FrameLen = len;
    if (len > 0U && src != NULL)
    {
        memcpy(FrameBuf, src, (size_t)len);
    }
    return ret;
}

#endif /* UNIT_TEST */

/* ---------------------------------------------------------------------------
 * SpW test frames
 *
 * Valid:   payload bytes + EOP (0x00) as last byte
 * EEP:     payload bytes + EEP (0x01) as last byte
 * Empty:   zero-length frame (malformed)
 * --------------------------------------------------------------------------- */
static const uint8 GOOD_SPW_FRAME[] = {0x01U, 0x02U, 0x03U, 0x04U,
                                        0x05U, MCU_PAYLOAD_GW_SPW_EOP};
static const uint8 EEP_SPW_FRAME[]  = {0x01U, 0x02U, 0x03U, 0x04U,
                                        0x05U, MCU_PAYLOAD_GW_SPW_EEP};

#define GOOD_SPW_LEN  ((uint16)6U)
#define EEP_SPW_LEN   ((uint16)6U)

/* ---------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------- */
static void queue_successful_init(void)
{
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    expect_function_call(CFE_EVS_SendEvent); /* startup event */
}

static void queue_idle_iteration(void)
{
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, MCU_PAYLOAD_GW_BUS_NO_DATA);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uint16)0U);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uintptr_t)NULL);
    will_return(MCU_PAYLOAD_GW_BusPoll, MCU_PAYLOAD_GW_BUS_NO_DATA);
}

/* ---------------------------------------------------------------------------
 * Init failure paths
 * --------------------------------------------------------------------------- */
static void test_init_register_app_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_ES_ERR_APP_REGISTER);
    will_return(CFE_ES_RunLoop, false);
    MCU_PAYLOAD_GW_AppMain();
}

static void test_init_evs_register_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_EVS_APP_FILTER_OVERLOAD);
    will_return(CFE_ES_RunLoop, false);
    MCU_PAYLOAD_GW_AppMain();
}

static void test_init_pipe_create_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  0);
    will_return(CFE_SB_CreatePipe,  CFE_SB_BAD_ARGUMENT);
    expect_function_call(CFE_EVS_SendEvent);
    will_return(CFE_ES_RunLoop, false);
    MCU_PAYLOAD_GW_AppMain();
}

static void test_init_subscribe_cmd_mid_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET);
    will_return(CFE_ES_RunLoop, false);
    MCU_PAYLOAD_GW_AppMain();
}

static void test_init_success(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop, false);
    MCU_PAYLOAD_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * SB receive error
 * --------------------------------------------------------------------------- */
static void test_sb_receive_error_increments_err_counter(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, CFE_SB_PIPE_RD_ERR);
    expect_function_call(CFE_EVS_SendEvent);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uint16)0U);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uintptr_t)NULL);
    will_return(MCU_PAYLOAD_GW_BusPoll, MCU_PAYLOAD_GW_BUS_NO_DATA);
    will_return(CFE_ES_RunLoop, false);
    MCU_PAYLOAD_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * GREEN TEST: valid SpW frame (EOP) publishes to SB
 * --------------------------------------------------------------------------- */
static void test_eop_frame_publishes_to_sb(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, MCU_PAYLOAD_GW_BUS_NO_DATA);
    will_return(MCU_PAYLOAD_GW_BusPoll, GOOD_SPW_LEN);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uintptr_t)GOOD_SPW_FRAME);
    will_return(MCU_PAYLOAD_GW_BusPoll, CFE_SUCCESS);
    expect_function_call(CFE_SB_TransmitMsg);
    will_return(CFE_SB_TransmitMsg, CFE_SUCCESS);
    will_return(CFE_ES_RunLoop, false);
    MCU_PAYLOAD_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * RED TEST: EEP frame is dropped — TransmitMsg never called
 *
 * Given:  init succeeds; BusPollStub returns a SpW packet ending with EEP (0x01)
 * When:   AppMain processes one run-loop iteration
 * Then:   MCU_PAYLOAD_GW_EEP_ERR_EID emitted; CFE_SB_TransmitMsg NOT called
 * --------------------------------------------------------------------------- */
static void test_eep_frame_dropped_not_published(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, MCU_PAYLOAD_GW_BUS_NO_DATA);
    will_return(MCU_PAYLOAD_GW_BusPoll, EEP_SPW_LEN);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uintptr_t)EEP_SPW_FRAME);
    will_return(MCU_PAYLOAD_GW_BusPoll, CFE_SUCCESS);
    expect_function_call(CFE_EVS_SendEvent); /* EEP error event */
    /* TransmitMsg must NOT be called */
    will_return(CFE_ES_RunLoop, false);
    MCU_PAYLOAD_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * Empty frame (zero-length) is dropped
 * --------------------------------------------------------------------------- */
static void test_empty_frame_dropped(void **state)
{
    (void)state;
    static const uint8 empty = 0U;
    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, MCU_PAYLOAD_GW_BUS_NO_DATA);
    /* len=0 triggers ERR_FRAME_FORMAT in ValidateSpwFrame */
    will_return(MCU_PAYLOAD_GW_BusPoll, (uint16)0U);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uintptr_t)&empty);
    will_return(MCU_PAYLOAD_GW_BusPoll, CFE_SUCCESS);
    expect_function_call(CFE_EVS_SendEvent);
    will_return(CFE_ES_RunLoop, false);
    MCU_PAYLOAD_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * Bus silence threshold triggers EVS event
 * --------------------------------------------------------------------------- */
static void test_silence_threshold_triggers_evs_event(void **state)
{
    (void)state;
    queue_successful_init();
    queue_idle_iteration();
    queue_idle_iteration();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, MCU_PAYLOAD_GW_BUS_NO_DATA);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uint16)0U);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uintptr_t)NULL);
    will_return(MCU_PAYLOAD_GW_BusPoll, MCU_PAYLOAD_GW_BUS_NO_DATA);
    expect_function_call(CFE_EVS_SendEvent);
    will_return(CFE_ES_RunLoop, false);
    MCU_PAYLOAD_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * Bus driver generic error increments ErrCounter
 * --------------------------------------------------------------------------- */
static void test_bus_driver_error_increments_err_counter(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, MCU_PAYLOAD_GW_BUS_NO_DATA);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uint16)0U);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uintptr_t)NULL);
    will_return(MCU_PAYLOAD_GW_BusPoll, CFE_SB_PIPE_RD_ERR);
    will_return(CFE_ES_RunLoop, false);
    MCU_PAYLOAD_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * TC forwarding: SB receive success does not error
 * --------------------------------------------------------------------------- */
static void test_tc_received_forwarded_to_bus(void **state)
{
    (void)state;
    static CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uint16)0U);
    will_return(MCU_PAYLOAD_GW_BusPoll, (uintptr_t)NULL);
    will_return(MCU_PAYLOAD_GW_BusPoll, MCU_PAYLOAD_GW_BUS_NO_DATA);
    will_return(CFE_ES_RunLoop, false);
    MCU_PAYLOAD_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test runner
 * --------------------------------------------------------------------------- */
int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_register_app_fails),
        cmocka_unit_test(test_init_evs_register_fails),
        cmocka_unit_test(test_init_pipe_create_fails),
        cmocka_unit_test(test_init_subscribe_cmd_mid_fails),
        cmocka_unit_test(test_init_success),
        cmocka_unit_test(test_sb_receive_error_increments_err_counter),
        /* GREEN test */
        cmocka_unit_test(test_eop_frame_publishes_to_sb),
        /* RED test — primary DoD requirement */
        cmocka_unit_test(test_eep_frame_dropped_not_published),
        cmocka_unit_test(test_empty_frame_dropped),
        cmocka_unit_test(test_silence_threshold_triggers_evs_event),
        cmocka_unit_test(test_bus_driver_error_increments_err_counter),
        cmocka_unit_test(test_tc_received_forwarded_to_bus),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
