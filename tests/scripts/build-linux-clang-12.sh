#!/usr/bin/env bash

NAME='build-linux-clang-12'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

FLAGS="-O3 -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c++17-extensions -Wno-variadic-macros -Wno-long-long -Wno-reserved-id-macro -Wno-sign-conversion -Wno-float-equal -Wno-padded -Wno-covered-switch-default -Wno-atomic-implicit-seq-cst -Wno-exit-time-destructors -Wno-global-constructors -Wno-missing-variable-declarations -Wno-documentation-unknown-command"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-12 -DCMAKE_CXX_COMPILER=clang++-12 -DCMAKE_LINKER=ld.lld-12 -DCMAKE_CUDA_COMPILER="$CONDA/envs/cuda-env/bin/nvcc" -DCMAKE_C_FLAGS="$FLAGS -Werror" -DCMAKE_CXX_FLAGS="$FLAGS -Werror" -DCMAKE_CUDA_FLAGS="$FLAGS -Wno-undef -Wno-newline-eof -Wno-old-style-cast -Wno-missing-noreturn -Wno-deprecated-dynamic-exception-spec -Wno-unused-template -Wno-zero-as-null-pointer-constant -Wno-used-but-marked-unused -Wno-extra-semi-stmt --Wno-deprecated-gpu-targets -Wno-disabled-macro-expansion -Wno-duplicate-enum -Wno-unused-function --Werror all-warnings"

ninja
