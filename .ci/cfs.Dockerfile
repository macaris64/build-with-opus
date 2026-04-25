# .ci/cfs.Dockerfile — SAKURA-II cFS SITL runtime image.
#
# The cFS binary (core-cpu1) is pre-built on the host with:
#   cmake -B build_cfs -DSAKURA_CFS_RUNTIME=ON && cmake --build build_cfs
# and injected at runtime via the compose volume mount ./build_cfs:/app/build_cfs.
# This image provides only the Ubuntu 22.04 runtime libraries that core-cpu1
# links against — no cross-toolchain (Q-H8 deferred to Phase C+).
#
# Also serves as the base for the fault_injector SITL service.
# No secrets embedded — compose injects credentials via env vars / --env-file.

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -q && \
    apt-get install -y --no-install-recommends \
        libstdc++6 \
        libgcc-s1 \
        bash \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /app/build_cfs /tmp/cfs/log

WORKDIR /app
