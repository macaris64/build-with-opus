/* world_plugin_test.cpp — OnUpdate hot-path unit tests for WorldEnvironmentPlugin.
 *
 * Tests are self-contained: no Gazebo dependency, no external framework.
 * Returns 0 on all-pass, 1 on any failure. Registered with CTest.
 *
 * Coverage target: 100% branch on world_environment_core.h hot-path logic,
 * including both the heartbeat-boundary branch and the common (skip) branch.
 *
 * Allocation sentinel: malloc() is overridden to set g_malloc_called.
 * Any heap allocation inside should_emit_heartbeat() would flip the sentinel
 * and fail the assertion, proving the OnUpdate hot path is alloc-free.
 */

#include "world_environment_core.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

/* ── Allocation sentinel ───────────────────────────────────────────────────── */

static bool g_malloc_called = false;

/* Override malloc so any heap allocation in the hot path flips the sentinel.
 * SAFETY: __libc_malloc is the underlying glibc allocator; calling it after
 * setting the sentinel prevents infinite recursion and memory corruption.
 * This override is scoped to the test binary only. */
extern "C" void *malloc(size_t sz)
{
    g_malloc_called = true;
    extern void *__libc_malloc(size_t);
    return __libc_malloc(sz);
}

/* ── Minimal test harness ──────────────────────────────────────────────────── */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if ((cond)) {                                                          \
            ++g_pass;                                                          \
        } else {                                                               \
            ++g_fail;                                                          \
            std::fprintf(stderr, "FAIL %s:%d  " #cond "\n",                   \
                         __FILE__, __LINE__);                                  \
        }                                                                      \
    } while (0)

/* ── Allocation-sentinel tests ─────────────────────────────────────────────── */

static void test_should_emit_heartbeat_no_alloc_common_path()
{
    /* Given tick_count = 1 (common path, no log boundary),
     * When should_emit_heartbeat() executes,
     * Then no heap allocation occurs. */
    g_malloc_called = false;
    const bool emit = gazebo_world::should_emit_heartbeat(1U);
    CHECK(!g_malloc_called);
    CHECK(!emit);
}

static void test_should_emit_heartbeat_no_alloc_boundary()
{
    /* Given tick_count = LOG_INTERVAL_TICKS (log boundary),
     * When should_emit_heartbeat() executes,
     * Then no heap allocation occurs. */
    g_malloc_called = false;
    const bool emit = gazebo_world::should_emit_heartbeat(
        gazebo_world::LOG_INTERVAL_TICKS);
    CHECK(!g_malloc_called);
    CHECK(emit);
}

/* ── Heartbeat-boundary correctness tests ─────────────────────────────────── */

static void test_no_heartbeat_at_tick_1()
{
    /* Given tick_count = 1,
     * When should_emit_heartbeat() executes,
     * Then it returns false (not a log boundary). */
    CHECK(!gazebo_world::should_emit_heartbeat(1U));
}

static void test_no_heartbeat_at_tick_999()
{
    /* Given tick_count = LOG_INTERVAL_TICKS - 1,
     * When should_emit_heartbeat() executes,
     * Then it returns false (just before boundary). */
    CHECK(!gazebo_world::should_emit_heartbeat(
        gazebo_world::LOG_INTERVAL_TICKS - 1U));
}

static void test_heartbeat_at_interval_boundary()
{
    /* Given tick_count == LOG_INTERVAL_TICKS,
     * When should_emit_heartbeat() executes,
     * Then it returns true (exactly on boundary). */
    CHECK(gazebo_world::should_emit_heartbeat(
        gazebo_world::LOG_INTERVAL_TICKS));
}

static void test_heartbeat_at_double_interval()
{
    /* Given tick_count == 2 * LOG_INTERVAL_TICKS,
     * When should_emit_heartbeat() executes,
     * Then it returns true (second boundary). */
    CHECK(gazebo_world::should_emit_heartbeat(
        2U * gazebo_world::LOG_INTERVAL_TICKS));
}

static void test_no_heartbeat_between_boundaries()
{
    /* Given tick_count = LOG_INTERVAL_TICKS + 1,
     * When should_emit_heartbeat() executes,
     * Then it returns false (just after boundary). */
    CHECK(!gazebo_world::should_emit_heartbeat(
        gazebo_world::LOG_INTERVAL_TICKS + 1U));
}

static void test_log_interval_constant_value()
{
    /* LOG_INTERVAL_TICKS must equal 1000 to match physics-step rate
     * (1000 Hz => 1 log line/second) per 05-simulation-gazebo.md §3. */
    CHECK(gazebo_world::LOG_INTERVAL_TICKS == 1000U);
}

/* ── Entry point ───────────────────────────────────────────────────────────── */

int main()
{
    test_should_emit_heartbeat_no_alloc_common_path();
    test_should_emit_heartbeat_no_alloc_boundary();
    test_no_heartbeat_at_tick_1();
    test_no_heartbeat_at_tick_999();
    test_heartbeat_at_interval_boundary();
    test_heartbeat_at_double_interval();
    test_no_heartbeat_between_boundaries();
    test_log_interval_constant_value();

    std::fprintf(stdout, "world_plugin_test: %d passed, %d failed\n",
                 g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
