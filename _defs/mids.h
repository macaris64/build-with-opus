#ifndef MIDS_H
#define MIDS_H

/* MID derivation formula (cFE v1 scheme):
 *   TM (telemetry, spacecraft → ground): MID = 0x0800 | APID
 *   TC (command,   ground → spacecraft): MID = 0x1800 | APID
 *
 * Source of truth: docs/interfaces/apid-registry.md §cFE Message ID (MID) Scheme.
 * APID allocation ranges: docs/interfaces/apid-registry.md §APID Allocation.
 *
 * Change control: add a row to docs/interfaces/apid-registry.md BEFORE adding
 * a macro here (see §Change Control). */

/* ── Orbiter TM MIDs (APID block 0x100–0x17F) ─────────────────────────────── */
#define SAMPLE_APP_HK_MID        0x0900U   /* 0x0800 | 0x100 */
#define ORBITER_CDH_HK_MID       0x0901U   /* 0x0800 | 0x101 */
#define ORBITER_ADCS_HK_MID      0x0910U   /* 0x0800 | 0x110 */
#define ORBITER_COMM_HK_MID      0x0920U   /* 0x0800 | 0x120 */
#define ORBITER_POWER_HK_MID     0x0930U   /* 0x0800 | 0x130 */
#define ORBITER_PAYLOAD_HK_MID   0x0940U   /* 0x0800 | 0x140 */

/* ── Orbiter TC MIDs (APID block 0x180–0x1FF) ─────────────────────────────── */
#define SAMPLE_APP_CMD_MID       0x1980U   /* 0x1800 | 0x180 */
#define ORBITER_CDH_CMD_MID      0x1981U   /* 0x1800 | 0x181 */
#define ORBITER_ADCS_CMD_MID     0x1982U   /* 0x1800 | 0x182 */
#define ORBITER_COMM_CMD_MID     0x1983U   /* 0x1800 | 0x183 */
#define ORBITER_POWER_CMD_MID    0x1984U   /* 0x1800 | 0x184 */
#define ORBITER_PAYLOAD_CMD_MID  0x1985U   /* 0x1800 | 0x185 */

/* ── Relay TM/TC block anchors (0x200–0x23F TM, 0x240–0x27F TC) ───────────── */
#define RELAY_HK_MID             0x0A00U   /* 0x0800 | 0x200 */
#define RELAY_CMD_MID            0x1A40U   /* 0x1800 | 0x240 */

/* ── Subsystem-MCU MIDs (bidirectional block anchors, 0x280–0x2FF) ─────────── */
#define MCU_PAYLOAD_HK_MID       0x0A80U   /* 0x0800 | 0x280 — SpaceWire gateway */
#define MCU_PAYLOAD_CMD_MID      0x1A80U   /* 0x1800 | 0x280 */
#define MCU_RWA_HK_MID           0x0A90U   /* 0x0800 | 0x290 — CAN gateway */
#define MCU_RWA_CMD_MID          0x1A90U   /* 0x1800 | 0x290 */
#define MCU_EPS_HK_MID           0x0AA0U   /* 0x0800 | 0x2A0 — UART/HDLC gateway */
#define MCU_EPS_CMD_MID          0x1AA0U   /* 0x1800 | 0x2A0 */

/* ── Rover block anchors ────────────────────────────────────────────────────── */
#define ROVER_LAND_HK_MID        0x0B00U   /* 0x0800 | 0x300 */
#define ROVER_LAND_CMD_MID       0x1B80U   /* 0x1800 | 0x380 */
#define ROVER_UAV_HK_MID         0x0BC0U   /* 0x0800 | 0x3C0 */
#define ROVER_UAV_CMD_MID        0x1BC0U   /* 0x1800 | 0x3C0 */
#define ROVER_CRYOBOT_HK_MID     0x0C00U   /* 0x0800 | 0x400 */
#define ROVER_CRYOBOT_CMD_MID    0x1C40U   /* 0x1800 | 0x440 */

/* ── Sim injection block anchor (sideband only; never flight-path) ──────────── */
#define SIM_INJECT_HK_MID        0x0D00U   /* 0x0800 | 0x500 */

#endif /* MIDS_H */
