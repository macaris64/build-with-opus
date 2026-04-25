#pragma once

#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/common/common.hh>
#include <gazebo/transport/transport.hh>

#include <mutex>

namespace gazebo
{

/**
 * CrybotPhysicsPlugin — Gazebo ModelPlugin for a tethered subsurface cryobot.
 *
 * Attach to a model SDF element:
 *   <plugin name="cryobot_physics" filename="libcryobot_physics_plugin.so"/>
 *
 * Models tether tension and ice-penetration force. Subscribes to
 * "~/<model_name>/cmd_vel" for downward velocity commands.
 * OnUpdate() applies net axial force each simulation step.
 */
class CrybotPhysicsPlugin : public ModelPlugin
{
public:
    CrybotPhysicsPlugin();
    ~CrybotPhysicsPlugin() override;

    void Load(physics::ModelPtr model, sdf::ElementPtr sdf) override;
    void Reset() override;

private:
    void OnUpdate();
    void OnCmdVel(ConstTwistPtr &msg);

    physics::ModelPtr    model_;
    event::ConnectionPtr update_connection_;

    transport::NodePtr       node_;
    transport::SubscriberPtr cmd_vel_sub_;

    /* Tether tension magnitude [N]; updated each physics step based on depth. */
    double tether_tension_n_{0.0};
    double descent_rate_ms_{0.0};
    std::mutex cmd_vel_mutex_;
};

GZ_REGISTER_MODEL_PLUGIN(CrybotPhysicsPlugin)

}  // namespace gazebo
