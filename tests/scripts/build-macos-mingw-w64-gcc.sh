#!/usr/bin/env zsh

NAME='build-macos-mingw-w64-gcc'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

FLAGS="-O3 -Wall -Wextra -Wmissing-braces -Wattributes -Wunused-result -Wlogical-op -Wcast-qual -Wduplicated-cond -Wnull-dereference -Wduplicated-branches -Warray-bounds=2 -Wpointer-arith -Wwrite-strings"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../mingw-w64-x86_64.cmake -DENABLE_CUDA=False -DCMAKE_C_FLAGS="$FLAGS -Wmissing-prototypes -Wstrict-prototypes -Werror" -DCMAKE_CXX_FLAGS="$FLAGS -Wextra-semi -Werror"

ninja
