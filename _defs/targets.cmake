# targets.cmake — Mission-level build configuration
#
# Defines the canonical mission name, spacecraft ID, and the list of cFS
# applications enabled for this build. Included by the top-level CMakeLists.txt.

set(MISSION_NAME "SAKURA_II")
set(SPACECRAFT_ID 42)

# List of cFS apps included in this mission build.
# Add new app directory names here as they are created under apps/.
set(MISSION_APPS
    sample_app
)

message(STATUS "Mission: ${MISSION_NAME}  SCID: ${SPACECRAFT_ID}  Apps: ${MISSION_APPS}")
