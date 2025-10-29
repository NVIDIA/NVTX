#!/usr/bin/env zsh

NAME='build-macos-appleclang'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

FLAGS="-O3 -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c++17-attribute-extensions -Wno-variadic-macros -Wno-long-long -Wno-reserved-identifier -Wno-unsafe-buffer-usage -Wno-sign-conversion -Wno-float-equal -Wno-padded -Wno-cast-function-type-strict -Wno-covered-switch-default -Wno-atomic-implicit-seq-cst -Wno-exit-time-destructors -Wno-global-constructors -Wno-missing-variable-declarations -Wno-unknown-warning-option"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="$FLAGS -Werror" -DCMAKE_CXX_FLAGS="$FLAGS -Werror"

ninja
