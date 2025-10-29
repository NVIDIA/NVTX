#!/usr/bin/env zsh

NAME='build-macos-gcc'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

GCC_VERSION="$(brew list --versions gcc | cut '-d ' -f2 | cut '-d.' -f1)"
FLAGS="-O3 -Wall -Wextra -Wmissing-braces -Wattributes -Wunused-result -Wlogical-op -Wcast-qual -Wduplicated-cond -Wnull-dereference -Wduplicated-branches -Warray-bounds=2 -Wpointer-arith -Wwrite-strings"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="gcc-$GCC_VERSION" -DCMAKE_CXX_COMPILER="g++-$GCC_VERSION" -DCMAKE_C_FLAGS="$FLAGS -Wmissing-prototypes -Wstrict-prototypes -Werror" -DCMAKE_CXX_FLAGS="$FLAGS -Wextra-semi -Werror"

ninja
