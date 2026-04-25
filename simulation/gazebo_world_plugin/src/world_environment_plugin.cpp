#include "world_environment_plugin.h"
#include "world_environment_core.h"

#include <gz/sim/World.hh>
#include <gz/plugin/Register.hh>

void WorldEnvironmentPlugin::Configure(
    const gz::sim::Entity &entity,
    const std::shared_ptr<const sdf::Element> &,
    gz::sim::EntityComponentManager &ecm,
    gz::sim::EventManager &)
{
    entity_ = entity;
    gz::sim::World world(entity);

    const std::string name = world.Name(ecm).value_or("unknown");
    const auto gravOpt = world.Gravity(ecm);

    gzmsg << "[WorldEnvironmentPlugin] Loaded world: " << name << "\n";
    if (gravOpt) {
        gzmsg << "  gravity: (" << gravOpt->X() << ", "
                                << gravOpt->Y() << ", "
                                << gravOpt->Z() << ") m/s²\n";
    }
}

void WorldEnvironmentPlugin::PostUpdate(
    const gz::sim::UpdateInfo &,
    const gz::sim::EntityComponentManager &)
{
    ++tick_count_;

    if (gazebo_world::should_emit_heartbeat(tick_count_)) {
        /* Periodic heartbeat — Phase 39+ will replace this with sideband
         * SPP emission to the FSW sim_adapter app. */
        gzmsg << "[WorldEnvironmentPlugin] tick=" << tick_count_ << "\n";
    }
}

GZ_ADD_PLUGIN(WorldEnvironmentPlugin, gz::sim::System,
              gz::sim::ISystemConfigure, gz::sim::ISystemPostUpdate)
GZ_ADD_PLUGIN_ALIAS(WorldEnvironmentPlugin, "world_environment")
