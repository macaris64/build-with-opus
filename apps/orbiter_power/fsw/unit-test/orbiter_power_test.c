/*
 * orbiter_power_test.c — CMocka unit tests for ORBITER_POWER
 *
 * CFE API calls are intercepted via UNIT_TEST stubs below.
 * No real cFE library is linked; all CFE_* symbols resolve to stubs
 * configured per-test via CMocka's mock infrastructure.
 *
 * Coverage target: 100% branch coverage of orbiter_power.c
 * Measure with: bash scripts/coverage-gate.sh apps/orbiter_power
 *
 * RED tests (primary DoD requirements):
 *   test_load_switch_prohibited_in_safe_mode_rejected — interlock rejects switch
 *   test_load_switch_allowed_in_normal_mode_accepted  — interlock accepts switch
 *
 * Note on static state: ORBITER_POWER_CurrentMode is radiation-anchored and
 * persists across AppMain calls within a test binary run.  Tests that depend
 * on a specific mode use two loop iterations: first SET_POWER_MODE, then the
 * operation under test.  Tests using Load 0 (ProhibitMask=0) are mode-invariant.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "orbiter_power.h"

#define ORBITER_POWER_INVALID_CC  ((CFE_MSG_FcnCode_t)0xFFU)

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
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* POWER_HK_MID */
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* MCU_EPS_HK_MID */
}

/* ---------------------------------------------------------------------------
 * Init failure path tests
 * --------------------------------------------------------------------------- */
static void test_init_register_app_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_ES_ERR_APP_REGISTER);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

static void test_init_evs_register_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_EVS_APP_FILTER_OVERLOAD);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

static void test_init_pipe_create_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  0);
    will_return(CFE_SB_CreatePipe,  CFE_SB_BAD_ARGUMENT);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
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
    ORBITER_POWER_AppMain();
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
    ORBITER_POWER_AppMain();
}

static void test_init_subscribe_mcu_eps_hk_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* MCU_EPS_HK fails */
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

/* ---------------------------------------------------------------------------
 * Happy-path init
 * --------------------------------------------------------------------------- */
