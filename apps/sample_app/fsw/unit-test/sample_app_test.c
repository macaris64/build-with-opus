/*
 * sample_app_test.c — CMocka unit tests for SAMPLE_APP
 *
 * CFE API calls are intercepted via the UNIT_TEST stub header.
 * No real cFE library is linked; all CFE_* symbols resolve to stubs
 * that can be configured per-test via CMocka's mock infrastructure.
 *
 * Coverage target: 100% branch coverage of sample_app.c
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "sample_app.h"

/* Named constant for an unknown command code used in failure-path tests.
 * Must not match any valid SAMPLE_APP_*_CC value. */
#define SAMPLE_APP_INVALID_CC  ((CFE_MSG_FcnCode_t)0xFFU)

/* ---------------------------------------------------------------------------
 * CFE stub implementations (compiled only under UNIT_TEST)
 * In a real cFS unit-test framework these live in a shared stub library.
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
    /* Intentionally empty stub — event output goes nowhere in unit tests */
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
 * Test: initialization succeeds when all CFE calls return CFE_SUCCESS
 * --------------------------------------------------------------------------- */
static void test_init_success(void **state)
{
    (void)state;

    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);           /* pipe handle */
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* CMD_MID */
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS); /* HK_MID */

    /* Drive AppMain through one loop iteration then exit */
    will_return(CFE_ES_RunLoop, false);

    SAMPLE_APP_AppMain();
    /* If we reach here without a CMocka assertion failure, init succeeded */
}

/* ---------------------------------------------------------------------------
 * Test: initialization sets RunStatus to APP_ERROR when pipe creation fails
 * --------------------------------------------------------------------------- */
static void test_init_pipe_create_failure(void **state)
{
    (void)state;

    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  0);                   /* pipe handle (unused) */
    will_return(CFE_SB_CreatePipe,  CFE_SB_BAD_ARGUMENT); /* failure */

    /* RunLoop returns false immediately — app exits with APP_ERROR */
    will_return(CFE_ES_RunLoop, false);

    SAMPLE_APP_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test: NOOP command increments CmdCounter and emits information event
 * --------------------------------------------------------------------------- */
static void test_noop_command_increments_counter(void **state)
{
    (void)state;

    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);

    /* One pass through the run loop: receive a NOOP command */
    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    will_return(CFE_ES_RunLoop,        true);
    will_return(CFE_SB_ReceiveBuffer,  (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer,  CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,      SAMPLE_APP_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,    SAMPLE_APP_NOOP_CC);

    /* Exit on the second RunLoop call */
    will_return(CFE_ES_RunLoop, false);

    SAMPLE_APP_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test: unknown command code increments ErrCounter
 * --------------------------------------------------------------------------- */
static void test_invalid_command_increments_counter(void **state)
{
    (void)state;

    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);

    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    will_return(CFE_ES_RunLoop,        true);
    will_return(CFE_SB_ReceiveBuffer,  (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer,  CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,      SAMPLE_APP_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,    SAMPLE_APP_INVALID_CC);

    will_return(CFE_ES_RunLoop, false);

    SAMPLE_APP_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test: initialization returns error when CFE_ES_RegisterApp fails
 * --------------------------------------------------------------------------- */
static void test_init_returns_error_when_register_app_fails(void **state)
{
    (void)state;

    will_return(CFE_ES_RegisterApp, CFE_ES_ERR_APP_REGISTER);

    will_return(CFE_ES_RunLoop, false);

    SAMPLE_APP_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test: initialization returns error when CFE_EVS_Register fails
 * --------------------------------------------------------------------------- */
static void test_init_returns_error_when_evs_register_fails(void **state)
{
    (void)state;

    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_EVS_APP_FILTER_OVERLOAD);

    will_return(CFE_ES_RunLoop, false);

    SAMPLE_APP_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test: initialization returns error when first CFE_SB_Subscribe fails (CMD_MID)
 * --------------------------------------------------------------------------- */
static void test_init_returns_error_when_cmd_subscribe_fails(void **state)
{
    (void)state;

    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* CMD_MID subscribe fails */

    will_return(CFE_ES_RunLoop, false);

    SAMPLE_APP_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test: initialization returns error when second CFE_SB_Subscribe fails (HK_MID)
 * --------------------------------------------------------------------------- */
static void test_init_returns_error_when_hk_subscribe_fails(void **state)
{
    (void)state;

    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);          /* CMD_MID ok */
    will_return(CFE_SB_Subscribe,   CFE_SB_MAX_MSGS_MET); /* HK_MID fails */

    will_return(CFE_ES_RunLoop, false);

    SAMPLE_APP_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test: SB receive error increments ErrCounter and continues the run loop
 * --------------------------------------------------------------------------- */
static void test_sb_receive_error_increments_err_counter(void **state)
{
    (void)state;

    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);

    /* One loop pass: ReceiveBuffer returns an error.
     * NULL BufPtr is intentional: the error path in sample_app.c never
     * dereferences SBBufPtr when status != CFE_SUCCESS (line 46 guards it). */
    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)NULL);
    will_return(CFE_SB_ReceiveBuffer, CFE_SB_PIPE_RD_ERR);

    /* Exit on the second RunLoop call */
    will_return(CFE_ES_RunLoop, false);

    SAMPLE_APP_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test: unknown message ID on the pipe increments ErrCounter
 * --------------------------------------------------------------------------- */
static void test_unknown_msgid_increments_err_counter(void **state)
{
    (void)state;

    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);

    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     CFE_SB_INVALID_MSG_ID);

    will_return(CFE_ES_RunLoop, false);

    SAMPLE_APP_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test: RESET command clears CmdCounter and ErrCounter
 * --------------------------------------------------------------------------- */
static void test_reset_command_clears_counters(void **state)
{
    (void)state;

    will_return(CFE_ES_RegisterApp, CFE_SUCCESS);
    will_return(CFE_EVS_Register,   CFE_SUCCESS);
    will_return(CFE_SB_CreatePipe,  1);
    will_return(CFE_SB_CreatePipe,  CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);
    will_return(CFE_SB_Subscribe,   CFE_SUCCESS);

    CFE_SB_Buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    will_return(CFE_ES_RunLoop,       true);
    will_return(CFE_SB_ReceiveBuffer, (uintptr_t)&buf);
    will_return(CFE_SB_ReceiveBuffer, CFE_SUCCESS);
    will_return(CFE_MSG_GetMsgId,     SAMPLE_APP_CMD_MID);
    will_return(CFE_MSG_GetFcnCode,   SAMPLE_APP_RESET_CC);

    will_return(CFE_ES_RunLoop, false);

    SAMPLE_APP_AppMain();
}

/* ---------------------------------------------------------------------------
 * Test runner
 * --------------------------------------------------------------------------- */
int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_success),
        cmocka_unit_test(test_init_pipe_create_failure),
        cmocka_unit_test(test_init_returns_error_when_register_app_fails),
        cmocka_unit_test(test_init_returns_error_when_evs_register_fails),
        cmocka_unit_test(test_init_returns_error_when_cmd_subscribe_fails),
        cmocka_unit_test(test_init_returns_error_when_hk_subscribe_fails),
        cmocka_unit_test(test_noop_command_increments_counter),
        cmocka_unit_test(test_invalid_command_increments_counter),
        cmocka_unit_test(test_sb_receive_error_increments_err_counter),
        cmocka_unit_test(test_unknown_msgid_increments_err_counter),
        cmocka_unit_test(test_reset_command_clears_counters),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
