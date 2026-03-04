#!/usr/bin/env zsh

NAME='build-macos-clang'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

HOMEBREWPREFIX="$(brew --prefix)"
export PATH="$HOMEBREWPREFIX/opt/llvm/bin:$PATH"
export LDFLAGS="-L$HOMEBREWPREFIX/opt/llvm/lib"
export CPPFLAGS="-I$HOMEBREWPREFIX/opt/llvm/include"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$HOMEBREWPREFIX/opt/llvm/bin/clang" -DCMAKE_CXX_COMPILER="$HOMEBREWPREFIX/opt/llvm/bin/clang++" -DCMAKE_LINKER="ld64.lld"

ninja
