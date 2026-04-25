# .ci/gazebo.Dockerfile — SAKURA-II Gazebo Harmonic headless SITL image.
#
# Builds from ubuntu:22.04 + OSRF apt repository because
# ghcr.io/gazebosim/gz-sim:harmonic requires authentication.
# Runs gz sim in headless server mode (-s flag); no display required.
# Simulation world SDF and plugins are mounted read-only from ./simulation.
# No HPSC cross-toolchain (Q-H8 deferred to Phase C+).
# No secrets embedded.

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

# Add OSRF Gazebo Harmonic apt repository.
RUN curl -fsSL https://packages.osrfoundation.org/gazebo.gpg \
        -o /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg && \
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] \
        http://packages.osrfoundation.org/gazebo/ubuntu-stable $(lsb_release -cs) main" \
        > /etc/apt/sources.list.d/gazebo-stable.list && \
    apt-get update -q && \
    apt-get install -y --no-install-recommends \
        gz-sim8 \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /app/simulation

# Plugin and world file discovery path for gz sim.
ENV GZ_SIM_RESOURCE_PATH=/app/simulation/worlds

WORKDIR /app
