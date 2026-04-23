FROM osrf/space-ros:humble-20240408

# Install colcon extensions and pytest for CI
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3-colcon-common-extensions \
    python3-pytest \
    && rm -rf /var/lib/apt/lists/*

# Workspace environment variables expected by colcon
ENV AMENT_PREFIX_PATH=/opt/ros/humble
ENV ROS_DISTRO=humble
ENV COLCON_HOME=/root/.colcon

WORKDIR /workspace
