cmake_minimum_required(VERSION 3.20)
project(gazebo_world_plugin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Hot-path unit tests (no Gazebo dependency) ────────────────────────────────
# world_environment_core.h is header-only; the test executable links nothing
# beyond the C++ runtime. Builds on any Linux host without Gazebo installed.
if(BUILD_TESTING)
    add_executable(world_plugin_test test/world_plugin_test.cpp)
    target_include_directories(world_plugin_test PRIVATE include)
    target_compile_options(world_plugin_test PRIVATE -Wall -Wextra -Werror)
    add_test(NAME world_plugin_unit_tests COMMAND world_plugin_test)
endif()

# ── Gazebo shared library ─────────────────────────────────────────────────────
find_package(gazebo QUIET)

if(NOT gazebo_FOUND)
    message(WARNING "Gazebo not found — world environment plugin will not be built.")
    return()
endif()

include_directories(
    include
    ${GAZEBO_INCLUDE_DIRS}
)
link_directories(${GAZEBO_LIBRARY_DIRS})
list(APPEND CMAKE_CXX_FLAGS "${GAZEBO_CXX_FLAGS}")

add_library(world_environment_plugin SHARED
    src/world_environment_plugin.cpp
)
target_include_directories(world_environment_plugin PRIVATE include)
target_compile_options(world_environment_plugin PRIVATE -Wall -Wextra -Werror)
target_link_libraries(world_environment_plugin ${GAZEBO_LIBRARIES})

install(TARGETS world_environment_plugin DESTINATION lib)
