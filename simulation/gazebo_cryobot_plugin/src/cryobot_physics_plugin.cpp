#include "cryobot_physics_plugin.h"

#include <gazebo/msgs/msgs.hh>

#include <functional>

namespace gazebo
{

/* Tether linear stiffness [N/m] — conservative estimate for Phase 38.
 * Exact value calibrated against thermal-drill power budget in Phase 42+. */
static constexpr double TETHER_STIFFNESS_NM = 500.0;

/* Nominal tether rest length [m] — updated at runtime in future phases. */
static constexpr double TETHER_REST_LEN_M = 10.0;

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
     * state directly.  Extension = max(0, depth - rest_len). */
    const double depth = -base->GetWorldPose().pos.z;
    const double extension = (depth > TETHER_REST_LEN_M)
                             ? (depth - TETHER_REST_LEN_M) : 0.0;

    double tension  = 0.0;
    double descent  = 0.0;
    {
        std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
        tether_tension_n_ = TETHER_STIFFNESS_NM * extension;
        tension  = tether_tension_n_;
        descent  = descent_rate_ms_;
    }

    /* Apply tether restoring force (upward) + commanded descent force.
     * Net force = thermal-drill thrust (from descent_rate PD controller,
     * Phase 42) - tether_tension.  For Phase 38 we apply a proportional
     * descent force directly. */
    const double net_force_z = (descent * 10.0) - tension;
    base->AddRelativeForce(math::Vector3(0.0, 0.0, net_force_z));
}

}  // namespace gazebo
