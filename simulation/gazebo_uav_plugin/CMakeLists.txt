cmake_minimum_required(VERSION 3.20)
project(gazebo_uav_plugin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Hot-path unit tests (no Gazebo dependency) ────────────────────────────────
# uav_flight_core.h is header-only; the test executable links nothing beyond
# the C++ runtime. Builds on any Linux host without Gazebo installed.
if(BUILD_TESTING)
    add_executable(uav_plugin_test test/uav_plugin_test.cpp)
    target_include_directories(uav_plugin_test PRIVATE include)
    target_compile_options(uav_plugin_test PRIVATE -Wall -Wextra -Werror)
    add_test(NAME uav_plugin_unit_tests COMMAND uav_plugin_test)
endif()

# ── Gazebo shared library ─────────────────────────────────────────────────────
find_package(gz-sim8 QUIET)
find_package(gz-transport13 QUIET)
find_package(gz-plugin2 QUIET)

if(NOT gz-sim8_FOUND)
    message(WARNING "gz-sim8 not found — UAV flight plugin will not be built.")
    return()
endif()

add_library(uav_flight_plugin SHARED
    src/uav_flight_plugin.cpp
)
target_include_directories(uav_flight_plugin PRIVATE include)
target_compile_options(uav_flight_plugin PRIVATE -Wall -Wextra -Werror)
target_link_libraries(uav_flight_plugin
    gz-sim8::gz-sim8
    gz-transport13::gz-transport13
    gz-plugin2::gz-plugin2
)

install(TARGETS uav_flight_plugin DESTINATION lib)
