/*
 * orbiter_adcs_test.c — CMocka unit tests for ORBITER_ADCS
 *
 * CFE API calls are intercepted via UNIT_TEST stubs below.
 * No real cFE library is linked; all CFE_* symbols resolve to stubs
 * configured per-test via CMocka's mock infrastructure.
 *
 * Coverage target: 100% branch coverage of orbiter_adcs.c
 * Measure with: bash scripts/coverage-gate.sh apps/orbiter_adcs
 *
 * RED tests: test_set_target_quat_invalid_norm_rejected and
 *            test_set_target_quat_valid_norm_accepted define the primary
 *            DoD requirement (quaternion validation at TC ingestion).
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "orbiter_adcs.h"

#define ORBITER_ADCS_INVALID_CC  ((CFE_MSG_FcnCode_t)0xFFU)

/* ---------------------------------------------------------------------------
 * CFE stub implementations (compiled only under UNIT_TEST)
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
 * Helper: queue will_return values for a successful 3-subscription init.
 * --------------------------------------------------------------------------- */
static void queue_successful_init(void)
{
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);           /* pipe handle */
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* CMD_MID */
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* ADCS_HK_MID */
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* MCU_RWA_HK_MID */
}

/* ---------------------------------------------------------------------------
 * Init failure path tests
 * --------------------------------------------------------------------------- */
static void test_init_register_app_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_ES_ERR_APP_REGISTER);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
}

static void test_init_evs_register_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_EVS_APP_FILTER_OVERLOAD);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
}

static void test_init_pipe_create_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  0);
    will_return(CFE_SB_CreatePipe,  CFE_SB_BAD_ARGUMENT);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
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
    ORBITER_ADCS_AppMain();
}

static void test_init_subscribe_hk_mid_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);          /* CMD_MID ok */
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* HK_MID fails */
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
}

static void test_init_subscribe_mcu_rwa_hk_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* MCU_RWA_HK fails */
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
}

/* ---------------------------------------------------------------------------
 * Happy-path init
 * --------------------------------------------------------------------------- */
static void test_init_success(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
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
    will_return(CFE_MSG_GetMsgId,     ORBITER_ADCS_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_ADCS_NOOP_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
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
    will_return(CFE_MSG_GetMsgId,     ORBITER_ADCS_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_ADCS_RESET_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
}

/* ---------------------------------------------------------------------------
 * RED TEST: SET_TARGET_QUAT — invalid (non-unit) quaternion rejected
 *
 * Given:  ADCS init succeeds
 * When:   SET_TARGET_QUAT (CC=2) arrives with {w=1.0, x=1.0, y=0.0, z=0.0}
 *          norm² = 2.0; |norm² - 1.0| = 1.0 >> ORBITER_ADCS_QUAT_NORM_TOL
 * Then:   ErrCounter increments, CmdCounter unchanged
 * --------------------------------------------------------------------------- */
static void test_set_target_quat_invalid_norm_rejected(void **state)
{
    (void)state;
    ORBITER_ADCS_SetTargetQuatCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.Quat.w = 1.0f;
    cmd.Quat.x = 1.0f;
    cmd.Quat.y = 0.0f;
    cmd.Quat.z = 0.0f;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_ADCS_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_ADCS_SET_TARGET_QUAT_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
}

/* ---------------------------------------------------------------------------
 * RED TEST: SET_TARGET_QUAT — valid (unit) quaternion accepted
 *
 * Given:  ADCS init succeeds
 * When:   SET_TARGET_QUAT (CC=2) arrives with identity {w=1.0, x=0.0, y=0.0, z=0.0}
 *          norm² = 1.0; |norm² - 1.0| = 0.0 ≤ ORBITER_ADCS_QUAT_NORM_TOL
 * Then:   CmdCounter increments, ErrCounter unchanged
 * --------------------------------------------------------------------------- */
static void test_set_target_quat_valid_norm_accepted(void **state)
{
    (void)state;
    ORBITER_ADCS_SetTargetQuatCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.Quat.w = 1.0f;
    cmd.Quat.x = 0.0f;
    cmd.Quat.y = 0.0f;
    cmd.Quat.z = 0.0f;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_ADCS_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_ADCS_SET_TARGET_QUAT_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
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
    will_return(CFE_MSG_GetMsgId,     ORBITER_ADCS_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_ADCS_INVALID_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
}

/* ---------------------------------------------------------------------------
 * MID dispatch: HK request triggers two TransmitMsg calls (attitude + wheel)
 * --------------------------------------------------------------------------- */
static void test_hk_request_sends_attitude_hk(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_ADCS_HK_MID);
    will_return(CFE_SB_TransmitMsg,   CFE_SUCCESS); /* attitude HK */
    will_return(CFE_SB_TransmitMsg,   CFE_SUCCESS); /* wheel TLM */
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
}

/* ---------------------------------------------------------------------------
 * MID dispatch: MCU_RWA_HK handled without error (stub no-op until Phase 35)
 * --------------------------------------------------------------------------- */
static void test_mcu_rwa_hk_handled_without_error(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     MCU_RWA_HK_MID);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_ADCS_AppMain();
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
    ORBITER_ADCS_AppMain();
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
    ORBITER_ADCS_AppMain();
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
        cmocka_unit_test(test_init_subscribe_mcu_rwa_hk_fails),
        /* Happy-path init */
        cmocka_unit_test(test_init_success),
        /* Command dispatch */
        cmocka_unit_test(test_noop_command_increments_counter),
        cmocka_unit_test(test_reset_command_clears_counters),
        cmocka_unit_test(test_set_target_quat_invalid_norm_rejected),
        cmocka_unit_test(test_set_target_quat_valid_norm_accepted),
        cmocka_unit_test(test_unknown_command_code_increments_err_counter),
        /* MID dispatch */
        cmocka_unit_test(test_hk_request_sends_attitude_hk),
        cmocka_unit_test(test_mcu_rwa_hk_handled_without_error),
        cmocka_unit_test(test_unknown_msgid_increments_err_counter),
        /* Run loop */
        cmocka_unit_test(test_sb_receive_error_increments_err_counter),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
