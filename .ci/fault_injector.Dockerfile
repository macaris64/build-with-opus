# .ci/fault_injector.Dockerfile — two-stage build for the fault_injector_run CLI.
#
# Stage 1 (builder): installs cmake + g++ and compiles fault_injector_run from
# the standalone simulation/fault_injector/ sub-project.  No Gazebo, no cFS,
# no Rust — pure C++17 + POSIX, builds in ~10 s.
#
# Stage 2 (runtime): copies only the compiled binary into a minimal Ubuntu
# 22.04 image so the final layer has no build tools (smaller attack surface).
#
# compose.yaml mounts simulation/scenarios/ read-only so the container can
# load scenario YAML files without embedding them at build time.

FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -q && \
    apt-get install -y --no-install-recommends \
        cmake g++ make ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy only the standalone fault_injector sub-project (no cFS / Gazebo deps).
COPY simulation/fault_injector/ .

RUN cmake -B /build \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        . \
    && cmake --build /build --target fault_injector_run

# ── Runtime stage ──────────────────────────────────────────────────────────────
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -q && \
    apt-get install -y --no-install-recommends \
        libstdc++6 libgcc-s1 bash ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/fault_injector_run /usr/local/bin/fault_injector_run

WORKDIR /app
