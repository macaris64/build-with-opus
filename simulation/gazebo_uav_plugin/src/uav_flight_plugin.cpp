#include "uav_flight_plugin.h"

#include <gazebo/msgs/msgs.hh>

#include <functional>

namespace gazebo
{

UavFlightPlugin::UavFlightPlugin()
: model_(nullptr)
{
}

UavFlightPlugin::~UavFlightPlugin()
{
    /* update_connection_ RAII destructor disconnects the WorldUpdateBegin
     * signal automatically. */
}

void UavFlightPlugin::Load(physics::ModelPtr model, sdf::ElementPtr /*sdf*/)
{
    if (!model)
    {
        gzerr << "[UavFlightPlugin] Load called with null model pointer\n";
        return;
    }

    model_ = model;

    node_ = transport::NodePtr(new transport::Node());
    node_->Init();
    const std::string topic = std::string("~/") + model_->GetName() + "/cmd_vel";
    cmd_vel_sub_ = node_->Subscribe(topic, &UavFlightPlugin::OnCmdVel, this);

    update_connection_ = event::Events::ConnectWorldUpdateBegin(
        std::bind(&UavFlightPlugin::OnUpdate, this));

    gzmsg << "[UavFlightPlugin] Loaded on model: " << model_->GetName()
          << " — cmd_vel topic: " << topic << "\n";
}

void UavFlightPlugin::Reset()
{
    std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
    thrust_   = 0.0;
    yaw_rate_ = 0.0;
}

void UavFlightPlugin::OnCmdVel(ConstTwistPtr &msg)
{
    std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
    /* linear.z is mapped to collective thrust; angular.z to yaw rate. */
    thrust_   = msg->linear().z();
    yaw_rate_ = msg->angular().z();
}

void UavFlightPlugin::OnUpdate()
{
    if (!model_)
    {
        return;
    }

    double thrust   = 0.0;
    double yaw_rate = 0.0;
    {
        std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
        thrust   = thrust_;
        yaw_rate = yaw_rate_;
    }

    /* Apply collective thrust as an upward body-frame force. The rotor
     * joints are used in a full implementation; for Phase 38 we apply
     * the net force directly to the base link so the UAV is controllable
     * in simulation before per-rotor torque modeling lands. */
    auto links = model_->GetLinks();
    if (!links.empty())
    {
        auto base = links.front();
        /* Force in world Z; torque about world Z for yaw. */
        base->AddRelativeForce(math::Vector3(0.0, 0.0, thrust));
        base->AddRelativeTorque(math::Vector3(0.0, 0.0, yaw_rate));
    }
}

}  // namespace gazebo
