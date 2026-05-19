#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

NAME='build-linux-aocc'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

NVCC="$CONDA/envs/cuda-env/bin/nvcc"
[[ -x "$NVCC" ]] && ENABLE_CUDA='True' || ENABLE_CUDA='False'

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_CUDA:BOOL="$ENABLE_CUDA" \
    -DCMAKE_C_COMPILER="$AOCC_PATH/clang" \
    -DCMAKE_CXX_COMPILER="$AOCC_PATH/clang++" \
    -DCMAKE_LINKER="$AOCC_PATH/ld.lld" \
    -DCMAKE_CUDA_COMPILER="$NVCC"

ninja
