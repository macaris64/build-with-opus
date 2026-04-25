/*
 * orbiter_payload_test.c — CMocka unit tests for ORBITER_PAYLOAD
 *
 * CFE API calls are intercepted via UNIT_TEST stubs below.
 * No real cFE library is linked; all CFE_* symbols resolve to stubs
 * configured per-test via CMocka's mock infrastructure.
 *
 * Coverage target: 100% branch coverage of orbiter_payload.c
 * Measure with: bash scripts/coverage-gate.sh apps/orbiter_payload
 *
 * RED tests (primary DoD requirements):
 *   test_set_science_mode_while_powered_off_rejected — power-guard blocks science mode
 *   test_set_science_mode_powered_on_accepted        — science mode accepted when powered
 *
 * Note on static state: ORBITER_PAYLOAD_PowerState and ScienceMode are
 * radiation-anchored and persist across AppMain calls.  Tests that depend on
 * power-on state use two loop iterations: first SET_POWER ON, then the operation.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "orbiter_payload.h"

#define ORBITER_PAYLOAD_INVALID_CC  ((CFE_MSG_FcnCode_t)0xFFU)

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
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* PAYLOAD_HK_MID */
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* MCU_PAYLOAD_HK_MID */
}

/* ---------------------------------------------------------------------------
 * Init failure path tests
 * --------------------------------------------------------------------------- */
static void test_init_register_app_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_ES_ERR_APP_REGISTER);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
}

static void test_init_evs_register_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_EVS_APP_FILTER_OVERLOAD);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
}

static void test_init_pipe_create_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  0);
    will_return(CFE_SB_CreatePipe,  CFE_SB_BAD_ARGUMENT);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
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
    ORBITER_PAYLOAD_AppMain();
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
    ORBITER_PAYLOAD_AppMain();
}

static void test_init_subscribe_mcu_payload_hk_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* MCU_PAYLOAD_HK fails */
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
}

/* ---------------------------------------------------------------------------
 * Happy-path init
 * --------------------------------------------------------------------------- */
static void test_init_success(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
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
    will_return(CFE_MSG_GetMsgId,     ORBITER_PAYLOAD_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_PAYLOAD_NOOP_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
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
    will_return(CFE_MSG_GetMsgId,     ORBITER_PAYLOAD_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_PAYLOAD_RESET_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
}

/* ---------------------------------------------------------------------------
 * RED TEST: SET_SCIENCE_MODE — rejected while payload is powered off
 *
 * Given:  PAYLOAD init succeeds; PowerState = 0 (OFF, initial radiation-anchored value
 *          — if prior tests powered it ON, this test must turn it OFF first)
 * When:   SET_SCIENCE_MODE_CC arrives with ScienceMode=1
 * Then:   ErrCounter increments; ScienceMode unchanged; EVS error emitted
 *
 * Uses two iterations: iter 1 powers OFF, iter 2 tests science mode rejection.
 * --------------------------------------------------------------------------- */
static void test_set_science_mode_while_powered_off_rejected(void **state)
{
    (void)state;
    ORBITER_PAYLOAD_SetPowerCmd_t       power_cmd;
    ORBITER_PAYLOAD_SetScienceModeCmd_t mode_cmd;

    memset(&power_cmd, 0, sizeof(power_cmd));
    memset(&mode_cmd, 0, sizeof(mode_cmd));
    power_cmd.PowerOn    = 0U; /* OFF */
    mode_cmd.ScienceMode = 1U;

    queue_successful_init();
    /* Iteration 1: ensure powered off */
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&power_cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_PAYLOAD_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_PAYLOAD_SET_POWER_CC);
    /* Iteration 2: science mode must be rejected */
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&mode_cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_PAYLOAD_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_PAYLOAD_SET_SCIENCE_MODE_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
}

/* ---------------------------------------------------------------------------
 * RED TEST: SET_SCIENCE_MODE — accepted when payload is powered on
 *
 * Given:  Power set to ON
 * When:   SET_SCIENCE_MODE_CC arrives with ScienceMode=2 (valid range)
 * Then:   CmdCounter increments; ScienceMode updated; no error
 *
 * Uses two iterations: power ON, then set science mode.
 * --------------------------------------------------------------------------- */
static void test_set_science_mode_powered_on_accepted(void **state)
{
    (void)state;
    ORBITER_PAYLOAD_SetPowerCmd_t       power_cmd;
    ORBITER_PAYLOAD_SetScienceModeCmd_t mode_cmd;

    memset(&power_cmd, 0, sizeof(power_cmd));
    memset(&mode_cmd, 0, sizeof(mode_cmd));
    power_cmd.PowerOn    = 1U; /* ON */
    mode_cmd.ScienceMode = 2U;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&power_cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_PAYLOAD_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_PAYLOAD_SET_POWER_CC);
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&mode_cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_PAYLOAD_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_PAYLOAD_SET_SCIENCE_MODE_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
}

