cmake_minimum_required(VERSION 3.20)
project(sample_app C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror -pedantic)

# ── Flight Software Target ────────────────────────────────────────────────────
# In a real cFS integration this uses the add_cfe_app() CMake function
# provided by the cFE framework. Here we define the library directly so
# the project builds without a full cFS installation.
add_library(sample_app OBJECT
    fsw/src/sample_app.c
)
target_include_directories(sample_app PUBLIC
    fsw/src
    ${CMAKE_SOURCE_DIR}/_defs
)

# ── Unit Test Target ──────────────────────────────────────────────────────────
# Compiles sample_app.c again with UNIT_TEST defined so that CFE_* calls
# resolve to the stubs declared in sample_app_test.c rather than the real
# cFE library. No actual cFE headers are required on the build machine.
enable_testing()

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(CMOCKA cmocka)
endif()

if(CMOCKA_FOUND)
    add_executable(sample_app_test
        fsw/unit-test/sample_app_test.c
        fsw/src/sample_app.c
    )
    target_include_directories(sample_app_test PRIVATE
        fsw/src
        ${CMAKE_SOURCE_DIR}/_defs
    )
    target_compile_definitions(sample_app_test PRIVATE UNIT_TEST)
    target_link_libraries(sample_app_test PRIVATE ${CMOCKA_LIBRARIES})
    target_include_directories(sample_app_test PRIVATE ${CMOCKA_INCLUDE_DIRS})
    target_compile_options(sample_app_test PRIVATE ${CMOCKA_CFLAGS_OTHER})

    add_test(NAME sample_app_unit_tests COMMAND sample_app_test)
else()
    message(WARNING "cmocka not found — unit tests will not be built. Install libcmocka-dev to enable.")
endif()
