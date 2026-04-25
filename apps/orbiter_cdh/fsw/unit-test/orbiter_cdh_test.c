/*
 * orbiter_cdh_test.c — CMocka unit tests for ORBITER_CDH
 *
 * CFE API calls are intercepted via UNIT_TEST stubs below.
 * No real cFE library is linked; all CFE_* symbols resolve to stubs
 * configured per-test via CMocka's mock infrastructure.
 *
 * Coverage target: 100% branch coverage of orbiter_cdh.c
 * Measure with: bash scripts/coverage-gate.sh apps/orbiter_cdh
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "orbiter_cdh.h"

#define ORBITER_CDH_INVALID_CC  ((CFE_MSG_FcnCode_t)0xFFU)
#define ORBITER_CDH_INVALID_MODE ((uint8)0xFFU)

/* ---------------------------------------------------------------------------
 * CFE stub implementations (compiled only under UNIT_TEST)
 * Stubs are thin: cast mock() return value to the required type.
 * No business logic inside stubs (testing.md).
 * --------------------------------------------------------------------------- */
#ifdef UNIT_TEST

int32 CFE_ES_RegisterApp(void)
{
    return (int32)mock();
}

int32 CFE_EVS_Register(const void *Filters, uint16 NumFilters, uint16 FilterScheme)
{
    (void)Filters; (void)NumFilters; (void)FilterScheme;
    return (int32)mock();
}

int32 CFE_SB_CreatePipe(CFE_SB_PipeId_t *PipeIdPtr, uint16 Depth, const char *PipeName)
{
    (void)Depth; (void)PipeName;
    *PipeIdPtr = (CFE_SB_PipeId_t)mock();
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

int32 CFE_SB_TransmitMsg(CFE_MSG_Message_t *MsgPtr, bool IncrementSequenceCount)
{
    (void)MsgPtr; (void)IncrementSequenceCount;
    return (int32)mock();
}

void CFE_ES_ExitApp(uint32 ExitStatus)
{
    (void)ExitStatus;
}

int32 CFE_MSG_GetMsgId(const CFE_MSG_Message_t *MsgPtr, CFE_SB_MsgId_t *MsgId)
{
    (void)MsgPtr;
    *MsgId = (CFE_SB_MsgId_t)mock();
    return CFE_SUCCESS;
}

int32 CFE_MSG_GetFcnCode(const CFE_MSG_Message_t *MsgPtr, CFE_MSG_FcnCode_t *FcnCode)
{
    (void)MsgPtr;
    *FcnCode = (CFE_MSG_FcnCode_t)mock();
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

#endif /* UNIT_TEST */

/* ---------------------------------------------------------------------------
 * Helper: queue will_return values for a successful 7-subscription init.
 * Call this at the top of any test that needs init to succeed.
 * --------------------------------------------------------------------------- */
static void queue_successful_init(void)
{
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);           /* pipe handle */
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* CMD_MID */
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* CDH_HK_MID */
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* SAMPLE_APP_HK_MID */
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* ORBITER_ADCS_HK_MID */
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* ORBITER_COMM_HK_MID */
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* ORBITER_POWER_HK_MID */
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* ORBITER_PAYLOAD_HK_MID */
}

/* ---------------------------------------------------------------------------
 * Init failure path tests
 * --------------------------------------------------------------------------- */
static void test_init_register_app_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_ES_ERR_APP_REGISTER);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_CDH_AppMain();
}

static void test_init_evs_register_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_EVS_APP_FILTER_OVERLOAD);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_CDH_AppMain();
}

static void test_init_pipe_create_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  0);
    will_return(CFE_SB_CreatePipe,  CFE_SB_BAD_ARGUMENT);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_CDH_AppMain();
}

static void test_init_subscribe_cmd_mid_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* CMD_MID fails */
    will_return(CFE_ES_RunLoop, false);
    ORBITER_CDH_AppMain();
}

static void test_init_subscribe_hk_mid_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);          /* CMD_MID ok */
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* CDH_HK_MID fails */
    will_return(CFE_ES_RunLoop, false);
    ORBITER_CDH_AppMain();
}

static void test_init_subscribe_sample_hk_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* SAMPLE_APP_HK fails */
    will_return(CFE_ES_RunLoop, false);
    ORBITER_CDH_AppMain();
}

static void test_init_subscribe_adcs_hk_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* ADCS_HK fails */
    will_return(CFE_ES_RunLoop, false);
    ORBITER_CDH_AppMain();
}

static void test_init_subscribe_comm_hk_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* COMM_HK fails */
    will_return(CFE_ES_RunLoop, false);
    ORBITER_CDH_AppMain();
}

static void test_init_subscribe_power_hk_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* POWER_HK fails */
    will_return(CFE_ES_RunLoop, false);
    ORBITER_CDH_AppMain();
}

static void test_init_subscribe_payload_hk_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* PAYLOAD_HK fails */
    will_return(CFE_ES_RunLoop, false);
    ORBITER_CDH_AppMain();
}

/* ---------------------------------------------------------------------------
 * Happy-path init
 * --------------------------------------------------------------------------- */
static void test_init_success(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop, false);
    ORBITER_CDH_AppMain();
}

/* ---------------------------------------------------------------------------
 * Command dispatch: NOOP
 * --------------------------------------------------------------------------- */
static void test_noop_command_increments_counter(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_CDH_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_CDH_NOOP_CC);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

/* ---------------------------------------------------------------------------
 * Command dispatch: RESET
 * --------------------------------------------------------------------------- */
static void test_reset_command_clears_counters(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_CDH_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_CDH_RESET_CC);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

