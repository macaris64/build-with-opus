"""
rover.launch.py — Top-level launch file for the rover software stack.

Launches the teleop_node as a lifecycle node. Parameters are loaded from
rover_params.yaml to keep all tunable values out of source code.
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import LifecycleNode
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    params_file_arg = DeclareLaunchArgument(
        "params_file",
        default_value=PathJoinSubstitution(
            [FindPackageShare("rover_bringup"), "config", "rover_params.yaml"]
        ),
        description="Path to the rover parameter file",
    )

    teleop_node = LifecycleNode(
        package="rover_teleop",
        executable="teleop_node",
        name="teleop_node",
        namespace="",
        parameters=[LaunchConfiguration("params_file")],
        output="screen",
    )

    return LaunchDescription([
        params_file_arg,
        teleop_node,
    ])
