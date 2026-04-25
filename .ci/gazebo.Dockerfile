# .ci/gazebo.Dockerfile — SAKURA-II Gazebo Harmonic headless SITL image.
#
# Runs gz sim in headless server mode (-s flag); no display required.
# Simulation world SDF and plugins are mounted read-only from ./simulation.
# No HPSC cross-toolchain (Q-H8 deferred to Phase C+).
# No secrets embedded.

FROM ghcr.io/gazebosim/gz-sim:harmonic

USER root

RUN apt-get update -q && \
    apt-get install -y --no-install-recommends \
        procps \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /app/simulation

# Plugin and world file discovery path for gz sim.
ENV GZ_SIM_RESOURCE_PATH=/app/simulation/worlds

WORKDIR /app
