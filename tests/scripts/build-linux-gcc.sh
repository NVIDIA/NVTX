#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

VER="${1-}"
if [[ -n "$VER" ]]; then
    SUFFIX="-$VER"
else
    SUFFIX=""
fi

NAME="build-linux-gcc$SUFFIX"
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir -p "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

if ! NVCC="$(command -v nvcc 2>/dev/null)"; then
    NVCC="${CONDA:-}/envs/cuda-env/bin/nvcc"
fi
CUDA_MAX_GCC_VER=15
if [[ ! "$VER" =~ ^[0-9]+$ || "$VER" -gt "$CUDA_MAX_GCC_VER" ]]; then
    echo "CUDA disabled: GCC version '$VER' is not supported; CUDA requires GCC <= $CUDA_MAX_GCC_VER."
    ENABLE_CUDA='False'
elif [[ -x "$NVCC" ]]; then
    ENABLE_CUDA='True'
else
    ENABLE_CUDA='False'
fi

CMAKE_ARGS=(
    ..
    -G
    Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DENABLE_CUDA:BOOL="$ENABLE_CUDA"
    -DCMAKE_C_COMPILER="gcc$SUFFIX"
    -DCMAKE_CXX_COMPILER="g++$SUFFIX"
    -DCMAKE_CUDA_COMPILER="$NVCC"
)

cmake "${CMAKE_ARGS[@]}"

ninja
