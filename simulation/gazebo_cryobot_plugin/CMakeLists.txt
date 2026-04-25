cmake_minimum_required(VERSION 3.20)
project(gazebo_cryobot_plugin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Hot-path unit tests (no Gazebo dependency) ────────────────────────────────
# cryobot_physics_core.h is header-only; the test executable links nothing
# beyond the C++ runtime. Builds on any Linux host without Gazebo installed.
if(BUILD_TESTING)
    add_executable(cryobot_plugin_test test/cryobot_plugin_test.cpp)
    target_include_directories(cryobot_plugin_test PRIVATE include)
    target_compile_options(cryobot_plugin_test PRIVATE -Wall -Wextra -Werror)
    add_test(NAME cryobot_plugin_unit_tests COMMAND cryobot_plugin_test)
endif()

# ── Gazebo shared library ─────────────────────────────────────────────────────
find_package(gazebo QUIET)

if(NOT gazebo_FOUND)
    message(WARNING "Gazebo not found — cryobot physics plugin will not be built.")
    return()
endif()

include_directories(
    include
    ${GAZEBO_INCLUDE_DIRS}
)
link_directories(${GAZEBO_LIBRARY_DIRS})
list(APPEND CMAKE_CXX_FLAGS "${GAZEBO_CXX_FLAGS}")

add_library(cryobot_physics_plugin SHARED
    src/cryobot_physics_plugin.cpp
)
target_include_directories(cryobot_physics_plugin PRIVATE include)
target_compile_options(cryobot_physics_plugin PRIVATE -Wall -Wextra -Werror)
target_link_libraries(cryobot_physics_plugin ${GAZEBO_LIBRARIES})

install(TARGETS cryobot_physics_plugin DESTINATION lib)