static void test_init_success(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
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
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_NOOP_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
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
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_RESET_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

/* ---------------------------------------------------------------------------
 * RED TEST: LOAD_SWITCH — unrestricted load (Load 0) allowed in any mode
 *
 * Given:  POWER init succeeds; Load 0 has ProhibitMask=0 (no restrictions)
 * When:   LOAD_SWITCH_CC arrives with LoadId=0, Action=ON
 * Then:   CmdCounter increments; LoadState[0] = ON; no error
 * --------------------------------------------------------------------------- */
static void test_load_switch_unrestricted_load_accepted(void **state)
{
    (void)state;
    ORBITER_POWER_LoadSwitchCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.LoadId = 0U;
    cmd.Action = ORBITER_POWER_LOAD_ON;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_LOAD_SWITCH_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

/* ---------------------------------------------------------------------------
 * RED TEST: LOAD_SWITCH — interlock rejects switch in SAFE mode
 *
 * Given:  POWER init succeeds; power mode set to SAFE (1)
 *          Load 1 has ProhibitMask bit 1 set → prohibited in SAFE
 * When:   LOAD_SWITCH_CC arrives with LoadId=1, Action=ON
 * Then:   ErrCounter increments; load state unchanged; EVS error emitted
 *
 * Uses two loop iterations: first sets mode to SAFE, then attempts the switch.
 * --------------------------------------------------------------------------- */
static void test_load_switch_prohibited_in_safe_mode_rejected(void **state)
{
    (void)state;
    ORBITER_POWER_SetPowerModeCmd_t mode_cmd;
    ORBITER_POWER_LoadSwitchCmd_t   sw_cmd;

    memset(&mode_cmd, 0, sizeof(mode_cmd));
    memset(&sw_cmd, 0, sizeof(sw_cmd));
    mode_cmd.ModeId = ORBITER_POWER_MODE_SAFE;
    sw_cmd.LoadId   = 1U;
    sw_cmd.Action   = ORBITER_POWER_LOAD_ON;

    queue_successful_init();
    /* Iteration 1: set power mode to SAFE */
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&mode_cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_SET_POWER_MODE_CC);
    /* Iteration 2: attempt Load 1 switch — interlock must block */
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&sw_cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_LOAD_SWITCH_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

/* ---------------------------------------------------------------------------
 * LOAD_SWITCH — interlock rejects Load 2 in ECLIPSE mode
 *
 * Given:  Power mode set to ECLIPSE (2)
 *          Load 2 has ProhibitMask bits 1|2 → prohibited in SAFE and ECLIPSE
 * When:   LOAD_SWITCH_CC arrives with LoadId=2, Action=ON
 * Then:   ErrCounter increments
 *
 * Uses two iterations: set ECLIPSE, then attempt switch.
 * --------------------------------------------------------------------------- */
static void test_load_switch_prohibited_in_eclipse_mode_rejected(void **state)
{
    (void)state;
    ORBITER_POWER_SetPowerModeCmd_t mode_cmd;
    ORBITER_POWER_LoadSwitchCmd_t   sw_cmd;

    memset(&mode_cmd, 0, sizeof(mode_cmd));
    memset(&sw_cmd, 0, sizeof(sw_cmd));
    mode_cmd.ModeId = ORBITER_POWER_MODE_ECLIP;
    sw_cmd.LoadId   = 2U;
    sw_cmd.Action   = ORBITER_POWER_LOAD_ON;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&mode_cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_SET_POWER_MODE_CC);
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&sw_cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_LOAD_SWITCH_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

/* ---------------------------------------------------------------------------
 * LOAD_SWITCH — Load 2 allowed in NORMAL mode (ProhibitMask does not cover mode 0)
 *
 * Uses two iterations: restore mode to NORMAL, then accept switch.
 * --------------------------------------------------------------------------- */
static void test_load_switch_allowed_in_normal_mode_accepted(void **state)
{
    (void)state;
    ORBITER_POWER_SetPowerModeCmd_t mode_cmd;
    ORBITER_POWER_LoadSwitchCmd_t   sw_cmd;

    memset(&mode_cmd, 0, sizeof(mode_cmd));
    memset(&sw_cmd, 0, sizeof(sw_cmd));
    mode_cmd.ModeId = ORBITER_POWER_MODE_NORMAL;
    sw_cmd.LoadId   = 2U;
    sw_cmd.Action   = ORBITER_POWER_LOAD_ON;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&mode_cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_SET_POWER_MODE_CC);
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&sw_cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_LOAD_SWITCH_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

/* ---------------------------------------------------------------------------
 * LOAD_SWITCH — turn a load OFF (exercises the "OFF" branch in the EVS log)
 * --------------------------------------------------------------------------- */
static void test_load_switch_turn_off_accepted(void **state)
{
    (void)state;
    ORBITER_POWER_LoadSwitchCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.LoadId = 0U;
    cmd.Action = ORBITER_POWER_LOAD_OFF;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_LOAD_SWITCH_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

/* ---------------------------------------------------------------------------
 * LOAD_SWITCH — out-of-bounds LoadId rejected
 * --------------------------------------------------------------------------- */
static void test_load_switch_out_of_bounds_rejected(void **state)
{
    (void)state;
    ORBITER_POWER_LoadSwitchCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.LoadId = 0xFFU; /* well beyond ORBITER_POWER_MAX_LOADS */
    cmd.Action = ORBITER_POWER_LOAD_ON;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_LOAD_SWITCH_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

/* ---------------------------------------------------------------------------
 * SET_POWER_MODE — valid mode accepted
 * --------------------------------------------------------------------------- */
static void test_set_power_mode_valid_accepted(void **state)
{
    (void)state;
    ORBITER_POWER_SetPowerModeCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.ModeId = ORBITER_POWER_MODE_NORMAL;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_SET_POWER_MODE_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

/* ---------------------------------------------------------------------------
 * SET_POWER_MODE — invalid mode rejected
 * --------------------------------------------------------------------------- */
static void test_set_power_mode_invalid_rejected(void **state)
{
    (void)state;
    ORBITER_POWER_SetPowerModeCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.ModeId = ORBITER_POWER_MODE_COUNT; /* one past valid range */

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_SET_POWER_MODE_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
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
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_POWER_INVALID_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

/* ---------------------------------------------------------------------------
 * MID dispatch: HK request triggers TransmitMsg
 * --------------------------------------------------------------------------- */
static void test_hk_request_sends_eps_hk(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_POWER_HK_MID);
    will_return(CFE_SB_TransmitMsg,   CFE_SUCCESS);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
}

/* ---------------------------------------------------------------------------
 * MID dispatch: MCU_EPS_HK handled without error (stub no-op until Phase 35)
 * --------------------------------------------------------------------------- */
static void test_mcu_eps_hk_handled_without_error(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     MCU_EPS_HK_MID);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_POWER_AppMain();
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
    ORBITER_POWER_AppMain();
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
    ORBITER_POWER_AppMain();
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
        cmocka_unit_test(test_init_subscribe_mcu_eps_hk_fails),
        /* Happy-path init */
        cmocka_unit_test(test_init_success),
        /* Command dispatch */
        cmocka_unit_test(test_noop_command_increments_counter),
        cmocka_unit_test(test_reset_command_clears_counters),
        /* RED tests — interlock DoD requirements */
        cmocka_unit_test(test_load_switch_unrestricted_load_accepted),
        cmocka_unit_test(test_load_switch_turn_off_accepted),
        cmocka_unit_test(test_load_switch_prohibited_in_safe_mode_rejected),
        cmocka_unit_test(test_load_switch_prohibited_in_eclipse_mode_rejected),
        cmocka_unit_test(test_load_switch_allowed_in_normal_mode_accepted),
        cmocka_unit_test(test_load_switch_out_of_bounds_rejected),
        cmocka_unit_test(test_set_power_mode_valid_accepted),
        cmocka_unit_test(test_set_power_mode_invalid_rejected),
        cmocka_unit_test(test_unknown_command_code_increments_err_counter),
        /* MID dispatch */
        cmocka_unit_test(test_hk_request_sends_eps_hk),
        cmocka_unit_test(test_mcu_eps_hk_handled_without_error),
        cmocka_unit_test(test_unknown_msgid_increments_err_counter),
        /* Run loop */
        cmocka_unit_test(test_sb_receive_error_increments_err_counter),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
