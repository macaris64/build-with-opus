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

# SAKURA_COVERAGE is declared in the root CMakeLists.txt (default OFF).
# This macro reads it; it does not own the declaration.

function(sakura_add_cfs_app app_name)

    # ── cFS Runtime mode (SAKURA_CFS_RUNTIME=ON) ─────────────────────────────
    # When cFE is available (submodule initialized), also build each app as a
    # SHARED library that cFE loads via dlopen(). The CMocka OBJECT+test path
    # below is built regardless — both modes coexist.
    if(SAKURA_CFS_RUNTIME)
        if(COMMAND add_cfe_app)
            # Real cFE cmake framework is present (submodule initialized).
            add_cfe_app(${app_name}
                SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/fsw/src/${app_name}.c
            )
            target_include_directories(${app_name} PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/fsw/src>
                $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/_defs>
            )
        else()
            # Fallback: build a plain SHARED library with cFE naming convention.
            add_library(${app_name}_runtime SHARED
                ${CMAKE_CURRENT_SOURCE_DIR}/fsw/src/${app_name}.c
            )
            set_target_properties(${app_name}_runtime PROPERTIES
                OUTPUT_NAME "${app_name}"
                PREFIX "lib"
            )
            target_include_directories(${app_name}_runtime PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/fsw/src>
                $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/_defs>
            )
            install(TARGETS ${app_name}_runtime
                LIBRARY DESTINATION "${CMAKE_BINARY_DIR}/cf"
            )
            message(STATUS
                "${app_name}: SAKURA_CFS_RUNTIME=ON — "
                "built as cFE-loadable shared library (lib${app_name}.so)")
        endif()
    endif()

    # ── 0. Auto-generate test-file boilerplate if absent ─────────────────────
    # Scaffold contract: configure_file writes into the SOURCE TREE so that
    # add_executable() below can reference the file by its source path, and so
    # that the developer can immediately commit and hand-edit it. This is an
    # intentional one-shot write — not a build-time regeneration. The
    # if(NOT EXISTS) guard is load-bearing: it prevents overwriting hand-edited
    # test files for apps that have already been onboarded. @ONLY prevents
    # CMake from interpreting C ${...} syntax inside the template.
    #
    # _APP_NAME_UPPER is function-scoped (prefixed _ by convention) to prevent
    # namespace pollution if configure_file is called again in the same scope.
    set(_test_file "${CMAKE_CURRENT_SOURCE_DIR}/fsw/unit-test/${app_name}_test.c")
    if(NOT EXISTS "${_test_file}")
        string(TOUPPER "${app_name}" _app_name_upper)
        set(_APP_NAME_UPPER "${_app_name_upper}")
        configure_file(
            "${CMAKE_SOURCE_DIR}/_defs/unit_test_template.c.in"
            "${_test_file}"
            @ONLY
        )
        message(STATUS "${app_name}: generated unit-test boilerplate → ${_test_file}")
        unset(_APP_NAME_UPPER)
    endif()

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
        target_link_directories(${app_name}_test PRIVATE ${CMOCKA_LIBRARY_DIRS})
        target_compile_options(${app_name}_test PRIVATE ${CMOCKA_CFLAGS_OTHER})

        # Coverage flags land here only — never on the FSW OBJECT library above.
        # SAKURA_COVERAGE is declared OFF in the root CMakeLists.txt; enable
        # explicitly with -DSAKURA_COVERAGE=ON (scripts/coverage-gate.sh does this).
        # --coverage at link time is accepted by both GCC and Clang; -lgcov is
        # GCC-only and would break Clang-based sanitizer runs.
        if(SAKURA_COVERAGE)
            target_compile_options(${app_name}_test PRIVATE
                -fprofile-arcs
                -ftest-coverage
            )
            target_link_options(${app_name}_test PRIVATE --coverage)
        endif()

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
                --std=c17
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
