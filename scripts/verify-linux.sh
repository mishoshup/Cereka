#!/usr/bin/env bash
# scripts/verify-linux.sh — Clean-room Linux verification via Docker.
#
# This script spawns an Ubuntu container, installs all build dependencies,
# and performs a full build and test cycle.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="ubuntu:22.04"

echo "─── Starting Linux Verification (Docker: $IMAGE) ───"

# Ensure we have the latest image
docker pull "$IMAGE"

# Run the verification inside Docker
# We mount the project root as /src and run the commands
docker run --rm \
    -v "$ROOT:/src" \
    -w /src \
    "$IMAGE" \
    /bin/bash -c "
        set -e
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get install -y \
            build-essential \
            cmake \
            ninja-build \
            libasound2-dev \
            libpulse-dev \
            libaudio-dev \
            libjack-dev \
            libsndio-dev \
            libx11-dev \
            libxext-dev \
            libxrandr-dev \
            libxcursor-dev \
            libxfixes-dev \
            libxi-dev \
            libxss-dev \
            libxkbcommon-dev \
            libwayland-dev \
            libegl1-mesa-dev \
            libgl1-mesa-dev \
            libgles2-mesa-dev \
            libdbus-1-dev \
            libibus-1.0-dev \
            libudev-dev \
            libpipewire-0.3-dev \
            libdecor-0-dev

        echo '─── Configuring CMake ───'
        cmake -B build-verify-linux -S . -G Ninja -DCMAKE_BUILD_TYPE=Release

        echo '─── Building ───'
        cmake --build build-verify-linux

        echo '─── Running Tests ───'
        cd build-verify-linux
        ctest --output-on-failure
    "

echo "─── Verification Successful ───"
