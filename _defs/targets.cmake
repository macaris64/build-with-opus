# targets.cmake — Mission-level build configuration
#
# Defines the canonical mission name, spacecraft ID, and the list of cFS
# applications enabled for this build. Included by the top-level CMakeLists.txt.
# Must be included before any add_subdirectory(apps/*) calls so that
# sakura_add_cfs_app() is defined when each app's CMakeLists.txt is processed.

# CMAKE_CURRENT_LIST_DIR resolves to _defs/ regardless of include chain depth.
include("${CMAKE_CURRENT_LIST_DIR}/cfs_app_template.cmake")

set(MISSION_NAME "SAKURA_II")
set(SPACECRAFT_ID 42)

# MISSION_APPS — authoritative list of cFS app directory names.
# To onboard a new cFS app:
#   1. Create apps/<new_app>/ following the layout in apps/sample_app/
#   2. Add <new_app> to this list
#   3. Add add_subdirectory(apps/<new_app>) to the top-level CMakeLists.txt
#   4. Register MIDs in _defs/mids.h and docs/interfaces/apid-registry.md
set(MISSION_APPS
    sample_app
    orbiter_cdh
    orbiter_adcs
    orbiter_comm
    orbiter_power
    orbiter_payload
    mcu_payload_gw
    mcu_rwa_gw
    mcu_eps_gw
    sim_adapter
)

message(STATUS "Mission: ${MISSION_NAME}  SCID: ${SPACECRAFT_ID}  Apps: ${MISSION_APPS}")

# ── cFE CPU Target Configuration ──────────────────────────────────────────────
# Required by cFE's mission_build.cmake / read_targetconfig() when
# SAKURA_CFS_RUNTIME=ON.  Defines one native Linux CPU (cpu1).
#
# cpu1_SYSTEM "native" → cFE builds for the host without a cross toolchain.
# cpu1_PSP_MODULELIST "pc-linux" → links the Linux PSP module into core-cpu1.
# cpu1_FILELIST → cfe_es_startup.scr is searched in MISSION_DEFS (_defs/).
#   cFE looks for _defs/cpu1_cfe_es_startup.scr (cpu-prefixed) first, then
#   _defs/cfe_es_startup.scr.  We supply the prefixed name so the same file
#   can vary per CPU in multi-CPU missions.
#
# MISSION_MODULE_SEARCH_PATH default includes "apps/" (relative to MISSION_SOURCE_DIR
# = repo root) so all apps/*/  directories are found automatically.
# osal and psp are found via repo-root symlinks (cfe/ osal/ psp/) created by
#   git checkout; these bridge to cfs/cFE, cfs/osal, cfs/PSP respectively.
SET(MISSION_CPUNAMES cpu1)
SET(cpu1_PROCESSORID 1)
SET(cpu1_APPLIST
    sample_app
    orbiter_cdh
    orbiter_adcs
    orbiter_comm
    orbiter_power
    orbiter_payload
    mcu_payload_gw
    mcu_rwa_gw
    mcu_eps_gw
    sim_adapter
)
SET(cpu1_FILELIST cfe_es_startup.scr)
SET(cpu1_SYSTEM native)
