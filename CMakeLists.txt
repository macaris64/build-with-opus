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
