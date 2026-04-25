#pragma once

#include <gz/sim/System.hh>
#include <gz/sim/EntityComponentManager.hh>
#include <gz/sim/EventManager.hh>
#include <gz/transport/Node.hh>
#include <gz/msgs/twist.pb.h>

#include <mutex>
#include <string>

/**
 * RoverDrivePlugin — Gazebo Harmonic system plugin for a wheeled rover chassis.
 *
 * Attach to a model SDF element:
 *   <plugin name="rover_drive" filename="librover_drive_plugin.so"/>
 *
 * Subscribes to "/model/<model_name>/cmd_vel" (gz::msgs::Twist) for
 * differential-drive velocity commands (linear.x = forward m/s,
 * angular.z = yaw rad/s).  PreUpdate() applies per-wheel angular velocity
 * to left_wheel_joint and right_wheel_joint via JointVelocityCmd each step.
 */
class RoverDrivePlugin :
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
    gz::sim::Entity leftJointEntity_{gz::sim::kNullEntity};
    gz::sim::Entity rightJointEntity_{gz::sim::kNullEntity};

    gz::transport::Node node_;

    double lin_vel_{0.0};
    double ang_vel_{0.0};
    std::mutex cmd_vel_mutex_;
};