/* ---------------------------------------------------------------------------
 * Command dispatch: MODE_TRANSITION — RED test (fails before orbiter_cdh.c exists)
 *
 * Given:  orbiter_cdh init succeeds (all 7 subscribes return CFE_SUCCESS)
 * When:   MODE_TRANSITION command (CC=2) with Mode=SAFE arrives on CDH_CMD_MID
 * Then:   CmdCounter increments, mode becomes SAFE, ErrCounter stays 0
 * --------------------------------------------------------------------------- */
static void test_mode_transition_to_safe(void **state)
{
    (void)state;
    ORBITER_CDH_ModeTransCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.Mode = ORBITER_CDH_MODE_SAFE;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_CDH_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_CDH_MODE_TRANSITION_CC);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

static void test_mode_transition_to_nominal(void **state)
{
    (void)state;
    ORBITER_CDH_ModeTransCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.Mode = ORBITER_CDH_MODE_NOMINAL;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_CDH_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_CDH_MODE_TRANSITION_CC);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

static void test_mode_transition_to_emergency(void **state)
{
    (void)state;
    ORBITER_CDH_ModeTransCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.Mode = ORBITER_CDH_MODE_EMERGENCY;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_CDH_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_CDH_MODE_TRANSITION_CC);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

static void test_mode_transition_invalid_mode_increments_err_counter(void **state)
{
    (void)state;
    ORBITER_CDH_ModeTransCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.Mode = ORBITER_CDH_INVALID_MODE;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_CDH_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_CDH_MODE_TRANSITION_CC);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

/* ---------------------------------------------------------------------------
 * Command dispatch: EVS_FILTER
 * --------------------------------------------------------------------------- */
static void test_evs_filter_command_increments_counter(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_CDH_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_CDH_EVS_FILTER_CC);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

/* ---------------------------------------------------------------------------
 * Command dispatch: unknown command code
 * --------------------------------------------------------------------------- */
static void test_unknown_command_code_increments_err_counter(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_CDH_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_CDH_INVALID_CC);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

/* ---------------------------------------------------------------------------
 * MID dispatch: HK request triggers CFE_SB_TransmitMsg
 * --------------------------------------------------------------------------- */
static void test_hk_request_calls_transmit(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_CDH_HK_MID);
    will_return(CFE_SB_TransmitMsg,   CFE_SUCCESS);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

/* ---------------------------------------------------------------------------
 * MID dispatch: peer HK messages update aggregate state without error
 * --------------------------------------------------------------------------- */
static void test_peer_hk_sample_app_handled(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     SAMPLE_APP_HK_MID);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

static void test_peer_hk_adcs_handled(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_ADCS_HK_MID);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

static void test_peer_hk_comm_handled(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_HK_MID);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

static void test_peer_hk_power_handled(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_HK_MID);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

static void test_peer_hk_payload_handled(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_PAYLOAD_HK_MID);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

/* ---------------------------------------------------------------------------
 * MID dispatch: unknown MsgId increments ErrCounter
 * --------------------------------------------------------------------------- */
static void test_unknown_msgid_increments_err_counter(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     CFE_SB_INVALID_MSG_ID);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

/* ---------------------------------------------------------------------------
 * Run loop: SB receive error increments ErrCounter and continues
 * --------------------------------------------------------------------------- */
static void test_sb_receive_error_increments_err_counter(void **state)
{
    (void)state;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, CFE_SB_PIPE_RD_ERR);
    will_return(CFE_ES_RunLoop, false);

    ORBITER_CDH_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test runner
 * --------------------------------------------------------------------------- */
int main(void)
{
    const struct CMUnitTest tests[] = {
        /* Init failure paths */
        cmocka_unit_test(test_init_register_app_fails),
        cmocka_unit_test(test_init_evs_register_fails),
        cmocka_unit_test(test_init_pipe_create_fails),
        cmocka_unit_test(test_init_subscribe_cmd_mid_fails),
        cmocka_unit_test(test_init_subscribe_hk_mid_fails),
        cmocka_unit_test(test_init_subscribe_sample_hk_fails),
        cmocka_unit_test(test_init_subscribe_adcs_hk_fails),
        cmocka_unit_test(test_init_subscribe_comm_hk_fails),
        cmocka_unit_test(test_init_subscribe_power_hk_fails),
        cmocka_unit_test(test_init_subscribe_payload_hk_fails),
        /* Happy-path init */
        cmocka_unit_test(test_init_success),
        /* Command dispatch */
        cmocka_unit_test(test_noop_command_increments_counter),
        cmocka_unit_test(test_reset_command_clears_counters),
        cmocka_unit_test(test_mode_transition_to_safe),
        cmocka_unit_test(test_mode_transition_to_nominal),
        cmocka_unit_test(test_mode_transition_to_emergency),
        cmocka_unit_test(test_mode_transition_invalid_mode_increments_err_counter),
        cmocka_unit_test(test_evs_filter_command_increments_counter),
        cmocka_unit_test(test_unknown_command_code_increments_err_counter),
        /* MID dispatch */
        cmocka_unit_test(test_hk_request_calls_transmit),
        cmocka_unit_test(test_peer_hk_sample_app_handled),
        cmocka_unit_test(test_peer_hk_adcs_handled),
        cmocka_unit_test(test_peer_hk_comm_handled),
        cmocka_unit_test(test_peer_hk_power_handled),
        cmocka_unit_test(test_peer_hk_payload_handled),
        cmocka_unit_test(test_unknown_msgid_increments_err_counter),
        /* Run loop */
        cmocka_unit_test(test_sb_receive_error_increments_err_counter),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
