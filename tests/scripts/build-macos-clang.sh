#!/usr/bin/env zsh

NAME='build-macos-clang'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

HOMEBREWPREFIX="$(brew --prefix)"
export PATH="$HOMEBREWPREFIX/opt/llvm/bin:$PATH"
export LDFLAGS="-L$HOMEBREWPREFIX/opt/llvm/lib"
export CPPFLAGS="-I$HOMEBREWPREFIX/opt/llvm/include"
FLAGS="-O3 -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c++17-attribute-extensions -Wno-variadic-macros -Wno-long-long -Wno-reserved-identifier -Wno-unsafe-buffer-usage -Wno-sign-conversion -Wno-float-equal -Wno-padded -Wno-cast-function-type-strict -Wno-covered-switch-default -Wno-atomic-implicit-seq-cst -Wno-exit-time-destructors -Wno-global-constructors -Wno-missing-variable-declarations -Wno-c++-keyword"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$HOMEBREWPREFIX/opt/llvm/bin/clang" -DCMAKE_CXX_COMPILER="$HOMEBREWPREFIX/opt/llvm/bin/clang++" -DCMAKE_LINKER="ld64.lld" -DCMAKE_C_FLAGS="$FLAGS -Werror" -DCMAKE_CXX_FLAGS="$FLAGS -Werror"

ninja
