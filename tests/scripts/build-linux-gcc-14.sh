#!/usr/bin/env bash

NAME='build-linux-gcc-14'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

FLAGS="-O3 -Wall -Wextra -Wmissing-braces -Wattributes -Wunused-result -Wlogical-op -Wcast-qual -Wduplicated-cond -Wnull-dereference -Wduplicated-branches -Warray-bounds=2 -Wpointer-arith -Wwrite-strings"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_CUDA_COMPILER="$CONDA/envs/cuda-env/bin/nvcc" -DCMAKE_C_FLAGS="$FLAGS -Wmissing-prototypes -Wstrict-prototypes -Werror" -DCMAKE_CXX_FLAGS="$FLAGS -Wextra-semi -Werror" -DCMAKE_CUDA_FLAGS="$FLAGS --Wno-deprecated-gpu-targets --Werror all-warnings"

ninja
