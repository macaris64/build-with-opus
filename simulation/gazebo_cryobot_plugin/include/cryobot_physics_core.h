#pragma once

// Pure cryobot tether-physics hot-path — no Gazebo types.
// Called from CrybotPhysicsPlugin::OnUpdate(); tested without a Gazebo installation.
// Constants calibrated against thermal-drill power budget in Phase 42+.

namespace gazebo_cryobot
{

static constexpr double TETHER_STIFFNESS_NM = 500.0;  // [N/m]
static constexpr double TETHER_REST_LEN_M   = 10.0;   // [m]

struct CrybotStep
{
    double tether_tension_n;  // restoring tether tension [N]
    double net_force_z;       // net body-frame +Z force [N] (positive = up)
};

inline CrybotStep compute_step(double depth_m, double descent_ms) noexcept
{
    const double extension    = (depth_m > TETHER_REST_LEN_M)
                                ? (depth_m - TETHER_REST_LEN_M) : 0.0;
    const double tension      = TETHER_STIFFNESS_NM * extension;
    const double net_force_z  = (descent_ms * 10.0) - tension;
    return {tension, net_force_z};
}

}  // namespace gazebo_cryobot
