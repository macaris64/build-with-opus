#pragma once

#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/common/common.hh>
#include <gazebo/transport/transport.hh>

#include <mutex>

namespace gazebo
{

/**
 * UavFlightPlugin — Gazebo ModelPlugin for a UAV (fixed or rotary wing).
 *
 * Attach to a model SDF element:
 *   <plugin name="uav_flight" filename="libuav_flight_plugin.so"/>
 *
 * Subscribes to "~/<model_name>/cmd_vel" for attitude + thrust commands.
 * OnUpdate() applies torques to rotor joints each simulation step.
 */
class UavFlightPlugin : public ModelPlugin
{
public:
    UavFlightPlugin();
    ~UavFlightPlugin() override;

    void Load(physics::ModelPtr model, sdf::ElementPtr sdf) override;
    void Reset() override;

private:
    void OnUpdate();
    void OnCmdVel(ConstTwistPtr &msg);

    physics::ModelPtr    model_;
    event::ConnectionPtr update_connection_;

    transport::NodePtr       node_;
    transport::SubscriberPtr cmd_vel_sub_;

    double thrust_{0.0};
    double yaw_rate_{0.0};
    std::mutex cmd_vel_mutex_;
};

GZ_REGISTER_MODEL_PLUGIN(UavFlightPlugin)

}  // namespace gazebo
