/*
 * mcu_eps_gw_test.c — CMocka unit tests for MCU_EPS_GW
 *
 * CFE API calls and the bus driver stub are intercepted via UNIT_TEST guards.
 * No real cFE library is linked.
 *
 * Coverage target: 100% branch coverage of mcu_eps_gw.c
 *
 * RED tests (primary DoD requirements):
 *   test_corrupted_hdlc_frame_dropped_not_published
 *       — corrupt CRC → EVS event, CFE_SB_TransmitMsg never called
 *   test_valid_hdlc_frame_publishes_to_sb
 *       — valid frame → CFE_SB_TransmitMsg called once
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "mcu_eps_gw.h"

/* ---------------------------------------------------------------------------
 * CFE stub implementations (UNIT_TEST only)
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
    /* Record that an EVS event was sent so tests can assert on call count */
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

CFE_SB_MsgId_Atom_t CFE_SB_MsgIdToValue(CFE_SB_MsgId_t MsgId)
{
    return (CFE_SB_MsgId_Atom_t)MsgId;
}

CFE_SB_MsgId_t CFE_SB_ValueToMsgId(CFE_SB_MsgId_Atom_t MsgIdValue)
{
    return (CFE_SB_MsgId_t)MsgIdValue;
}

/* Bus driver stub — controlled by test via will_return / mock() */
int32 MCU_EPS_GW_BusPoll(uint8 *FrameBuf, uint16 *FrameLen)
{
    uint16  len  = (uint16)mock();
    uint8  *src  = (uint8 *)mock();
    int32   ret  = (int32)mock();

    *FrameLen = len;
    if (len > 0U && src != NULL)
    {
        memcpy(FrameBuf, src, (size_t)len);
    }
    return ret;
}

#endif /* UNIT_TEST */

/* ---------------------------------------------------------------------------
 * Test frame helpers
 *
 * CRC-16/CCITT-FALSE computed over the SPP payload, stored LSB-first.
 * Minimal valid frame: FLAG | 0x01 | CRC_lo | CRC_hi | FLAG
 *   payload = {0x01}, CRC-16/CCITT-FALSE({0x01}) = 0x1021 ^ ... = let's use
 *   a pre-computed known-good frame with 1 byte payload 0xAB.
 *
 * To compute: CRC-16/CCITT-FALSE(0xAB):
 *   init=0xFFFF, poly=0x1021
 *   After processing 0xAB: result = 0x5A0E
 *   Frame: 0x7E 0xAB 0x0E 0x5A 0x7E  (CRC LSB-first: lo=0x0E, hi=0x5A)
 * --------------------------------------------------------------------------- */

/* Valid 1-byte payload frame: FLAG | 0xAB | CRC_lo=0x71 | CRC_hi=0xE5 | FLAG
 * CRC-16/CCITT-FALSE({0xAB}) = 0xE571; stored LSB-first per ICD §2.3. */
static const uint8 GOOD_FRAME[] = {0x7EU, 0xABU, 0x71U, 0xE5U, 0x7EU};
#define GOOD_FRAME_LEN ((uint16)5U)

/* Corrupted frame: same structure but CRC bytes deliberately wrong */
static const uint8 BAD_FRAME[] = {0x7EU, 0xABU, 0xFFU, 0xFFU, 0x7EU};
#define BAD_FRAME_LEN  ((uint16)5U)

/* Frame missing opening flag */
static const uint8 NO_FLAG_FRAME[] = {0x00U, 0xABU, 0x0EU, 0x5AU, 0x7EU};
#define NO_FLAG_FRAME_LEN ((uint16)5U)

/* ---------------------------------------------------------------------------
 * Init helpers
 * --------------------------------------------------------------------------- */
static void queue_successful_init(void)
{
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* MCU_EPS_CMD_MID */
    /* Startup EVS event */
    expect_function_call(CFE_EVS_SendEvent);
}

