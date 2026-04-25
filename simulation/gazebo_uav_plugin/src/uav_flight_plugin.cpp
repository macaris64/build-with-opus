#include "uav_flight_plugin.h"
#include "uav_flight_core.h"

#include <gz/sim/Model.hh>
#include <gz/sim/components/ExternalWorldWrenchCmd.hh>
#include <gz/msgs/wrench.pb.h>
#include <gz/plugin/Register.hh>

#include <string>
#include <vector>

void UavFlightPlugin::Configure(
    const gz::sim::Entity &entity,
    const std::shared_ptr<const sdf::Element> &,
    gz::sim::EntityComponentManager &ecm,
    gz::sim::EventManager &)
{
    entity_ = entity;
    gz::sim::Model model(entity);

    /* Use the first link as the base link for force and torque application.
     * Phase 42+ will replace this with per-rotor entity selection. */
    const std::vector<gz::sim::Entity> links = model.Links(ecm);
    if (!links.empty()) {
        linkEntity_ = links.front();
        ecm.CreateComponent(linkEntity_,
            gz::sim::components::ExternalWorldWrenchCmd());
    } else {
        gzerr << "[UavFlightPlugin] No links found on model — forces will not be applied\n";
    }

    const std::string modelName = model.Name(ecm);
    const std::string topic = "/model/" + modelName + "/cmd_vel";
    node_.Subscribe(topic, &UavFlightPlugin::OnCmdVel, this);

    gzmsg << "[UavFlightPlugin] Loaded on model: " << modelName
          << " — cmd_vel topic: " << topic << "\n";
}

void UavFlightPlugin::PreUpdate(
    const gz::sim::UpdateInfo &,
    gz::sim::EntityComponentManager &ecm)
{
    if (linkEntity_ == gz::sim::kNullEntity)
        return;

    double thrust   = 0.0;
    double yaw_rate = 0.0;
    {
        std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
        thrust   = thrust_;
        yaw_rate = yaw_rate_;
    }

    /* Apply collective thrust as world-frame +Z force; yaw torque about world
     * +Z.  Forces are in world frame — appropriate for a hover-stable UAV at
     * small angles.  Phase 42+ replaces with body-frame per-rotor wrench. */
    const auto cmd = gazebo_uav::compute_force_cmd(thrust, yaw_rate);

    auto *wrenchCmd = ecm.Component<gz::sim::components::ExternalWorldWrenchCmd>(linkEntity_);
    if (wrenchCmd) {
        gz::msgs::Wrench wrench;
        wrench.mutable_force()->set_x(0.0);
        wrench.mutable_force()->set_y(0.0);
        wrench.mutable_force()->set_z(cmd.force_z);
        wrench.mutable_torque()->set_x(0.0);
        wrench.mutable_torque()->set_y(0.0);
        wrench.mutable_torque()->set_z(cmd.torque_z);
        wrenchCmd->Data() = wrench;
    }
}

void UavFlightPlugin::OnCmdVel(const gz::msgs::Twist &msg)
{
    std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
    /* linear.z is mapped to collective thrust; angular.z to yaw rate. */
    thrust_   = msg.linear().z();
    yaw_rate_ = msg.angular().z();
}

GZ_ADD_PLUGIN(UavFlightPlugin, gz::sim::System,
              gz::sim::ISystemConfigure, gz::sim::ISystemPreUpdate)
GZ_ADD_PLUGIN_ALIAS(UavFlightPlugin, "uav_flight")
