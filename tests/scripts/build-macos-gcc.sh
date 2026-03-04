#!/usr/bin/env zsh
set -euo pipefail

NAME='build-macos-gcc'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

GCC_VERSION="$(brew list --versions gcc | cut '-d ' -f2 | cut '-d.' -f1)"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="gcc-$GCC_VERSION" \
    -DCMAKE_CXX_COMPILER="g++-$GCC_VERSION"

ninja
