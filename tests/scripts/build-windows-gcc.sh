#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

NAME="$1"
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir -p "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

CMAKE_ARGS=(
    ..
    -G
    Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DENABLE_CUDA=False
    -DCMAKE_C_COMPILER=gcc
    -DCMAKE_CXX_COMPILER=g++
)

cmake "${CMAKE_ARGS[@]}"

ninja
