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

# ── Simulation Plugins ────────────────────────────────────────────────────────
# Gazebo plugins require Gazebo Harmonic to be installed. The subdirectory
# guards against building when Gazebo is absent.
add_subdirectory(simulation/gazebo_rover_plugin)

# ── Note on ROS 2 workspace ───────────────────────────────────────────────────
# ros2_ws/ is built separately with colcon (not cmake --build).
# Run from ros2_ws/: colcon build --symlink-install
