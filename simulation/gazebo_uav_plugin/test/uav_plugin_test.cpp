/* uav_plugin_test.cpp — OnUpdate hot-path unit tests for UavFlightPlugin.
 *
 * Tests are self-contained: no Gazebo dependency, no external framework.
 * Returns 0 on all-pass, 1 on any failure. Registered with CTest.
 *
 * Coverage target: 100% branch on uav_flight_core.h hot-path logic.
 *
 * Allocation sentinel: malloc() is overridden to set g_malloc_called.
 * Any heap allocation inside compute_force_cmd() would flip the sentinel
 * and fail the assertion, proving the OnUpdate hot path is alloc-free.
 */

#include "uav_flight_core.h"

#include <cstdio>
#include <cstdlib>

/* ── Allocation sentinel ───────────────────────────────────────────────────── */

static bool g_malloc_called = false;

/* Override malloc so any heap allocation in the hot path flips the sentinel.
 * SAFETY: __libc_malloc is the underlying glibc allocator; calling it after
 * setting the sentinel prevents infinite recursion and memory corruption.
 * This override is scoped to the test binary only — it does not affect the
 * plugin shared library at runtime. */
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

static void test_compute_force_cmd_no_alloc_hover()
{
    /* Given thrust = 9.81 N (hover) and yaw_rate = 0,
     * When compute_force_cmd() executes,
     * Then no heap allocation occurs. */
    g_malloc_called = false;
    const auto cmd = gazebo_uav::compute_force_cmd(9.81, 0.0);
    CHECK(!g_malloc_called);
    (void)cmd;
}

static void test_compute_force_cmd_no_alloc_with_yaw()
{
    /* Given non-zero yaw rate, When compute_force_cmd() executes,
     * Then no heap allocation occurs. */
    g_malloc_called = false;
    const auto cmd = gazebo_uav::compute_force_cmd(5.0, 1.57);
    CHECK(!g_malloc_called);
    (void)cmd;
}

static void test_compute_force_cmd_no_alloc_zero()
{
    /* Given all-zero commands, When compute_force_cmd() executes,
     * Then no heap allocation occurs. */
    g_malloc_called = false;
    const auto cmd = gazebo_uav::compute_force_cmd(0.0, 0.0);
    CHECK(!g_malloc_called);
    (void)cmd;
}

/* ── Value-correctness tests ───────────────────────────────────────────────── */

static void test_compute_force_cmd_hover_values()
{
    /* Given thrust = 9.81 N and yaw_rate = 0,
     * When compute_force_cmd() executes,
     * Then force_z == 9.81 and torque_z == 0. */
    const auto cmd = gazebo_uav::compute_force_cmd(9.81, 0.0);
    CHECK(cmd.force_z  == 9.81);
    CHECK(cmd.torque_z == 0.0);
}

static void test_compute_force_cmd_full_command()
{
    /* Given thrust = 20.0 N and yaw_rate = -0.5 rad/s,
     * When compute_force_cmd() executes,
     * Then force_z and torque_z match the commanded values. */
    const auto cmd = gazebo_uav::compute_force_cmd(20.0, -0.5);
    CHECK(cmd.force_z  == 20.0);
    CHECK(cmd.torque_z == -0.5);
}

static void test_compute_force_cmd_negative_thrust()
{
    /* Negative thrust (braking) must pass through unchanged.
     * Given thrust = -3.0 N, When compute_force_cmd() executes,
     * Then force_z == -3.0. */
    const auto cmd = gazebo_uav::compute_force_cmd(-3.0, 0.0);
    CHECK(cmd.force_z == -3.0);
}

/* ── Entry point ───────────────────────────────────────────────────────────── */

int main()
{
    test_compute_force_cmd_no_alloc_hover();
    test_compute_force_cmd_no_alloc_with_yaw();
    test_compute_force_cmd_no_alloc_zero();
    test_compute_force_cmd_hover_values();
    test_compute_force_cmd_full_command();
    test_compute_force_cmd_negative_thrust();

    std::fprintf(stdout, "uav_plugin_test: %d passed, %d failed\n",
                 g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
