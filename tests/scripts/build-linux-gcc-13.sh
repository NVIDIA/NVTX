#!/usr/bin/env bash

NAME='build-linux-gcc-13'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

NVCC="$CONDA/envs/cuda-env/bin/nvcc"
[[ -x "$NVCC" ]] && ENABLE_CUDA='True' || ENABLE_CUDA='False'

FLAGS="-O3 -Wall -Wextra -Wmissing-braces -Wattributes -Wunused-result -Wlogical-op -Wcast-qual -Wduplicated-cond -Wnull-dereference -Wduplicated-branches -Warray-bounds=2 -Wpointer-arith -Wwrite-strings"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA:BOOL="$ENABLE_CUDA" -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_CUDA_COMPILER="$NVCC" -DCMAKE_C_FLAGS="$FLAGS -Wmissing-prototypes -Wstrict-prototypes -Werror" -DCMAKE_CXX_FLAGS="$FLAGS -Wextra-semi -Werror" -DCMAKE_CUDA_FLAGS="$FLAGS --Wno-deprecated-gpu-targets --Werror all-warnings"

ninja
