/*
 * orbiter_comm_test.c — CMocka unit tests for ORBITER_COMM
 *
 * CFE API calls are intercepted via UNIT_TEST stubs below.
 * No real cFE library is linked; all CFE_* symbols resolve to stubs
 * configured per-test via CMocka's mock infrastructure.
 *
 * Coverage target: 100% branch coverage of orbiter_comm.c
 * Measure with: bash scripts/coverage-gate.sh apps/orbiter_comm
 *
 * RED tests:
 *   test_set_downlink_rate_invalid_vc_rejected — VcId out of range rejected.
 *   test_set_downlink_rate_invalid_rate_rejected — rate above ceiling rejected.
 * Both verify the primary DoD requirements (VC bounds + rate ceiling enforcement).
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "orbiter_comm.h"

#define ORBITER_COMM_INVALID_CC  ((CFE_MSG_FcnCode_t)0xFFU)

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
 * Helper: queue will_return values for a successful 2-subscription init.
 * --------------------------------------------------------------------------- */
static void queue_successful_init(void)
{
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);           /* pipe handle */
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* CMD_MID */
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* HK_MID */
}

/* ---------------------------------------------------------------------------
 * Init failure path tests
 * --------------------------------------------------------------------------- */
static void test_init_register_app_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_ES_ERR_APP_REGISTER);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
}

static void test_init_evs_register_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_EVS_APP_FILTER_OVERLOAD);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
}

static void test_init_pipe_create_fails(void **state)
{
    (void)state;
    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  0);
    will_return(CFE_SB_CreatePipe,  CFE_SB_BAD_ARGUMENT);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
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
    ORBITER_COMM_AppMain();
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
    ORBITER_COMM_AppMain();
}

/* ---------------------------------------------------------------------------
 * Happy-path init
 * --------------------------------------------------------------------------- */
static void test_init_success(void **state)
{
    (void)state;
    queue_successful_init();
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
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
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_COMM_NOOP_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
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
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_COMM_RESET_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
}

/* ---------------------------------------------------------------------------
 * RED TEST: SET_DOWNLINK_RATE — invalid VC ID rejected
 *
 * Given:  COMM init succeeds (downlink rates at defaults)
 * When:   SET_DOWNLINK_RATE (CC=2) arrives with VcId=4 (> MAX_VC_ID=3)
 * Then:   ErrCounter increments; DownlinkRateKbps[0..3] unchanged; no crash
 * --------------------------------------------------------------------------- */
static void test_set_downlink_rate_invalid_vc_rejected(void **state)
{
    (void)state;
    ORBITER_COMM_SetDownlinkRateCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.VcId     = 4U;   /* out of range: valid range is 0–3 */
    cmd.RateKbps = 800U;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_COMM_SET_DOWNLINK_RATE_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
}

/* ---------------------------------------------------------------------------
 * RED TEST: SET_DOWNLINK_RATE — rate above ceiling rejected
 *
 * Given:  COMM init succeeds; VC 2 default rate is 800 kbps
 * When:   SET_DOWNLINK_RATE (CC=2) arrives with VcId=2, RateKbps=1001
 *          (> ORBITER_COMM_MAX_RATE_KBPS=1000)
 * Then:   ErrCounter increments; DownlinkRateKbps[2] stays at 800; no crash
 * --------------------------------------------------------------------------- */
static void test_set_downlink_rate_invalid_rate_rejected(void **state)
{
    (void)state;
    ORBITER_COMM_SetDownlinkRateCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.VcId     = 2U;
    cmd.RateKbps = 1001U; /* exceeds ORBITER_COMM_MAX_RATE_KBPS = 1000 */

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_COMM_SET_DOWNLINK_RATE_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
}

/* ---------------------------------------------------------------------------
 * SET_DOWNLINK_RATE — valid: VC 2 rate updated to 900 kbps
 * --------------------------------------------------------------------------- */
static void test_set_downlink_rate_valid_updates_rate(void **state)
{
    (void)state;
    ORBITER_COMM_SetDownlinkRateCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.VcId     = 2U;
    cmd.RateKbps = 900U; /* valid: within ceiling and valid VC */

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_COMM_SET_DOWNLINK_RATE_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
}

/* ---------------------------------------------------------------------------
 * PROCESS_TCP — accepted; RTT probe echoed in HkTlm
 * --------------------------------------------------------------------------- */
static void test_process_tcp_accepted(void **state)
{
    (void)state;
    ORBITER_COMM_ProcessTcpCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.NtpStratum = 1U;
    cmd.RttProbeId = 0xDEADBEEFU;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_COMM_PROCESS_TCP_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
}

/* ---------------------------------------------------------------------------
 * PROCESS_TCP — stratum 0 (unsynced) is valid and accepted
 * --------------------------------------------------------------------------- */
