#ifndef CFE_H
#define CFE_H

/*
 * cfe.h — Minimal cFE type and constant stubs for standalone builds.
 *
 * Extends the shared stub pattern established in apps/sample_app/fsw/src/cfe.h
 * with CFE_SB_TransmitMsg, required by orbiter_comm's HK publication path.
 *
 * MISRA C:2012 Rule 20.5 deviation: conditionally compiled; superseded by the
 * real cfe.h when cFS is present on the include path ahead of this stub.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

/* ── Primitive typedefs (mirror cFE common_types.h) ─────────────────────── */
typedef int32_t  int32;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint8_t  uint8;

/* ── Software Bus types ──────────────────────────────────────────────────── */
typedef uintptr_t           CFE_SB_PipeId_t;
typedef uint32_t            CFE_SB_MsgId_Atom_t;
typedef CFE_SB_MsgId_Atom_t CFE_SB_MsgId_t;

/* CCSDS primary-header minimum (6 bytes) plus secondary-header stub */
typedef struct { uint8 Byte[16]; } CFE_MSG_Message_t;
typedef struct { CFE_MSG_Message_t Msg; } CFE_SB_Buffer_t;
typedef uint16_t CFE_MSG_FcnCode_t;

/* ── Status codes ────────────────────────────────────────────────────────── */
#define CFE_SUCCESS                  ((int32) 0)
#define CFE_SB_BAD_ARGUMENT          ((int32)-1)
#define CFE_SB_PIPE_RD_ERR           ((int32)-2)
#define CFE_SB_MAX_MSGS_MET          ((int32)-3)
#define CFE_ES_ERR_APP_REGISTER      ((int32)-4)
#define CFE_EVS_APP_FILTER_OVERLOAD  ((int32)-5)

/* ── Executive Services constants ───────────────────────────────────────── */
#define CFE_ES_RunStatus_APP_RUN    1U
#define CFE_ES_RunStatus_APP_ERROR  2U
#define CFE_ES_RunStatus_APP_EXIT   3U

/* ── Software Bus constants ─────────────────────────────────────────────── */
#define CFE_SB_PEND_FOREVER         ((int32)-1)
#define CFE_SB_INVALID_MSG_ID       ((CFE_SB_MsgId_t)0xFFFFU)

/* ── Event Services constants ────────────────────────────────────────────── */
#define CFE_EVS_EventFilter_BINARY  1U
#define CFE_EVS_EventType_INFORMATION 1U
#define CFE_EVS_EventType_ERROR       4U

/* ── API declarations (implemented by cFE or by UNIT_TEST stubs) ─────────── */
int32 CFE_ES_RegisterApp(void);
bool  CFE_ES_RunLoop(uint32 *RunStatus);
void  CFE_ES_ExitApp(uint32 ExitStatus);

int32 CFE_EVS_Register(const void *Filters, uint16 NumFilters, uint16 FilterScheme);
void  CFE_EVS_SendEvent(uint16 EventID, uint16 EventType, const char *Spec, ...);

int32 CFE_SB_CreatePipe(CFE_SB_PipeId_t *PipeIdPtr, uint16 Depth, const char *PipeName);
int32 CFE_SB_Subscribe(CFE_SB_MsgId_t MsgId, CFE_SB_PipeId_t PipeId);
int32 CFE_SB_ReceiveBuffer(CFE_SB_Buffer_t **BufPtr, CFE_SB_PipeId_t PipeId, int32 TimeOut);
int32 CFE_SB_TransmitMsg(CFE_MSG_Message_t *MsgPtr, bool IncrementSequenceCount);
CFE_SB_MsgId_Atom_t CFE_SB_MsgIdToValue(CFE_SB_MsgId_t MsgId);
CFE_SB_MsgId_t      CFE_SB_ValueToMsgId(CFE_SB_MsgId_Atom_t MsgIdValue);

int32 CFE_MSG_GetMsgId(const CFE_MSG_Message_t *MsgPtr, CFE_SB_MsgId_t *MsgId);
int32 CFE_MSG_GetFcnCode(const CFE_MSG_Message_t *MsgPtr, CFE_MSG_FcnCode_t *FcnCode);

#endif /* CFE_H */
