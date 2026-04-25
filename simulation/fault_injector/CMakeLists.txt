cmake_minimum_required(VERSION 3.20)
project(fault_injector CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Core library (no Gazebo dependency) ──────────────────────────────────────
# spp_encoder and FaultInjector are pure C++17 + POSIX; they build on any
# Linux host, enabling CI to run unit tests without Gazebo installed.
add_library(fault_injector_lib STATIC
    src/spp_encoder.cpp
    src/fault_injector.cpp
)
target_include_directories(fault_injector_lib PUBLIC include)
target_compile_options(fault_injector_lib PRIVATE -Wall -Wextra -Werror)

# ── Unit test executable ──────────────────────────────────────────────────────
add_executable(fault_injector_test
    test/fault_injector_test.cpp
)
target_include_directories(fault_injector_test PRIVATE include)
target_compile_options(fault_injector_test PRIVATE -Wall -Wextra -Werror)
target_link_libraries(fault_injector_test PRIVATE fault_injector_lib)

# Pass source-tree scenarios directory so canonical-file tests can locate it.
target_compile_definitions(fault_injector_test PRIVATE
    SCENARIOS_DIR="${CMAKE_SOURCE_DIR}/simulation/scenarios")

# CTest integration — same pattern as cFS app unit-test executables.
if(BUILD_TESTING)
    add_test(NAME fault_injector_unit_tests COMMAND fault_injector_test)
endif()
