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
#define ORBITER_ADCS_HK_MID         0x0910U   /* 0x0800 | 0x110 — attitude quaternion HK */
#define ORBITER_ADCS_WHEEL_TLM_MID  0x0911U   /* 0x0800 | 0x111 — wheel telemetry (stub; mcu_rwa_gw Phase 35) */
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

/* ── sim_adapter MIDs (SITL only; CFS_FLIGHT_BUILD excluded) ────────────────── */
#define SIM_ADAPTER_CMD_MID      0x1D00U   /* 0x1800 | 0x500 */
#define SIM_ADAPTER_HK_MID       0x0D01U   /* 0x0800 | 0x501 */

/* ── ros2_bridge MIDs (APID 0x128 in orbiter TM block — free slot) ──────────── */
#define ROS2_BRIDGE_HK_MID       0x0928U   /* 0x0800 | 0x128 */
#define ROS2_BRIDGE_CMD_MID      0x1928U   /* 0x1800 | 0x128 */

/* ── Fault-injection SPP APIDs (sideband only; ICD-sim-fsw.md §2) ───────────
 * These are raw APID values (not cFE MIDs) used by simulation/fault_injector.
 * They are never placed on the cFE Software Bus (CFS_FLIGHT_BUILD guard
 * required in sim_adapter; see ICD-sim-fsw.md §5.2). */
#define SIM_FAULT_APID_BASE      0x0540U   /* first fault-injection APID */
#define SIM_FAULT_APID_PKT_DROP  0x0540U   /* PKT-SIM-0540-0001 packet drop */
#define SIM_FAULT_APID_CLK_SKEW  0x0541U   /* PKT-SIM-0541-0001 clock skew */
#define SIM_FAULT_APID_SAFE_MODE 0x0542U   /* PKT-SIM-0542-0001 force safe-mode */
#define SIM_FAULT_APID_SNS_NOISE 0x0543U   /* PKT-SIM-0543-0001 sensor-noise corruption */
#define SIM_FAULT_APID_LAST      0x0543U   /* last reserved fault-injection APID */

#endif /* MIDS_H */
