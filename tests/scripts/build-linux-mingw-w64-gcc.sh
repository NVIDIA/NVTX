#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

NAME='build-linux-mingw-w64-gcc'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

CMAKE_ARGS=(
    ..
    -G
    Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_TOOLCHAIN_FILE=../mingw-w64-x86_64.cmake
    -DENABLE_CUDA=False
)

cmake "${CMAKE_ARGS[@]}"

ninja
