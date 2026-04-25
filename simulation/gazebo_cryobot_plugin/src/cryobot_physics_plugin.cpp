#include "cryobot_physics_plugin.h"
#include "cryobot_physics_core.h"

#include <gz/sim/Model.hh>
#include <gz/sim/components/ExternalWorldWrenchCmd.hh>
#include <gz/sim/Util.hh>
#include <gz/msgs/wrench.pb.h>
#include <gz/plugin/Register.hh>

#include <string>
#include <vector>

void CrybotPhysicsPlugin::Configure(
    const gz::sim::Entity &entity,
    const std::shared_ptr<const sdf::Element> &,
    gz::sim::EntityComponentManager &ecm,
    gz::sim::EventManager &)
{
    entity_ = entity;
    gz::sim::Model model(entity);

    /* Use the first link as the base link for tether force application. */
    const std::vector<gz::sim::Entity> links = model.Links(ecm);
    if (!links.empty()) {
        linkEntity_ = links.front();
        ecm.CreateComponent(linkEntity_,
            gz::sim::components::ExternalWorldWrenchCmd());
    } else {
        gzerr << "[CrybotPhysicsPlugin] No links found on model — forces will not be applied\n";
    }

    const std::string modelName = model.Name(ecm);
    const std::string topic = "/model/" + modelName + "/cmd_vel";
    node_.Subscribe(topic, &CrybotPhysicsPlugin::OnCmdVel, this);

    gzmsg << "[CrybotPhysicsPlugin] Loaded on model: " << modelName
          << " — cmd_vel topic: " << topic << "\n";
}

void CrybotPhysicsPlugin::PreUpdate(
    const gz::sim::UpdateInfo &,
    gz::sim::EntityComponentManager &ecm)
{
    if (linkEntity_ == gz::sim::kNullEntity)
        return;

    /* Compute approximate depth from world Z as proxy for tether extension.
     * Phase 42+ reads the tether joint state directly. */
    const gz::math::Pose3d wPose = gz::sim::worldPose(linkEntity_, ecm);
    const double depth = -wPose.Pos().Z();

    double descent = 0.0;
    {
        std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
        descent = descent_rate_ms_;
    }

    const auto step = gazebo_cryobot::compute_step(depth, descent);

    {
        std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
        tether_tension_n_ = step.tether_tension_n;
    }

    /* Apply net axial force: tether restoring (upward) + descent thrust. */
    auto *wrenchCmd = ecm.Component<gz::sim::components::ExternalWorldWrenchCmd>(linkEntity_);
    if (wrenchCmd) {
        gz::msgs::Wrench wrench;
        wrench.mutable_force()->set_x(0.0);
        wrench.mutable_force()->set_y(0.0);
        wrench.mutable_force()->set_z(step.net_force_z);
        wrench.mutable_torque()->set_x(0.0);
        wrench.mutable_torque()->set_y(0.0);
        wrench.mutable_torque()->set_z(0.0);
        wrenchCmd->Data() = wrench;
    }
}

void CrybotPhysicsPlugin::OnCmdVel(const gz::msgs::Twist &msg)
{
    std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
    /* linear.z is the commanded descent rate (negative = down in NED). */
    descent_rate_ms_ = msg.linear().z();
}

GZ_ADD_PLUGIN(CrybotPhysicsPlugin, gz::sim::System,
              gz::sim::ISystemConfigure, gz::sim::ISystemPreUpdate)
GZ_ADD_PLUGIN_ALIAS(CrybotPhysicsPlugin, "cryobot_physics")
