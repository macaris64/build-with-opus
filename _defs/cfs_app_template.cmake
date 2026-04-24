# _defs/cfs_app_template.cmake — SAKURA-II cFS application scaffolding macro
#
# Usage (from apps/<app_name>/CMakeLists.txt):
#   sakura_add_cfs_app(<app_name>)
#
# Provides per app:
#   <app_name>              — OBJECT library (FSW source, no UNIT_TEST flag)
#   <app_name>_test         — CMocka executable (requires libcmocka-dev)
#   <app_name>_unit_tests   — CTest entry
#   <app_name>_cppcheck     — cppcheck custom target (requires cppcheck)
#
# Invariants upheld by this macro (do not duplicate in callers):
#   - add_compile_options(-Wall -Wextra -Werror -pedantic) is set at top-level
#   - include(CTest) in top-level already called enable_testing()
#   - CMAKE_CURRENT_SOURCE_DIR at call time resolves to apps/<app_name>/
#   - CMAKE_SOURCE_DIR always resolves to repo root

function(sakura_add_cfs_app app_name)

    # ── 1. FSW OBJECT Library ─────────────────────────────────────────────────
    # OBJECT library: .o files can be reused by both the mission link (future
    # cFS integration) and the unit-test executable without recompiling.
    add_library(${app_name} OBJECT
        fsw/src/${app_name}.c
    )
    target_include_directories(${app_name} PUBLIC
        fsw/src
        ${CMAKE_SOURCE_DIR}/_defs
    )

    # ── 2. CMocka Unit-Test Executable ────────────────────────────────────────
    # QUIET suppresses "not found" noise; an explicit WARNING is issued instead
    # so the message matches the rest of the build output tone.
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(CMOCKA QUIET cmocka)
    endif()

    if(CMOCKA_FOUND)
        # Re-compiles app source with UNIT_TEST defined so CFE_* calls resolve
        # to stubs in the test driver (see fsw/unit-test/<app_name>_test.c).
        add_executable(${app_name}_test
            fsw/unit-test/${app_name}_test.c
            fsw/src/${app_name}.c
        )
        target_include_directories(${app_name}_test PRIVATE
            fsw/src
            ${CMAKE_SOURCE_DIR}/_defs
            ${CMOCKA_INCLUDE_DIRS}
        )
        target_compile_definitions(${app_name}_test PRIVATE UNIT_TEST)
        target_link_libraries(${app_name}_test PRIVATE ${CMOCKA_LIBRARIES})
        target_compile_options(${app_name}_test PRIVATE ${CMOCKA_CFLAGS_OTHER})
        add_test(NAME ${app_name}_unit_tests COMMAND ${app_name}_test)
    else()
        message(WARNING
            "${app_name}: cmocka not found — unit tests will not be built. "
            "Install libcmocka-dev to enable."
        )
    endif()

    # ── 3. cppcheck Per-App Static Analysis Target ────────────────────────────
    # Scoped to fsw/src/<app_name>.c. The CI-wide check that exercises all of
    # apps/ is a separate job; this target is for rapid development iteration.
    # VERBATIM prevents CMake from mangling semicolons in the command list.
    find_program(CPPCHECK_EXECUTABLE cppcheck)
    if(CPPCHECK_EXECUTABLE)
        add_custom_target(${app_name}_cppcheck
            COMMAND ${CPPCHECK_EXECUTABLE}
                --enable=all
                --std=c17
                --error-exitcode=1
                --suppress=missingIncludeSystem
                -I ${CMAKE_CURRENT_SOURCE_DIR}/fsw/src
                -I ${CMAKE_SOURCE_DIR}/_defs
                ${CMAKE_CURRENT_SOURCE_DIR}/fsw/src/${app_name}.c
            COMMENT "cppcheck: static analysis on ${app_name}"
            VERBATIM
        )
    else()
        message(STATUS
            "${app_name}: cppcheck not found — ${app_name}_cppcheck target "
            "unavailable. Install cppcheck to enable."
        )
    endif()

endfunction()
