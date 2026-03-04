#!/usr/bin/env bash
set -euo pipefail

VER="${1-}"
if [[ -n "$VER" ]]; then
    SUFFIX="-$VER"
else
    SUFFIX=""
fi

NAME="build-linux-clang$SUFFIX"
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

NVCC="$CONDA/envs/cuda-env/bin/nvcc"
[[ -x "$NVCC" ]] && ENABLE_CUDA='True' || ENABLE_CUDA='False'

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_CUDA:BOOL="$ENABLE_CUDA" \
    -DCMAKE_C_COMPILER="clang$SUFFIX" \
    -DCMAKE_CXX_COMPILER="clang++$SUFFIX" \
    -DCMAKE_LINKER="ld.lld$SUFFIX" \
    -DCMAKE_CUDA_COMPILER="$NVCC"

ninja
