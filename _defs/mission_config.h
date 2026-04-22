#ifndef MISSION_CONFIG_H
#define MISSION_CONFIG_H

/* All values are compile-time constants — no runtime tuning.
 * This satisfies DO-178C §6.3.3 (deterministic resource allocation)
 * and MISRA C:2012 Rule 1.3 (no undefined behavior from dynamic sizing). */

#define SAMPLE_MISSION_MAX_PIPES   8U    /* Maximum software bus pipes per app */
#define SAMPLE_MISSION_TASK_STACK  8192U /* Default task stack depth in bytes */
#define SPACECRAFT_ID              42U   /* CCSDS APID spacecraft identifier */

#endif /* MISSION_CONFIG_H */