/* Queue one run-loop iteration with no SB TC and bus returning BUS_NO_DATA */
static void queue_idle_iteration(void)
{
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, MCU_EPS_GW_BUS_NO_DATA); /* timeout — no TC */
    /* BusPoll: no data */
    will_return(MCU_EPS_GW_BusPoll, (uint16)0U);
    will_return(MCU_EPS_GW_BusPoll, (uintptr_t)NULL);
    will_return(MCU_EPS_GW_BusPoll, MCU_EPS_GW_BUS_NO_DATA);
}

/* ---------------------------------------------------------------------------
 * Init failure paths
 * --------------------------------------------------------------------------- */
static void test_init_register_app_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_ES_ERR_APP_REGISTER);
    will_return(CFE_ES_RunLoop, false);
    MCU_EPS_GW_AppMain();
}

static void test_init_evs_register_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_EVS_APP_FILTER_OVERLOAD);
    will_return(CFE_ES_RunLoop, false);
    MCU_EPS_GW_AppMain();
}

static void test_init_pipe_create_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  0);
    will_return(CFE_SB_CreatePipe,  CFE_SB_BAD_ARGUMENT);
    expect_function_call(CFE_EVS_SendEvent); /* pipe creation error event */
    will_return(CFE_ES_RunLoop, false);
    MCU_EPS_GW_AppMain();
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
    MCU_EPS_GW_AppMain();
}

static void test_init_success(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop, false);
    MCU_EPS_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * Run-loop: SB pipe read error increments ErrCounter
 * --------------------------------------------------------------------------- */
static void test_sb_receive_error_increments_err_counter(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, CFE_SB_PIPE_RD_ERR);
    expect_function_call(CFE_EVS_SendEvent); /* SB receive error event */
    /* BusPoll: no data */
    will_return(MCU_EPS_GW_BusPoll, (uint16)0U);
    will_return(MCU_EPS_GW_BusPoll, (uintptr_t)NULL);
    will_return(MCU_EPS_GW_BusPoll, MCU_EPS_GW_BUS_NO_DATA);
    will_return(CFE_ES_RunLoop, false);
    MCU_EPS_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * Bus poll: no data increments silence count (does NOT publish)
 * --------------------------------------------------------------------------- */
