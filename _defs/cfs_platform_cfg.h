#ifndef CFS_PLATFORM_CFG_H
#define CFS_PLATFORM_CFG_H

/*
 * cfs_platform_cfg.h — SAKURA-II cpu1 platform-level configuration overrides.
 *
 * These values are passed to cFE at build time when SAKURA_CFS_RUNTIME=ON.
 * They override cFE defaults from cfs/cFE/modules/core_api/fsw/inc/cfe_platform_cfg.h.
 *
 * Change-control: any modification to this file that affects memory sizing or
 * task counts requires a review against docs/architecture/01-orbiter-cfs.md §4
 * (task table) and SYS-REQ-0060 (memory budget).
 */

/* Maximum number of cFE Applications (including core apps). */
#define CFE_PLATFORM_ES_MAX_APPLICATIONS    20U

/* Maximum number of Software Bus message IDs. */
#define CFE_PLATFORM_SB_MAX_MSG_IDS         64U

/* Maximum number of Software Bus pipes across all apps. */
#define CFE_PLATFORM_SB_MAX_PIPES           48U

/* Maximum depth of any single Software Bus pipe. */
#define CFE_PLATFORM_SB_MAX_PIPE_DEPTH      256U

/* EVS log capacity. */
#define CFE_PLATFORM_EVS_LOG_MAX            20U

/* Maximum number of registered EVS applications. */
#define CFE_PLATFORM_EVS_MAX_APP_EVENT_BURST 32U

/* cFE ES startup timeout [milliseconds]. */
#define CFE_PLATFORM_ES_STARTUP_SCRIPT_TIMEOUT_MSEC  30000U

/* RAM disk path (where /cf/ shared libraries are loaded from). */
#define CFE_PLATFORM_ES_RAM_DISK_MOUNT_STRING   "/cf"

/* Spacecraft ID — must match SPACECRAFT_ID in _defs/targets.cmake and mission_config.h. */
#define CFE_MISSION_SPACECRAFT_ID               42U

#endif /* CFS_PLATFORM_CFG_H */
