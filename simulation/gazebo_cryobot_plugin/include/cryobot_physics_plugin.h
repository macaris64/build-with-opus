#pragma once

#include <gz/sim/System.hh>
#include <gz/sim/EntityComponentManager.hh>
#include <gz/sim/EventManager.hh>
#include <gz/transport/Node.hh>
#include <gz/msgs/twist.pb.h>

#include <mutex>
#include <string>

/**
 * CrybotPhysicsPlugin — Gazebo Harmonic system plugin for a tethered cryobot.
 *
 * Attach to a model SDF element:
 *   <plugin name="cryobot_physics" filename="libcryobot_physics_plugin.so"/>
 *
 * Models tether spring restoring force and descent thrust.  Subscribes to
 * "/model/<model_name>/cmd_vel" for downward velocity commands (linear.z).
 * PreUpdate() applies net axial force to the base link each physics step.
 *
 * Phase 38: tether extension derived from world-Z pose; PD controller lands
 * in Phase 42.
 */
class CrybotPhysicsPlugin :
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

    double tether_tension_n_{0.0};
    double descent_rate_ms_{0.0};
    std::mutex cmd_vel_mutex_;
};
