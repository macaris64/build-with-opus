#include "cryobot_physics_plugin.h"
#include "cryobot_physics_core.h"

#include <gazebo/msgs/msgs.hh>

#include <functional>

namespace gazebo
{

CrybotPhysicsPlugin::CrybotPhysicsPlugin()
: model_(nullptr)
{
}

CrybotPhysicsPlugin::~CrybotPhysicsPlugin()
{
    /* update_connection_ RAII destructor disconnects the WorldUpdateBegin
     * signal automatically. */
}

void CrybotPhysicsPlugin::Load(physics::ModelPtr model, sdf::ElementPtr /*sdf*/)
{
    if (!model)
    {
        gzerr << "[CrybotPhysicsPlugin] Load called with null model pointer\n";
        return;
    }

    model_ = model;

    node_ = transport::NodePtr(new transport::Node());
    node_->Init();
    const std::string topic = std::string("~/") + model_->GetName() + "/cmd_vel";
    cmd_vel_sub_ = node_->Subscribe(topic, &CrybotPhysicsPlugin::OnCmdVel, this);

    update_connection_ = event::Events::ConnectWorldUpdateBegin(
        std::bind(&CrybotPhysicsPlugin::OnUpdate, this));

    gzmsg << "[CrybotPhysicsPlugin] Loaded on model: " << model_->GetName()
          << " — cmd_vel topic: " << topic << "\n";
}

void CrybotPhysicsPlugin::Reset()
{
    std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
    tether_tension_n_ = 0.0;
    descent_rate_ms_  = 0.0;
}

void CrybotPhysicsPlugin::OnCmdVel(ConstTwistPtr &msg)
{
    std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
    /* linear.z is the commanded descent rate (negative = down in NED). */
    descent_rate_ms_ = msg->linear().z();
}

void CrybotPhysicsPlugin::OnUpdate()
{
    if (!model_)
    {
        return;
    }

    auto links = model_->GetLinks();
    if (links.empty())
    {
        return;
    }

    auto base = links.front();

    /* Compute approximate depth from world origin as a proxy for tether
     * extension. A full Phase 42+ implementation reads the tether joint
     * state directly. */
    const double depth = -base->GetWorldPose().pos.z;

    double descent = 0.0;
    {
        std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
        descent = descent_rate_ms_;
    }

    const auto step = gazebo_cryobot::compute_step(depth, descent);

    {
        std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
        tether_tension_n_ = step.tether_tension_n;
    }

    /* Apply net axial force: tether restoring (upward) + descent thrust.
     * Phase 42+ replaces the proportional mapping with a PD controller. */
    base->AddRelativeForce(math::Vector3(0.0, 0.0, step.net_force_z));
}

}  // namespace gazebo
