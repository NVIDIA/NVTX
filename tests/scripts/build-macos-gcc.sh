#!/usr/bin/env zsh

# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

NAME='build-macos-gcc'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

GCC_VERSION="$(brew list --versions gcc | cut '-d ' -f2 | cut '-d.' -f1)"

CMAKE_ARGS=(
    ..
    -G
    Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_C_COMPILER="gcc-$GCC_VERSION"
    -DCMAKE_CXX_COMPILER="g++-$GCC_VERSION"
)

cmake "${CMAKE_ARGS[@]}"

ninja
