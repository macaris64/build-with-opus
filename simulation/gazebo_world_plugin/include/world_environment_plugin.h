#pragma once

#include <gz/sim/System.hh>
#include <gz/sim/EntityComponentManager.hh>
#include <gz/sim/EventManager.hh>

#include <cstdint>

/**
 * WorldEnvironmentPlugin — Gazebo Harmonic system plugin for the Mars-surrogate world.
 *
 * Attach to the world SDF element:
 *   <plugin name="world_environment" filename="libworld_environment_plugin.so"/>
 *
 * Responsibilities:
 *   - Logs gravity, world name, and physics step size at startup.
 *   - Emits periodic heartbeat ticks (Phase 39+ wires to ICD-sim-fsw sideband SPP).
 *   - Does NOT set time authority — that is a separate clock_link_model container
 *     per docs/architecture/08-timing-and-clocks.md §5.2.
 */
class WorldEnvironmentPlugin :
    public gz::sim::System,
    public gz::sim::ISystemConfigure,
    public gz::sim::ISystemPostUpdate
{
public:
    void Configure(const gz::sim::Entity &entity,
                   const std::shared_ptr<const sdf::Element> &sdf,
                   gz::sim::EntityComponentManager &ecm,
                   gz::sim::EventManager &eventMgr) override;

    void PostUpdate(const gz::sim::UpdateInfo &info,
                    const gz::sim::EntityComponentManager &ecm) override;

private:
    gz::sim::Entity entity_{gz::sim::kNullEntity};
    uint64_t tick_count_{0U};
};
