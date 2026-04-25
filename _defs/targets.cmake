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
