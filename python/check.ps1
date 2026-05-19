#!/usr/bin/env pwsh

# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

param(
    [string]$Python = "python3"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Exit-IfFailed {
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

& $Python -m pip install ".[test]" numpy
Exit-IfFailed

& $Python -m pytest tests
Exit-IfFailed

& $Python -m pytest --enable-injection tests
Exit-IfFailed

& $Python -m pip uninstall -y numpy
Exit-IfFailed

& $Python -m pytest tests
Exit-IfFailed

& $Python -m pytest --enable-injection tests
Exit-IfFailed
