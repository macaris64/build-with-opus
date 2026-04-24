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
#     because add_subdirectory() sets it before the callee CMakeLists.txt runs
#   - CMAKE_SOURCE_DIR always resolves to repo root regardless of call depth
#
# Extension point: ARGN is forwarded to cmake_parse_arguments for future
# keywords (EXTRA_SOURCES, EXTRA_INCLUDES) added by Phase 19+. Do not
# pass positional arguments beyond app_name until keywords are defined.

function(sakura_add_cfs_app app_name)

    # ── 1. FSW OBJECT Library ─────────────────────────────────────────────────
    # OBJECT library: .o files can be reused by both the mission link (future
    # cFS integration) and the unit-test executable without recompiling.
    # Absolute paths guard against callers outside apps/<name>/ scope.
    add_library(${app_name} OBJECT
        ${CMAKE_CURRENT_SOURCE_DIR}/fsw/src/${app_name}.c
    )
    target_include_directories(${app_name} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/fsw/src>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/_defs>
    )

    # ── 2. CMocka Unit-Test Executable ────────────────────────────────────────
    # QUIET suppresses pkg-config "not found" noise; an explicit WARNING is
    # issued instead so the signal is visible but not treated as a build error.
    # find_program result is cached by CMake after the first call, so repeated
    # invocations for additional apps incur no re-scan overhead.
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(CMOCKA QUIET cmocka)
    endif()

    if(CMOCKA_FOUND)
        # Re-compiles app source with UNIT_TEST defined so CFE_* calls resolve
        # to stubs in the test driver (see fsw/unit-test/<app_name>_test.c).
        # Coverage flags (Phase 19) should land here only, not on the OBJECT
        # library, so FSW binaries are never instrumented.
        add_executable(${app_name}_test
            ${CMAKE_CURRENT_SOURCE_DIR}/fsw/unit-test/${app_name}_test.c
            ${CMAKE_CURRENT_SOURCE_DIR}/fsw/src/${app_name}.c
        )
        target_include_directories(${app_name}_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/fsw/src
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
    # Scoped to fsw/src/<app_name>.c. The CI-wide check (cppcheck apps/) is a
    # separate job; this target is for rapid development iteration.
    # VERBATIM prevents CMake from mangling semicolons in the COMMAND list.
    #
    # --inline-suppr: honours MISRA deviation comments of the form
    #   /* MISRA C:2012 Rule X.Y deviation: <reason> */  (per .claude/rules/general.md)
    #   and any cppcheck-suppress annotations in app source.
    # --suppress=missingInclude: cfe.h and OSAL headers are build-system stubs
    #   not resolvable by cppcheck; suppressing avoids false positives from
    #   the standalone-analysis context (real cFS headers absent).
    # SAKURA_CPPCHECK_EXECUTABLE: namespaced to avoid collision with any future
    #   cmake module that also calls find_program(CPPCHECK_EXECUTABLE ...).
    find_program(SAKURA_CPPCHECK_EXECUTABLE cppcheck)
    if(SAKURA_CPPCHECK_EXECUTABLE)
        add_custom_target(${app_name}_cppcheck
            COMMAND ${SAKURA_CPPCHECK_EXECUTABLE}
                --enable=all
                --std=c11
                --error-exitcode=1
                --inline-suppr
                --suppress=missingIncludeSystem
                --suppress=missingInclude
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
