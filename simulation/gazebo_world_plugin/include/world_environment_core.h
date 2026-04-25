#pragma once

#include <cstdint>

// Pure world-environment hot-path — no Gazebo types.
// Called from WorldEnvironmentPlugin::OnUpdate(); tested without Gazebo.
// Phase 39+ replaces the heartbeat log with sideband SPP emission.

namespace gazebo_world
{

static constexpr uint64_t LOG_INTERVAL_TICKS = 1000U;

inline bool should_emit_heartbeat(uint64_t tick_count) noexcept
{
    return (tick_count % LOG_INTERVAL_TICKS) == 0U;
}

}  // namespace gazebo_world