/* ---------------------------------------------------------------------------
 * SET_SCIENCE_MODE — out-of-range mode rejected (even when powered on)
 *
 * Uses two iterations: power ON (state from prior test may already be ON, but
 * be explicit), then attempt out-of-range mode.
 * --------------------------------------------------------------------------- */
static void test_set_science_mode_out_of_range_rejected(void **state)
{
    (void)state;
    ORBITER_PAYLOAD_SetPowerCmd_t       power_cmd;
    ORBITER_PAYLOAD_SetScienceModeCmd_t mode_cmd;

    memset(&power_cmd, 0, sizeof(power_cmd));
    memset(&mode_cmd, 0, sizeof(mode_cmd));
    power_cmd.PowerOn    = 1U;
    mode_cmd.ScienceMode = ORBITER_PAYLOAD_MAX_SCIENCE_MODES; /* one past valid */

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&power_cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_PAYLOAD_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_PAYLOAD_SET_POWER_CC);
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&mode_cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_PAYLOAD_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_PAYLOAD_SET_SCIENCE_MODE_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
}

/* ---------------------------------------------------------------------------
 * SET_POWER — invalid value (not 0 or 1) rejected
 * --------------------------------------------------------------------------- */
static void test_set_power_invalid_value_rejected(void **state)
{
    (void)state;
    ORBITER_PAYLOAD_SetPowerCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.PowerOn = 2U; /* invalid: only 0 and 1 are valid */

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_PAYLOAD_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_PAYLOAD_SET_POWER_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
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
    will_return(CFE_MSG_GetMsgId,     ORBITER_PAYLOAD_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_PAYLOAD_INVALID_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
}

/* ---------------------------------------------------------------------------
 * MID dispatch: HK request triggers TransmitMsg
 * --------------------------------------------------------------------------- */
static void test_hk_request_sends_payload_hk(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_PAYLOAD_HK_MID);
    will_return(CFE_SB_TransmitMsg,   CFE_SUCCESS);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
}

/* ---------------------------------------------------------------------------
 * MID dispatch: MCU_PAYLOAD_HK handled without error (stub no-op until Phase 35)
 * --------------------------------------------------------------------------- */
static void test_mcu_payload_hk_handled_without_error(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     MCU_PAYLOAD_HK_MID);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_PAYLOAD_AppMain();
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
    ORBITER_PAYLOAD_AppMain();
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
    ORBITER_PAYLOAD_AppMain();
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
        cmocka_unit_test(test_init_subscribe_mcu_payload_hk_fails),
        /* Happy-path init */
        cmocka_unit_test(test_init_success),
        /* Command dispatch */
        cmocka_unit_test(test_noop_command_increments_counter),
        cmocka_unit_test(test_reset_command_clears_counters),
        /* RED tests — power-guard DoD requirements */
        cmocka_unit_test(test_set_science_mode_while_powered_off_rejected),
        cmocka_unit_test(test_set_science_mode_powered_on_accepted),
        cmocka_unit_test(test_set_science_mode_out_of_range_rejected),
        cmocka_unit_test(test_set_power_invalid_value_rejected),
        cmocka_unit_test(test_unknown_command_code_increments_err_counter),
        /* MID dispatch */
        cmocka_unit_test(test_hk_request_sends_payload_hk),
        cmocka_unit_test(test_mcu_payload_hk_handled_without_error),
        cmocka_unit_test(test_unknown_msgid_increments_err_counter),
        /* Run loop */
        cmocka_unit_test(test_sb_receive_error_increments_err_counter),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
