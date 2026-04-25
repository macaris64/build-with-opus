#pragma once

// Pure UAV flight-physics hot-path — no Gazebo types.
// Called from UavFlightPlugin::OnUpdate(); tested without a Gazebo installation.
// Phase 42+ will replace the direct mapping with a per-rotor torque model here.

namespace gazebo_uav
{

struct UavForceCmd
{
    double force_z;   // body-frame +Z collective force [N]
    double torque_z;  // body-frame +Z yaw torque [N·m]
};

inline UavForceCmd compute_force_cmd(double thrust, double yaw_rate) noexcept
{
    return {thrust, yaw_rate};
}

}  // namespace gazebo_uav