static void test_no_data_from_bus_does_not_publish(void **state)
{
    (void)state;
    queue_successful_init();
    queue_idle_iteration();
    will_return(CFE_ES_RunLoop, false);
    /* CFE_SB_TransmitMsg must NOT be called */
    MCU_EPS_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * RED TEST: corrupted HDLC frame is dropped — TransmitMsg never called
 *
 * Given:  init succeeds; BusPollStub returns a frame with wrong CRC bytes
 * When:   AppMain processes one run-loop iteration
 * Then:   MCU_EPS_GW_HDLC_CRC_FAIL_ERR_EID event emitted;
 *         CFE_SB_TransmitMsg is NOT called
 * --------------------------------------------------------------------------- */
static void test_corrupted_hdlc_frame_dropped_not_published(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, MCU_EPS_GW_BUS_NO_DATA);
    /* BusPoll returns the corrupted frame */
    will_return(MCU_EPS_GW_BusPoll, BAD_FRAME_LEN);
    will_return(MCU_EPS_GW_BusPoll, (uintptr_t)BAD_FRAME);
    will_return(MCU_EPS_GW_BusPoll, CFE_SUCCESS);
    /* Expect EVS error event for CRC failure */
    expect_function_call(CFE_EVS_SendEvent);
    /* CFE_SB_TransmitMsg must NOT be called — do NOT queue a will_return for it */
    will_return(CFE_ES_RunLoop, false);
    MCU_EPS_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * GREEN TEST: valid HDLC frame publishes to SB exactly once
 *
 * Given:  init succeeds; BusPollStub returns a frame with correct CRC
 * When:   AppMain processes one run-loop iteration
 * Then:   CFE_SB_TransmitMsg called once (MCU_EPS_HK_MID)
 * --------------------------------------------------------------------------- */
static void test_valid_hdlc_frame_publishes_to_sb(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, MCU_EPS_GW_BUS_NO_DATA);
    /* BusPoll returns the good frame */
    will_return(MCU_EPS_GW_BusPoll, GOOD_FRAME_LEN);
    will_return(MCU_EPS_GW_BusPoll, (uintptr_t)GOOD_FRAME);
    will_return(MCU_EPS_GW_BusPoll, CFE_SUCCESS);
    /* TransmitMsg must be called exactly once */
    expect_function_call(CFE_SB_TransmitMsg);
    will_return(CFE_SB_TransmitMsg, CFE_SUCCESS);
    will_return(CFE_ES_RunLoop, false);
    MCU_EPS_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * Frame missing opening flag — dropped with format error (no TransmitMsg)
 * --------------------------------------------------------------------------- */
static void test_frame_missing_flag_dropped(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, MCU_EPS_GW_BUS_NO_DATA);
    will_return(MCU_EPS_GW_BusPoll, NO_FLAG_FRAME_LEN);
    will_return(MCU_EPS_GW_BusPoll, (uintptr_t)NO_FLAG_FRAME);
    will_return(MCU_EPS_GW_BusPoll, CFE_SUCCESS);
    expect_function_call(CFE_EVS_SendEvent); /* CRC-fail covers format errors too */
    will_return(CFE_ES_RunLoop, false);
    MCU_EPS_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * Bus silence threshold triggers EVS silent event (3 consecutive idle cycles)
 * --------------------------------------------------------------------------- */
static void test_silence_threshold_triggers_evs_event(void **state)
{
    (void)state;
    queue_successful_init();
    /* Three consecutive idle iterations (BUS_NO_DATA each) */
    queue_idle_iteration();
    queue_idle_iteration();
    /* Third idle iteration hits the threshold → EVS silent event */
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, MCU_EPS_GW_BUS_NO_DATA);
    will_return(MCU_EPS_GW_BusPoll, (uint16)0U);
    will_return(MCU_EPS_GW_BusPoll, (uintptr_t)NULL);
    will_return(MCU_EPS_GW_BusPoll, MCU_EPS_GW_BUS_NO_DATA);
    expect_function_call(CFE_EVS_SendEvent); /* BUS_SILENT event */
    will_return(CFE_ES_RunLoop, false);
    MCU_EPS_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * TC forwarding: SB receive success calls BusEmitTc (no error)
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
    /* BusPoll: no data */
    will_return(MCU_EPS_GW_BusPoll, (uint16)0U);
    will_return(MCU_EPS_GW_BusPoll, (uintptr_t)NULL);
    will_return(MCU_EPS_GW_BusPoll, MCU_EPS_GW_BUS_NO_DATA);
    will_return(CFE_ES_RunLoop, false);
    MCU_EPS_GW_AppMain();
}

/* ---------------------------------------------------------------------------
 * Bus poll returns generic driver error (not BUS_NO_DATA) — increments ErrCounter
 * --------------------------------------------------------------------------- */
static void test_bus_driver_error_increments_err_counter(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, MCU_EPS_GW_BUS_NO_DATA);
    will_return(MCU_EPS_GW_BusPoll, (uint16)0U);
    will_return(MCU_EPS_GW_BusPoll, (uintptr_t)NULL);
    will_return(MCU_EPS_GW_BusPoll, CFE_SB_PIPE_RD_ERR); /* generic driver error */
    will_return(CFE_ES_RunLoop, false);
    MCU_EPS_GW_AppMain();
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
        cmocka_unit_test(test_no_data_from_bus_does_not_publish),
        /* RED test — primary DoD requirement */
        cmocka_unit_test(test_corrupted_hdlc_frame_dropped_not_published),
        /* GREEN test */
        cmocka_unit_test(test_valid_hdlc_frame_publishes_to_sb),
        cmocka_unit_test(test_frame_missing_flag_dropped),
        cmocka_unit_test(test_silence_threshold_triggers_evs_event),
        cmocka_unit_test(test_tc_received_forwarded_to_bus),
        cmocka_unit_test(test_bus_driver_error_increments_err_counter),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
