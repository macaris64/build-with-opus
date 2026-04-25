#ifndef ORBITER_COMM_H
#define ORBITER_COMM_H

/*
 * orbiter_comm.h — Radio manager and CFDP Class 1 comm app for SAKURA-II orbiter.
 *
 * Drives AOS VC 0/1/2/3 downlink emission, tracks link budget, processes
 * downlink rate control commands (TC MID 0x1983/CC2) and Time Correlation
 * Packets (TC 0x1983/CC3).  Manages a CFDP Class 1 transaction table (up to
 * ORBITER_COMM_MAX_CFDP_TXNS concurrent entries).
 *
 * Source of truth: docs/architecture/01-orbiter-cfs.md §3.2 (orbiter_comm);
 *                  docs/architecture/07-comms-stack.md §3 (AOS VC), §5 (CFDP);
 *                  docs/interfaces/ICD-orbiter-ground.md §2.2 (rate budget), §4, §5.
 * SYS-REQ-0023 (CFDP Class 1 downlink capability); SYS-REQ-0025 (link budget).
 *
 * Compliance notes:
 *   [Q-C8] RttProbeId consumed via cfs_bindings struct (no raw from_be_bytes).
 *   [Q-F3] CFDP table + LinkState anchored in .critical_mem.
 *   [Q-F4] TimeSuspect seam reserved for Phase 43.
 */

#include "cfe.h"
#include "orbiter_comm_events.h"
#include "orbiter_comm_version.h"
#include "mids.h"

/* ── Command codes (TC MID: ORBITER_COMM_CMD_MID = 0x1983) ──────────────── */
#define ORBITER_COMM_NOOP_CC              ((CFE_MSG_FcnCode_t)0U)
#define ORBITER_COMM_RESET_CC             ((CFE_MSG_FcnCode_t)1U)
#define ORBITER_COMM_SET_DOWNLINK_RATE_CC ((CFE_MSG_FcnCode_t)2U)
#define ORBITER_COMM_PROCESS_TCP_CC       ((CFE_MSG_FcnCode_t)3U)

/* ── Pipe and protocol limits ────────────────────────────────────────────── */
#define ORBITER_COMM_PIPE_DEPTH          ((uint16)20U)
#define ORBITER_COMM_MAX_CFDP_TXNS       16U   /* max concurrent CFDP Class 1 transactions */
#define ORBITER_COMM_MAX_RATE_KBPS       1000U  /* total downlink budget ceiling (kbps) */
#define ORBITER_COMM_MAX_VC_ID           3U     /* valid VC IDs: 0–3 (per ICD §2.2) */
/* HK cycles at 1 Hz without uplink before LOS declared (ICD §ground-LOS > 10 s) */
#define ORBITER_COMM_LOS_TIMEOUT_CYCLES  10U
#define ORBITER_COMM_NUM_VCS             4U     /* VC 0, 1, 2, 3 */

/* ── Link state values ───────────────────────────────────────────────────── */
#define ORBITER_COMM_LINK_LOS  0U
#define ORBITER_COMM_LINK_AOS  1U

/* ── CFDP transaction state machine values ───────────────────────────────── */
#define ORBITER_COMM_CFDP_STATE_IDLE     0U
#define ORBITER_COMM_CFDP_STATE_ACTIVE   1U
#define ORBITER_COMM_CFDP_STATE_COMPLETE 2U

/* ── Default per-VC downlink rates (kbps) per ICD-orbiter-ground.md §2.2 ── */
#define ORBITER_COMM_DEFAULT_RATE_VC0  50U    /* real-time HK */
#define ORBITER_COMM_DEFAULT_RATE_VC1  20U    /* event log */
#define ORBITER_COMM_DEFAULT_RATE_VC2  800U   /* CFDP bulk */
#define ORBITER_COMM_DEFAULT_RATE_VC3  100U   /* rover-forward */

/* ── CFDP transaction record ─────────────────────────────────────────────── */
typedef struct
{
    uint32 TransactionId;
    uint32 BytesReceived;
    uint32 TotalFileSize;
    uint8  State;       /* ORBITER_COMM_CFDP_STATE_* */
    uint8  Padding[3];  /* explicit padding (MISRA C:2012 Rule 6.7) */
} ORBITER_COMM_CfdpTxn_t;

/* ── SET_DOWNLINK_RATE command payload ───────────────────────────────────── */
typedef struct
{
    CFE_MSG_Message_t CmdHeader;
    uint8             VcId;      /* target VC: 0–3 */
    uint8             Reserved;
    uint16            RateKbps;  /* requested nominal rate in kbps */
} ORBITER_COMM_SetDownlinkRateCmd_t;

/*
 * PROCESS_TCP command payload — Time Correlation Packet per ICD §4.2.
 *
 * Q-C8: RttProbeId is a 4-byte big-endian field on the wire.  The member is
 * stored as-is from the SB buffer whose byte order matches the wire; endianness
 * resolution is cfs_bindings' responsibility.  No direct from_be_bytes call is
 * made in C-FSW — see decisions-log.md [Q-C8].
 */
typedef struct
{
    CFE_MSG_Message_t CmdHeader;
    uint8             GroundTaiCuc[7]; /* CUC-encoded TAI at ground emission */
    uint32            RttProbeId;      /* BE echo token (Q-C8: see above) */
    uint8             NtpStratum;      /* ground NTP stratum (0 = unsynced) */
    uint8             Reserved;
} ORBITER_COMM_ProcessTcpCmd_t;

/* ── HK telemetry (APID 0x120, MID 0x0920) ──────────────────────────────── */
typedef struct
{
    CFE_MSG_Message_t Header;
    uint8             LinkState;          /* ORBITER_COMM_LINK_LOS / AOS */
    uint8             CfdpActiveTxns;     /* count of ACTIVE entries in CFDP table */
    /* Q-F4: TimeSuspect reserved; time_suspect propagation deferred to Phase 43. */
    uint8             TimeSuspect;
    uint8             Padding;            /* explicit padding (MISRA C:2012 Rule 6.7) */
    uint16            DownlinkRateKbps[ORBITER_COMM_NUM_VCS]; /* per-VC: VC0–VC3 */
    uint32            CmdCounter;
    uint32            ErrCounter;
    uint32            RttProbeIdEcho;     /* last accepted RttProbeId echoed for RTT */
    uint32            CfdpBytesTotal;     /* cumulative CFDP bytes this session */
} ORBITER_COMM_HkTlm_t;

/* ── Application state ───────────────────────────────────────────────────── */
typedef struct
{
    uint32               RunStatus;
    CFE_SB_PipeId_t      CmdPipe;
    uint32               CmdCounter;
    uint32               ErrCounter;
    uint32               UplinkIdleCount; /* HK cycles without TC; reset on any TC */
    ORBITER_COMM_HkTlm_t HkTlm;
} ORBITER_COMM_Data_t;

/* ── Ground station AOS output (Phase D: cFS↔Rust integration) ──────────── */
/* Destination for UDP AOS-frame emission during AOS link state. Override
 * at build time via -DORBITER_COMM_GS_HOST='"x.x.x.x"' if needed. */
#ifndef ORBITER_COMM_GS_HOST
#define ORBITER_COMM_GS_HOST       "127.0.0.1"
#endif
#define ORBITER_COMM_GS_PORT       10000U
#define ORBITER_COMM_AOS_FRAME_LEN 1024U

/* ── Entry point ─────────────────────────────────────────────────────────── */
void ORBITER_COMM_AppMain(void);

#endif /* ORBITER_COMM_H */
