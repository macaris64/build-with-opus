FROM osrf/space-ros:latest

# colcon and pytest are pre-installed; AMENT_PREFIX_PATH, ROS_DISTRO, and
# SPACEROS_DIR are set by the base image — no extra configuration needed.
#
# Create /workspace with spaceros-user ownership so colcon can write build/
# log/ and install/ outputs there without a permission error.
USER root
RUN mkdir -p /workspace && chown spaceros-user:spaceros-user /workspace

# Install ros_gz_bridge and ros_gz_sim for Gazebo Harmonic ↔ ROS 2 topic bridging.
# The || true allows the build to proceed if the Space ROS apt overlay does not
# carry these packages — the colcon build in compose.yaml then builds them from
# source via the ros_gz_src clone below.
RUN apt-get update -q && \
    apt-get install -y --no-install-recommends \
        ros-${ROS_DISTRO}-ros-gz-bridge \
        ros-${ROS_DISTRO}-ros-gz-sim \
    && rm -rf /var/lib/apt/lists/* \
    || (rm -rf /var/lib/apt/lists/* && \
        git clone --depth 1 -b harmonic \
            https://github.com/gazebosim/ros_gz /workspace/ros_gz_src || true)

USER spaceros-user

WORKDIR /workspace
