/* cryobot_plugin_test.cpp — OnUpdate hot-path unit tests for CrybotPhysicsPlugin.
 *
 * Tests are self-contained: no Gazebo dependency, no external framework.
 * Returns 0 on all-pass, 1 on any failure. Registered with CTest.
 *
 * Coverage target: 100% branch on cryobot_physics_core.h hot-path logic,
 * including the tether-extension branch (depth above and below rest length).
 *
 * Allocation sentinel: malloc() is overridden to set g_malloc_called.
 * Any heap allocation inside compute_step() would flip the sentinel and
 * fail the assertion, proving the OnUpdate hot path is alloc-free.
 */

#include "cryobot_physics_core.h"

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

static void test_compute_step_no_alloc_shallow()
{
    /* Given depth < rest_length (tether slack),
     * When compute_step() executes,
     * Then no heap allocation occurs. */
    g_malloc_called = false;
    const auto s = gazebo_cryobot::compute_step(5.0, -0.1);
    CHECK(!g_malloc_called);
    (void)s;
}

static void test_compute_step_no_alloc_deep()
{
    /* Given depth > rest_length (tether taut),
     * When compute_step() executes,
     * Then no heap allocation occurs. */
    g_malloc_called = false;
    const auto s = gazebo_cryobot::compute_step(15.0, -0.2);
    CHECK(!g_malloc_called);
    (void)s;
}

/* ── Tether physics correctness tests ─────────────────────────────────────── */

static void test_compute_step_no_tension_when_shallow()
{
    /* Given depth = 5 m (< rest_length = 10 m),
     * When compute_step() executes,
     * Then tether_tension_n == 0 (slack). */
    const auto s = gazebo_cryobot::compute_step(5.0, 0.0);
    CHECK(s.tether_tension_n == 0.0);
}

static void test_compute_step_tension_at_rest_length_boundary()
{
    /* Given depth == TETHER_REST_LEN_M exactly,
     * When compute_step() executes,
     * Then tether_tension_n == 0 (boundary: extension = 0). */
    const auto s = gazebo_cryobot::compute_step(
        gazebo_cryobot::TETHER_REST_LEN_M, 0.0);
    CHECK(s.tether_tension_n == 0.0);
}

static void test_compute_step_tension_when_extended()
{
    /* Given depth = 12 m (extension = 2 m, stiffness = 500 N/m),
     * When compute_step() executes,
     * Then tether_tension_n == 1000 N. */
    const auto s = gazebo_cryobot::compute_step(12.0, 0.0);
    CHECK(s.tether_tension_n == 1000.0);
}

static void test_compute_step_net_force_descent()
{
    /* Given depth = 5 m (slack), descent_rate = 1.0 m/s,
     * When compute_step() executes,
     * Then net_force_z == 1.0 * 10.0 - 0.0 == 10.0 N. */
    const auto s = gazebo_cryobot::compute_step(5.0, 1.0);
    CHECK(s.net_force_z == 10.0);
}

static void test_compute_step_net_force_with_tension()
{
    /* Given depth = 11 m (extension = 1 m, tension = 500 N),
     * descent_rate = 0 m/s,
     * When compute_step() executes,
     * Then net_force_z == 0.0 - 500.0 == -500.0 N (restoring). */
    const auto s = gazebo_cryobot::compute_step(11.0, 0.0);
    CHECK(s.tether_tension_n == 500.0);
    CHECK(s.net_force_z      == -500.0);
}

/* ── Entry point ───────────────────────────────────────────────────────────── */

int main()
{
    test_compute_step_no_alloc_shallow();
    test_compute_step_no_alloc_deep();
    test_compute_step_no_tension_when_shallow();
    test_compute_step_tension_at_rest_length_boundary();
    test_compute_step_tension_when_extended();
    test_compute_step_net_force_descent();
    test_compute_step_net_force_with_tension();

    std::fprintf(stdout, "cryobot_plugin_test: %d passed, %d failed\n",
                 g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
