#!/usr/bin/env zsh

# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

NAME='build-macos-clang'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

HOMEBREWPREFIX="$(brew --prefix)"
export PATH="$HOMEBREWPREFIX/opt/llvm/bin:$PATH"
export LDFLAGS="-L$HOMEBREWPREFIX/opt/llvm/lib"
export CPPFLAGS="-I$HOMEBREWPREFIX/opt/llvm/include"

CMAKE_ARGS=(
    ..
    -G
    Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_C_COMPILER="$HOMEBREWPREFIX/opt/llvm/bin/clang"
    -DCMAKE_CXX_COMPILER="$HOMEBREWPREFIX/opt/llvm/bin/clang++"
    -DCMAKE_LINKER="ld64.lld"
)

cmake "${CMAKE_ARGS[@]}"

ninja
