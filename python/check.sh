#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -eux

[[ -z "${Python:-}" ]] && Python=python3

"$Python" -m pip install .[test] numpy
"$Python" -m pytest tests
"$Python" -m pytest --enable-injection tests
"$Python" -m pip uninstall -y numpy
"$Python" -m pytest tests
"$Python" -m pytest --enable-injection tests
