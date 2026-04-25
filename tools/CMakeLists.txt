# tools/CMakeLists.txt — SAKURA-II cFS table-tool stub
#
# cFE's mission_build.cmake requires that MISSION_SOURCE_DIR/tools/ defines
# CFS_TABLETOOL_SCRIPT_DIR pointing to a directory that contains
# add_cfe_tables_impl.cmake.  None of the SAKURA-II apps use cFE tables, so
# this stub satisfies the cmake requirement without building anything.

set(CFS_TABLETOOL_SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake" PARENT_SCOPE)
