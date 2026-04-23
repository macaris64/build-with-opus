#ifndef MISSION_CONFIG_H
#define MISSION_CONFIG_H

/* All values are compile-time constants — no runtime tuning.
 * This satisfies DO-178C §6.3.3 (deterministic resource allocation)
 * and MISRA C:2012 Rule 1.3 (no undefined behavior from dynamic sizing). */

#define SAKURA_II_MAX_PIPES   8U    /* Maximum software bus pipes per app */
#define SAKURA_II_TASK_STACK  8192U /* Default task stack depth in bytes */

/* Spacecraft ID (SCID). Fleet allocation anchors on SAKURA_II_SCID_BASE;
 * instance N derives SPACECRAFT_ID = SAKURA_II_SCID_BASE + N. The single-
 * instance repo ships with the anchor value itself. See
 * docs/interfaces/apid-registry.md §Identifiers and Ranges and SYS-REQ-0026. */
#define SAKURA_II_SCID_BASE   42U
#define SPACECRAFT_ID         42U   /* CCSDS APID spacecraft identifier */

/* Canonical mission name. Must agree with _defs/targets.cmake (enforced by
 * rust/cfs_bindings::tests::test_mission_name_is_sakura_ii). */
#define MISSION_NAME "SAKURA_II"

#endif /* MISSION_CONFIG_H */
