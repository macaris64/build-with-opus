#pragma once

#include <gazebo/gazebo.hh>
#include <gazebo/physics/WorldPtr.hh>
#include <gazebo/common/common.hh>

namespace gazebo
{

/**
 * WorldEnvironmentPlugin — Gazebo WorldPlugin for the Mars-surrogate environment.
 *
 * Attach to the world SDF element:
 *   <plugin name="world_environment" filename="libworld_environment_plugin.so"/>
 *
 * Responsibilities:
 *   - Logs environment properties (gravity, wind, time step) at startup.
 *   - Forwards simulation-time ticks for synchronisation with the TAI authority
 *     (clock_link_model); actual clock authority is a separate container per
 *     docs/architecture/08-timing-and-clocks.md §5.2.
 *   - Emits periodic env telemetry (stub; wired to ICD-sim-fsw in Phase 39+).
 */
class WorldEnvironmentPlugin : public WorldPlugin
{
public:
    WorldEnvironmentPlugin();
    ~WorldEnvironmentPlugin() override;

    void Load(physics::WorldPtr world, sdf::ElementPtr sdf) override;

private:
    void OnUpdate();

    physics::WorldPtr    world_;
    event::ConnectionPtr update_connection_;

    uint64_t tick_count_{0U};
};

GZ_REGISTER_WORLD_PLUGIN(WorldEnvironmentPlugin)

}  // namespace gazebo
