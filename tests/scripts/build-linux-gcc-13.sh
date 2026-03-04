#!/usr/bin/env bash

NAME='build-linux-gcc-13'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

NVCC="$CONDA/envs/cuda-env/bin/nvcc"
[[ -x "$NVCC" ]] && ENABLE_CUDA='True' || ENABLE_CUDA='False'

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA:BOOL="$ENABLE_CUDA" -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_CUDA_COMPILER="$NVCC"

ninja
