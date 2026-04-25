# .ci/gazebo.Dockerfile — SAKURA-II Gazebo Harmonic SITL image (GUI-capable).
#
# Two-stage build:
#   Stage 1 (builder): installs Gazebo dev headers, compiles all 4 system
#     plugins into shared libraries.
#   Stage 2 (runtime): installs Gazebo runtime + X11/Mesa GUI packages, copies
#     .so files from builder.  Final image is ~400 MB smaller than single-stage.
#
# GUI support: the runtime stage includes Mesa llvmpipe (software renderer) and
# X11 client libraries so gz sim can open a window when $DISPLAY is forwarded
# from the host via /tmp/.X11-unix (see compose.yaml gazebo service).
# LIBGL_ALWAYS_SOFTWARE=1 forces Mesa llvmpipe; no GPU passthrough required.
#
# Plugin discovery: GZ_SIM_SYSTEM_PLUGIN_PATH=/usr/local/lib (Harmonic API).
# World file discovery: GZ_SIM_RESOURCE_PATH=/app/simulation/worlds.
# No HPSC cross-toolchain (Q-H8 deferred to Phase C+).
# No secrets embedded.

# ── Stage 1: builder ──────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -q && \
    apt-get install -y --no-install-recommends \
        curl \
        gnupg \
        lsb-release \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Add OSRF Gazebo Harmonic apt repository.
RUN curl -fsSL https://packages.osrfoundation.org/gazebo.gpg \
        -o /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg && \
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] http://packages.osrfoundation.org/gazebo/ubuntu-stable $(lsb_release -cs) main" \
        > /etc/apt/sources.list.d/gazebo-stable.list && \
    apt-get update -q && \
    apt-get install -y --no-install-recommends \
        gz-harmonic \
        libgz-sim8-dev \
        libgz-transport13-dev \
        libgz-plugin2-dev \
        libgz-math7-dev \
        cmake \
        g++ \
        make \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy plugin source trees into the builder.
COPY simulation/gazebo_rover_plugin   /src/gazebo_rover_plugin
COPY simulation/gazebo_uav_plugin     /src/gazebo_uav_plugin
COPY simulation/gazebo_cryobot_plugin /src/gazebo_cryobot_plugin
COPY simulation/gazebo_world_plugin   /src/gazebo_world_plugin

# Build and install each plugin.  Unit-test targets (uav_plugin_test, etc.)
# have no Gazebo dependency and are skipped here (BUILD_TESTING=OFF).
RUN cmake -B /build/rover \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_TESTING=OFF \
        /src/gazebo_rover_plugin && \
    cmake --build /build/rover && \
    cmake --install /build/rover

RUN cmake -B /build/uav \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_TESTING=OFF \
        /src/gazebo_uav_plugin && \
    cmake --build /build/uav && \
    cmake --install /build/uav

RUN cmake -B /build/cryobot \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_TESTING=OFF \
        /src/gazebo_cryobot_plugin && \
    cmake --build /build/cryobot && \
    cmake --install /build/cryobot

RUN cmake -B /build/world \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_TESTING=OFF \
        /src/gazebo_world_plugin && \
    cmake --build /build/world && \
    cmake --install /build/world

# ── Stage 2: runtime ─────────────────────────────────────────────────────────
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -q && \
    apt-get install -y --no-install-recommends \
        curl \
        gnupg \
        lsb-release \
        ca-certificates \
        procps \
    && rm -rf /var/lib/apt/lists/*

# Add OSRF Gazebo Harmonic apt repository (runtime packages only).
RUN curl -fsSL https://packages.osrfoundation.org/gazebo.gpg \
        -o /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg && \
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] http://packages.osrfoundation.org/gazebo/ubuntu-stable $(lsb_release -cs) main" \
        > /etc/apt/sources.list.d/gazebo-stable.list && \
    apt-get update -q && \
    apt-get install -y --no-install-recommends \
        gz-harmonic \
        # Mesa llvmpipe software renderer — GPU-free OpenGL for Docker containers.
        libgl1-mesa-dri \
        libgl1 \
        libegl1 \
        libglu1-mesa \
        # X11 client libraries for forwarding the GUI window to the host display.
        libx11-6 \
        libx11-xcb1 \
        libxrender1 \
        libxext6 \
        libxcb1 \
    && rm -rf /var/lib/apt/lists/*

# Copy compiled plugin .so files from the builder stage.
COPY --from=builder /usr/local/lib/librover_drive_plugin.so       /usr/local/lib/
COPY --from=builder /usr/local/lib/libuav_flight_plugin.so        /usr/local/lib/
COPY --from=builder /usr/local/lib/libcryobot_physics_plugin.so   /usr/local/lib/
COPY --from=builder /usr/local/lib/libworld_environment_plugin.so /usr/local/lib/

RUN ldconfig

RUN mkdir -p /app/simulation

# GZ_SIM_RESOURCE_PATH: world .sdf and mesh files mounted from ./simulation
# GZ_SIM_SYSTEM_PLUGIN_PATH: where gz-sim scans for compiled system plugins
ENV GZ_SIM_RESOURCE_PATH=/app/simulation/worlds
ENV GZ_SIM_SYSTEM_PLUGIN_PATH=/usr/local/lib

WORKDIR /app
