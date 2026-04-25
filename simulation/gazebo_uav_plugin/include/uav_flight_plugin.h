#pragma once

#include <gz/sim/System.hh>
#include <gz/sim/EntityComponentManager.hh>
#include <gz/sim/EventManager.hh>
#include <gz/transport/Node.hh>
#include <gz/msgs/twist.pb.h>

#include <mutex>
#include <string>

/**
 * UavFlightPlugin — Gazebo Harmonic system plugin for a UAV (rotary wing).
 *
 * Attach to a model SDF element:
 *   <plugin name="uav_flight" filename="libuav_flight_plugin.so"/>
 *
 * Subscribes to "/model/<model_name>/cmd_vel" for thrust + yaw commands
 * (linear.z = collective thrust N, angular.z = yaw rate rad/s).
 * PreUpdate() applies net force/torque to the base link via
 * ExternalWorldWrenchCmd each physics step.
 *
 * Phase 38: direct mapping; per-rotor torque model lands in Phase 42.
 */
class UavFlightPlugin :
    public gz::sim::System,
    public gz::sim::ISystemConfigure,
    public gz::sim::ISystemPreUpdate
{
public:
    void Configure(const gz::sim::Entity &entity,
                   const std::shared_ptr<const sdf::Element> &sdf,
                   gz::sim::EntityComponentManager &ecm,
                   gz::sim::EventManager &eventMgr) override;

    void PreUpdate(const gz::sim::UpdateInfo &info,
                   gz::sim::EntityComponentManager &ecm) override;

private:
    void OnCmdVel(const gz::msgs::Twist &msg);

    gz::sim::Entity entity_{gz::sim::kNullEntity};
    gz::sim::Entity linkEntity_{gz::sim::kNullEntity};

    gz::transport::Node node_;

    double thrust_{0.0};
    double yaw_rate_{0.0};
    std::mutex cmd_vel_mutex_;
};
