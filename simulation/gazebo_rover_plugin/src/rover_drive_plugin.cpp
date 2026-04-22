#include "rover_drive_plugin.h"

#include <gazebo/transport/transport.hh>
#include <functional>

namespace gazebo
{

RoverDrivePlugin::RoverDrivePlugin()
: model_(nullptr)
{
}

RoverDrivePlugin::~RoverDrivePlugin()
{
    /* update_connection_ RAII destructor disconnects the WorldUpdateBegin
     * signal automatically — no explicit Disconnect() call needed. */
}

void RoverDrivePlugin::Load(physics::ModelPtr model, sdf::ElementPtr /*sdf*/)
{
    if (!model)
    {
        gzerr << "[RoverDrivePlugin] Load called with null model pointer\n";
        return;
    }

    model_ = model;

    /* Connect to the world update signal. The connection object keeps the
     * callback alive — Reset() does not disconnect it. */
    update_connection_ = event::Events::ConnectWorldUpdateBegin(
        std::bind(&RoverDrivePlugin::OnUpdate, this));

    gzmsg << "[RoverDrivePlugin] Loaded on model: " << model_->GetName() << "\n";
}

void RoverDrivePlugin::Reset()
{
    /* Called when the simulation is reset. Joint velocities are zeroed by
     * the physics engine; we only need to reset plugin-internal state here. */
}

void RoverDrivePlugin::OnUpdate()
{
    /* Called every simulation step. Must not block.
     * This stub reads joint states; a full implementation would apply
     * torques based on a subscribed cmd_vel message. */
    if (!model_)
    {
        return;
    }

    /* Read wheel joint states for telemetry (example — extend as needed) */
    auto joints = model_->GetJoints();
    for (const auto & joint : joints)
    {
        (void)joint->GetVelocity(0U); /* axis 0 angular velocity */
    }
}

}  // namespace gazebo
