#!/usr/bin/env zsh
set -euo pipefail

NAME='build-macos-mingw-w64-gcc'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=../mingw-w64-x86_64.cmake \
    -DENABLE_CUDA=False

ninja
