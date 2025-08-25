#!/usr/bin/env bash

NAME='build-linux-nvhpc'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

FLAGS="-O4 -Wall"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$NVHPC_PATH/nvc" -DCMAKE_CXX_COMPILER="$NVHPC_PATH/nvc++" -DCMAKE_CUDA_COMPILER="$NVHPC_PATH/nvcc" -DCMAKE_C_FLAGS="$FLAGS -Werror" -DCMAKE_CXX_FLAGS="$FLAGS -Werror" -DCMAKE_CUDA_FLAGS="$FLAGS --Wno-deprecated-gpu-targets --Werror all-warnings"

ninja
