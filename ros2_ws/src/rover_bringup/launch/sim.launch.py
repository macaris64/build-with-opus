"""
sim.launch.py — Combined Gazebo + ROS 2 simulation launch for SAKURA-II.

Starts three processes in dependency order:
  1. Gazebo Harmonic server (headless by default) loading mars_surrogate.sdf
  2. ros_gz_bridge parameter_bridge: bridges /cmd_vel between ROS 2 and Gazebo
  3. teleop_node (lifecycle) with parameters from rover_params.yaml

Requirements:
  - Gazebo Harmonic (gz-sim8) installed: libgz-sim8-dev
  - ros_gz_sim and ros_gz_bridge ROS 2 packages installed
  - simulation/worlds/mars_surrogate.sdf present in the repo

Phase B gate (HOW_TO_RUN.md §7):
  Verify /cmd_vel flows from teleop_node through the bridge into the rover
  model joints by running:
    ros2 topic echo /cmd_vel
"""

import os

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    TimerAction,
)
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import LifecycleNode, Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    # ── Launch arguments ──────────────────────────────────────────────────────

    params_file_arg = DeclareLaunchArgument(
        "params_file",
        default_value=PathJoinSubstitution(
            [FindPackageShare("rover_bringup"), "config", "rover_params.yaml"]
        ),
        description="Path to the rover parameter file",
    )

    # Default SDF path assumes the user runs from the repo root or that the
    # world file is accessible at this relative path. Override at launch time:
    #   ros2 launch rover_bringup sim.launch.py sdf_path:=/abs/path/mars.sdf
    sdf_path_default = os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(
            os.path.dirname(os.path.abspath(__file__))))),  # ros2_ws/src
        os.pardir, os.pardir,  # repo root
        "simulation", "worlds", "mars_surrogate.sdf",
    )
    sdf_path_arg = DeclareLaunchArgument(
        "sdf_path",
        default_value=os.path.normpath(sdf_path_default),
        description="Absolute path to the Gazebo world SDF file",
    )

    headless_arg = DeclareLaunchArgument(
        "headless",
        default_value="true",
        description="Run Gazebo without GUI (true=headless, false=with GUI)",
    )

    # ── Process 1: Gazebo Harmonic server ────────────────────────────────────
    # gz sim -r -s loads the world in headless (server-only) mode.
    # The -r flag starts physics immediately (no manual 'play' required).
    # When headless:=false, drop the -s flag to launch the full GUI.
    gz_server = ExecuteProcess(
        cmd=[
            "gz", "sim",
            "-r",           # run physics immediately on load
            "-s",           # server-only (headless); remove for GUI
            LaunchConfiguration("sdf_path"),
        ],
        output="screen",
        name="gz_sim_server",
    )

    # ── Process 2: ros_gz_bridge ─────────────────────────────────────────────
    # Bridge topics between ROS 2 and Gazebo.
    #
    # Topic format for parameter_bridge:
    #   /topic@ros_msg_type]gz_msg_type  — ROS 2 → Gazebo
    #   /topic@ros_msg_type[gz_msg_type  — Gazebo → ROS 2
    #   /topic@ros_msg_type              — bidirectional
    #
    # /cmd_vel flows ROS 2 → Gazebo so the teleop_node commands reach the
    # rover model's rover_drive_plugin (subscribed to ~/rover_land_0/cmd_vel
    # in Gazebo transport).
    # /model/rover_land_0/odometry flows Gazebo → ROS 2 for state feedback.
    gz_bridge = TimerAction(
        # Delay bridge startup by 2 s to allow Gazebo to finish loading the
        # world and register its topics before the bridge tries to connect.
        period=2.0,
        actions=[
            Node(
                package="ros_gz_bridge",
                executable="parameter_bridge",
                name="gz_bridge",
                arguments=[
                    "/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist",
                    "/model/rover_land_0/odometry"
                    "@nav_msgs/msg/Odometry[gz.msgs.Odometry",
                ],
                remappings=[
                    # Remap to model-scoped Gazebo topic so rover_drive_plugin
                    # receives the command on its subscribed topic name.
                    ("/cmd_vel", "/model/rover_land_0/cmd_vel"),
                ],
                output="screen",
            )
        ],
    )

    # ── Process 3: teleop_node (lifecycle) ───────────────────────────────────
    # Starts in 'unconfigured' state. Transition manually or use lifecycle_manager.
    # The bridge delay above ensures Gazebo is ready before the node starts publishing.
    teleop_node = TimerAction(
        period=3.0,
        actions=[
            LifecycleNode(
                package="rover_teleop",
                executable="teleop_node",
                name="teleop_node",
                namespace="",
                parameters=[LaunchConfiguration("params_file")],
                output="screen",
            )
        ],
    )

    return LaunchDescription([
        params_file_arg,
        sdf_path_arg,
        headless_arg,
        gz_server,
        gz_bridge,
        teleop_node,
    ])
