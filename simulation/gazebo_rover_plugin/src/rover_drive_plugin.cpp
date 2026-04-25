#include "rover_drive_plugin.h"

#include <gazebo/msgs/msgs.hh>
#include <gazebo/transport/transport.hh>

#include <functional>

namespace gazebo
{

/* Physical parameters of the rover chassis used for differential-drive kinematics. */
static constexpr double WHEEL_RADIUS_M  = 0.1;   /* wheel radius [m] */
static constexpr double HALF_TRACK_M    = 0.2;   /* half track width [m] */

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

    node_ = transport::NodePtr(new transport::Node());
    node_->Init();
    const std::string topic = std::string("~/") + model_->GetName() + "/cmd_vel";
    cmd_vel_sub_ = node_->Subscribe(topic, &RoverDrivePlugin::OnCmdVel, this);

    update_connection_ = event::Events::ConnectWorldUpdateBegin(
        std::bind(&RoverDrivePlugin::OnUpdate, this));

    gzmsg << "[RoverDrivePlugin] Loaded on model: " << model_->GetName()
          << " — cmd_vel topic: " << topic << "\n";
}

void RoverDrivePlugin::Reset()
{
    std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
    lin_vel_ = 0.0;
    ang_vel_ = 0.0;
}

void RoverDrivePlugin::OnCmdVel(ConstTwistPtr &msg)
{
    std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
    lin_vel_ = msg->linear().x();
    ang_vel_ = msg->angular().z();
}

void RoverDrivePlugin::OnUpdate()
{
    if (!model_)
    {
        return;
    }

    double lin = 0.0;
    double ang = 0.0;
    {
        std::lock_guard<std::mutex> lock(cmd_vel_mutex_);
        lin = lin_vel_;
        ang = ang_vel_;
    }

    /* Differential drive kinematics: convert (v, ω) to per-wheel angular velocity. */
    const double left_rad_s  = (lin - ang * HALF_TRACK_M) / WHEEL_RADIUS_M;
    const double right_rad_s = (lin + ang * HALF_TRACK_M) / WHEEL_RADIUS_M;

    auto left_joint  = model_->GetJoint("left_wheel_joint");
    auto right_joint = model_->GetJoint("right_wheel_joint");

    if (left_joint)
    {
        left_joint->SetVelocity(0U, left_rad_s);
    }
    if (right_joint)
    {
        right_joint->SetVelocity(0U, right_rad_s);
    }
}

}  // namespace gazebo
