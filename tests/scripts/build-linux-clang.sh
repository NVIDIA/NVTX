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

NAME="build-linux-clang$SUFFIX"
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

NVCC="$CONDA/envs/cuda-env/bin/nvcc"
CUDA_MAX_CLANG_VER=21
if [[ ! "$VER" =~ ^[0-9]+$ || "$VER" -gt "$CUDA_MAX_CLANG_VER" ]]; then
    echo "CUDA disabled: Clang version '$VER' is not supported; CUDA requires Clang <= $CUDA_MAX_CLANG_VER."
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
    -DCMAKE_C_COMPILER="clang$SUFFIX"
    -DCMAKE_CXX_COMPILER="clang++$SUFFIX"
    -DCMAKE_LINKER="ld.lld$SUFFIX"
    -DCMAKE_CUDA_COMPILER="$NVCC"
)

cmake "${CMAKE_ARGS[@]}"

ninja
