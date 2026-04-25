FROM osrf/space-ros:latest

# colcon and pytest are pre-installed; AMENT_PREFIX_PATH, ROS_DISTRO, and
# SPACEROS_DIR are set by the base image — no extra configuration needed.
#
# Create /workspace with spaceros-user ownership so colcon can write build/
# log/ and install/ outputs there without a permission error.
USER root
RUN mkdir -p /workspace && chown spaceros-user:spaceros-user /workspace
USER spaceros-user

WORKDIR /workspace
