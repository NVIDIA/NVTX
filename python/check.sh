#!/usr/bin/env bash

set -eux

[[ -z "${Python:-}" ]] && Python=python3

"$Python" -m pip install .[test]
"$Python" -m pytest tests
"$Python" -m pytest --enable-injection tests
