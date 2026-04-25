# .ci/ground_station.Dockerfile — SAKURA-II Rust ground station SITL image.
#
# Pre-fetches crate dependencies so the first `cargo build` in the compose
# command hits the layer cache rather than the network.
# Q-C8: ccsds_wire and cfs_bindings are the sole BE-encoding loci; no
#       from_le_bytes / to_le_bytes in ground_station source.
# Q-F3: no new Vault<T> sites — time_suspect_seen is SITL-transient state.
# No secrets embedded — compose injects env vars via --env-file .env.

FROM rust:1.77-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -q && \
    apt-get install -y --no-install-recommends \
        pkg-config \
        libssl-dev \
        iproute2 \
        curl \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy manifests and toolchain pin first for layer caching.
COPY rust-toolchain.toml .
COPY Cargo.toml Cargo.lock ./
COPY .cargo/ .cargo/

# Copy workspace member source trees.
COPY rust/ccsds_wire/ rust/ccsds_wire/
COPY rust/cfs_bindings/ rust/cfs_bindings/
COPY rust/ground_station/ rust/ground_station/
COPY _defs/ _defs/

# Pre-populate the dependency registry so `cargo build` in the compose
# command doesn't re-fetch from the network on every container restart.
RUN cargo fetch --locked
