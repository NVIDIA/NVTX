#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

NAME='build-linux-nvhpc'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$NVHPC_PATH/nvc" \
    -DCMAKE_CXX_COMPILER="$NVHPC_PATH/nvc++" \
    -DCMAKE_CUDA_COMPILER="$NVHPC_PATH/nvcc"

ninja