static void test_process_tcp_stratum_zero_accepted(void **state)
{
    (void)state;
    ORBITER_COMM_ProcessTcpCmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.NtpStratum = 0U; /* 0 = unsynced; still a valid TCP */
    cmd.RttProbeId = 0x00000001U;

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&cmd);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_COMM_PROCESS_TCP_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
}

/* ---------------------------------------------------------------------------
 * Command dispatch: unknown command code increments ErrCounter
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
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_COMM_INVALID_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
}

/* ---------------------------------------------------------------------------
 * MID dispatch: HK request triggers TransmitMsg
 * --------------------------------------------------------------------------- */
static void test_hk_request_sends_hk(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_HK_MID);
    will_return(CFE_SB_TransmitMsg,   CFE_SUCCESS);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
}

/* ---------------------------------------------------------------------------
 * Link-state: TC received from LOS sets AOS (LINK_STATE event fired)
 *
 * Init starts in LOS. First TC (NOOP) transitions to AOS.
 * --------------------------------------------------------------------------- */
static void test_link_state_aos_on_tc_received(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    queue_successful_init();
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_COMM_NOOP_CC);
    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
}

/* ---------------------------------------------------------------------------
 * Link-state: LOS declared after ORBITER_COMM_LOS_TIMEOUT_CYCLES HK cycles
 *
 * Given:  init succeeds; NOOP sets link to AOS (idle reset to 0)
 * When:   LOS_TIMEOUT_CYCLES HK requests arrive with no intervening TC
 * Then:   LOS is declared on the Nth HK cycle (LINK_STATE_INF event emitted)
 * --------------------------------------------------------------------------- */
static void test_link_goes_los_after_timeout(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));
    uint8 i;

    queue_successful_init();

    /* Send NOOP to transition from LOS → AOS and reset idle counter */
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_COMM_NOOP_CC);

    /* Send LOS_TIMEOUT_CYCLES HK requests; the Nth increments idle to >= threshold */
    for (i = 0U; i < ORBITER_COMM_LOS_TIMEOUT_CYCLES; i++)
    {
        will_return(CFE_ES_RunLoop,       true);
        will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
        will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
        will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_HK_MID);
        will_return(CFE_SB_TransmitMsg,   CFE_SUCCESS);
    }

    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
}

/* ---------------------------------------------------------------------------
 * Link-state: already in LOS, additional HK cycles do not re-fire event
 * --------------------------------------------------------------------------- */
static void test_link_stays_los_past_timeout(void **state)
{
    (void)state;
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    /* Init starts in LOS; send LOS_TIMEOUT_CYCLES+1 HK requests — LOS event
     * fires exactly once (on the Nth) and is suppressed thereafter. */
    queue_successful_init();

    /* NOOP to set AOS */
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   ORBITER_COMM_NOOP_CC);

    /* Timeout HK cycles to trigger LOS */
    {
        uint8 i;
        for (i = 0U; i < ORBITER_COMM_LOS_TIMEOUT_CYCLES; i++)
        {
            will_return(CFE_ES_RunLoop,       true);
            will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
            will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
            will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_HK_MID);
            will_return(CFE_SB_TransmitMsg,   CFE_SUCCESS);
        }
    }

    /* One additional HK cycle; already in LOS, no second event */
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     ORBITER_COMM_HK_MID);
    will_return(CFE_SB_TransmitMsg,   CFE_SUCCESS);

    will_return(CFE_ES_RunLoop, false);
    ORBITER_COMM_AppMain();
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
    ORBITER_COMM_AppMain();
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
    ORBITER_COMM_AppMain();
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
        /* Happy-path init */
        cmocka_unit_test(test_init_success),
        /* Command dispatch */
        cmocka_unit_test(test_noop_command_increments_counter),
        cmocka_unit_test(test_reset_command_clears_counters),
        cmocka_unit_test(test_set_downlink_rate_invalid_vc_rejected),
        cmocka_unit_test(test_set_downlink_rate_invalid_rate_rejected),
        cmocka_unit_test(test_set_downlink_rate_valid_updates_rate),
        cmocka_unit_test(test_process_tcp_accepted),
        cmocka_unit_test(test_process_tcp_stratum_zero_accepted),
        cmocka_unit_test(test_unknown_command_code_increments_err_counter),
        /* MID dispatch */
        cmocka_unit_test(test_hk_request_sends_hk),
        /* Link-state machine */
        cmocka_unit_test(test_link_state_aos_on_tc_received),
        cmocka_unit_test(test_link_goes_los_after_timeout),
        cmocka_unit_test(test_link_stays_los_past_timeout),
        /* Error paths */
        cmocka_unit_test(test_unknown_msgid_increments_err_counter),
        cmocka_unit_test(test_sb_receive_error_increments_err_counter),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
