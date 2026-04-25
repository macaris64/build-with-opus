#include "world_environment_plugin.h"

#include <functional>

namespace gazebo
{

/* Log every N physics steps to avoid flooding the console.
 * At 1000 Hz physics this is one log line per second. */
static constexpr uint64_t LOG_INTERVAL_TICKS = 1000U;

WorldEnvironmentPlugin::WorldEnvironmentPlugin()
: world_(nullptr)
{
}

WorldEnvironmentPlugin::~WorldEnvironmentPlugin()
{
    /* update_connection_ RAII destructor disconnects the signal automatically. */
}

void WorldEnvironmentPlugin::Load(physics::WorldPtr world, sdf::ElementPtr /*sdf*/)
{
    if (!world)
    {
        gzerr << "[WorldEnvironmentPlugin] Load called with null world pointer\n";
        return;
    }

    world_ = world;

    update_connection_ = event::Events::ConnectWorldUpdateBegin(
        std::bind(&WorldEnvironmentPlugin::OnUpdate, this));

    /* Log environment properties at startup for verification (Phase A gate). */
    const auto gravity = world_->Gravity();
    gzmsg << "[WorldEnvironmentPlugin] Loaded world: " << world_->Name() << "\n"
          << "  gravity      : (" << gravity.X() << ", "
                                  << gravity.Y() << ", "
                                  << gravity.Z() << ") m/s²\n"
          << "  max_step_size: " << world_->Physics()->GetMaxStepSize() << " s\n";
}

void WorldEnvironmentPlugin::OnUpdate()
{
    if (!world_)
    {
        return;
    }

    ++tick_count_;

    if (tick_count_ % LOG_INTERVAL_TICKS == 0U)
    {
        /* Periodic heartbeat — Phase 39+ will replace this with sideband
         * SPP emission to the FSW sim_adapter app. */
        gzmsg << "[WorldEnvironmentPlugin] sim_time="
              << world_->SimTime().Double() << " s"
              << "  tick=" << tick_count_ << "\n";
    }
}

}  // namespace gazebo
