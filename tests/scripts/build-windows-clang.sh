#!/usr/bin/env bash
set -euo pipefail

NAME="$1"
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir -p "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_CUDA=False \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_LINKER=ld.lld

ninja
