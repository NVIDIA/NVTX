#!/usr/bin/env zsh
set -euo pipefail

NAME='build-macos-appleclang'
LOCATION="$(cd "$(dirname "$0")/.." ; pwd)"
mkdir "$LOCATION/$NAME"
cd "$LOCATION/$NAME"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

ninja
