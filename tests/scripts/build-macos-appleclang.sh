#!/usr/bin/env zsh

# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

NAME='build-macos-appleclang'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

CMAKE_ARGS=(
    ..
    -G
    Ninja
    -DCMAKE_BUILD_TYPE=Release
)

cmake "${CMAKE_ARGS[@]}"

ninja
