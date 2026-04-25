FROM ubuntu:22.04

# cFS runtime build environment for SAKURA-II CI.
# Installs the minimal toolchain needed to build cFE + OSAL + PSP + apps
# with SAKURA_CFS_RUNTIME=ON.
#
# Usage (from repo root):
#   docker build -f .ci/cfs-runtime.Dockerfile -t sakura-ci/cfs-runtime:local .
#   docker run --rm -v "$(pwd):/workspace" sakura-ci/cfs-runtime:local bash /workspace/scripts/ci-cfs-runtime.sh

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -q && \
    apt-get install -y --no-install-recommends \
        cmake \
        ninja-build \
        gcc \
        g++ \
        git \
        make \
        libcmocka-dev \
        python3 \
        python3-pip \
        curl \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
