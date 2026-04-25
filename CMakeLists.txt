cmake_minimum_required(VERSION 3.20)
project(space_systems_boilerplate C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Strict flags applied to all targets. -Werror turns warnings into build failures.
add_compile_options(-Wall -Wextra -Werror -pedantic)

# CTest integration — run with: ctest --test-dir build --output-on-failure
include(CTest)

# Mission-level definitions (targets, spacecraft ID)
include(_defs/targets.cmake)

# ── Coverage gate option ───────────────────────────────────────────────────────
# Adds -fprofile-arcs/-ftest-coverage to _test executables only (never FSW).
# Default OFF for normal dev builds; scripts/coverage-gate.sh passes ON explicitly.
option(SAKURA_COVERAGE "Enable gcov branch-coverage instrumentation on unit-test executables" OFF)

# ── cFS Runtime option ─────────────────────────────────────────────────────────
# When ON, each cFS app is also compiled as a SHARED library loadable by cFE
# (core-cpu1). Requires cFE/OSAL/PSP git submodules initialized under cfs/.
# Run: git submodule update --init --recursive   (see cfs/README.md).
# CMocka unit-test path remains fully functional regardless of this option.
option(SAKURA_CFS_RUNTIME "Build each cFS app as a cFE-loadable shared library" OFF)

if(SAKURA_CFS_RUNTIME)
    # Verify submodules are present before attempting to include cFE CMake.
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/cfs/cFE/CMakeLists.txt")
        message(FATAL_ERROR
            "SAKURA_CFS_RUNTIME=ON but cfs/cFE submodule is not initialized.\n"
            "Run: git submodule update --init --recursive\n"
            "See cfs/README.md for details.")
    endif()
    # cFE's mission_build.cmake defaults MISSION_SOURCE_DIR to the cFE parent
    # directory (cfs/).  Override to repo root so it finds _defs/ (named _defs,
    # matching the *_defs glob) and the apps/ search path resolves correctly.
    # Symlinks cfe/ osal/ psp/ at repo root bridge the cfs/cFE cfs/osal cfs/PSP
    # layout to the paths cFE's default MISSION_MODULE_SEARCH_PATH expects.
    set(MISSION_SOURCE_DIR "${CMAKE_SOURCE_DIR}" CACHE PATH
        "Top level mission source directory (overrides cFE default)")
    # cFE's top-level cmake sets up add_cfe_app() and related helpers.
    add_subdirectory(cfs/cFE)
    message(STATUS "SAKURA_CFS_RUNTIME: ON — apps will be linked as cFE shared libraries")
else()
    message(STATUS "SAKURA_CFS_RUNTIME: OFF — CMocka unit-test path only (default)")
endif()

# ── cFS Applications ──────────────────────────────────────────────────────────
# Each app lives under apps/<name>/ and owns its own CMakeLists.txt.
# In a full cFS integration, the cFE cmake framework provides add_cfe_app().
add_subdirectory(apps/sample_app)
add_subdirectory(apps/orbiter_cdh)
add_subdirectory(apps/orbiter_adcs)
add_subdirectory(apps/orbiter_comm)
add_subdirectory(apps/orbiter_power)
add_subdirectory(apps/orbiter_payload)
add_subdirectory(apps/mcu_payload_gw)
add_subdirectory(apps/mcu_rwa_gw)
add_subdirectory(apps/mcu_eps_gw)
add_subdirectory(apps/sim_adapter)

# ── Simulation Plugins ────────────────────────────────────────────────────────
# Gazebo plugins require Gazebo to be installed. Each subdirectory guards
# against building when Gazebo headers are absent — all cFS unit-test targets
# remain unaffected if Gazebo is not installed.
add_subdirectory(simulation/gazebo_rover_plugin)
add_subdirectory(simulation/gazebo_uav_plugin)
add_subdirectory(simulation/gazebo_cryobot_plugin)
add_subdirectory(simulation/gazebo_world_plugin)
# fault_injector core library builds always (no Gazebo dep); only the
# scenario-runner integration with Gazebo requires Gazebo headers.
add_subdirectory(simulation/fault_injector)

# ── Note on ROS 2 workspace ───────────────────────────────────────────────────
# ros2_ws/ is built separately with colcon (not cmake --build).
# Run from ros2_ws/: colcon build --symlink-install
