#include "rover_drive_plugin.h"

#include <gz/sim/Model.hh>
#include <gz/sim/components/JointVelocityCmd.hh>
#include <gz/plugin/Register.hh>

#include <string>

/* Physical parameters of the rover chassis. */
static constexpr double WHEEL_RADIUS_M = 0.1;
static constexpr double HALF_TRACK_M   = 0.2;

void RoverDrivePlugin::Configure(
    const gz::sim::Entity &entity,
    const std::shared_ptr<const sdf::Element> &,
    gz::sim::EntityComponentManager &ecm,
    gz::sim::EventManager &)
{
    entity_ = entity;
    gz::sim::Model model(entity);

    leftJointEntity_  = model.JointByName(ecm, "left_wheel_joint");
    rightJointEntity_ = model.JointByName(ecm, "right_wheel_joint");

    if (leftJointEntity_ != gz::sim::kNullEntity)
        ecm.CreateComponent(leftJointEntity_,
            gz::sim::components::JointVelocityCmd({0.0}));
    if (rightJointEntity_ != gz::sim::kNullEntity)
        ecm.CreateComponent(rightJointEntity_,
            gz::sim::components::JointVelocityCmd({0.0}));

    const std::string modelName = model.Name(ecm);
    const std::string topic = "/model/" + modelName + "/cmd_vel";
    node_.Subscribe(topic, &RoverDrivePlugin::OnCmdVel, this);

    gzmsg << "[RoverDrivePlugin] Loaded on model: " << modelName
          << " — cmd_vel topic: " << topic << "\n";
}

void RoverDrivePlugin::PreUpdate(
    const gz::sim::UpdateInfo &,
    gz::sim::EntityComponentManager &ecm)
{
    double lin = 0.0;
    double ang = 0.0;
    {
        std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
        lin = lin_vel_;
        ang = ang_vel_;
    }

    /* Differential drive kinematics: convert (v, ω) to per-wheel angular velocity. */
    const double left_rad_s  = (lin - ang * HALF_TRACK_M) / WHEEL_RADIUS_M;
    const double right_rad_s = (lin + ang * HALF_TRACK_M) / WHEEL_RADIUS_M;

    if (leftJointEntity_ != gz::sim::kNullEntity) {
        auto *cmd = ecm.Component<gz::sim::components::JointVelocityCmd>(leftJointEntity_);
        if (cmd) cmd->Data() = {left_rad_s};
    }
    if (rightJointEntity_ != gz::sim::kNullEntity) {
        auto *cmd = ecm.Component<gz::sim::components::JointVelocityCmd>(rightJointEntity_);
        if (cmd) cmd->Data() = {right_rad_s};
    }
}

void RoverDrivePlugin::OnCmdVel(const gz::msgs::Twist &msg)
{
    std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
    lin_vel_ = msg.linear().x();
    ang_vel_ = msg.angular().z();
}

GZ_ADD_PLUGIN(RoverDrivePlugin, gz::sim::System,
              gz::sim::ISystemConfigure, gz::sim::ISystemPreUpdate)
GZ_ADD_PLUGIN_ALIAS(RoverDrivePlugin, "rover_drive")
