#pragma once

#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/common/common.hh>

namespace gazebo
{

/**
 * RoverDrivePlugin — Gazebo ModelPlugin for a wheeled rover chassis.
 *
 * Attach to a model SDF element:
 *   <plugin name="rover_drive" filename="librover_drive_plugin.so"/>
 *
 * OnUpdate() is called every simulation step. It must not block.
 */
class RoverDrivePlugin : public ModelPlugin
{
public:
    RoverDrivePlugin();
    ~RoverDrivePlugin() override;

    void Load(physics::ModelPtr model, sdf::ElementPtr sdf) override;
    void Reset() override;

private:
    void OnUpdate();

    physics::ModelPtr     model_;
    event::ConnectionPtr  update_connection_;
};

GZ_REGISTER_MODEL_PLUGIN(RoverDrivePlugin)

}  // namespace gazebo
