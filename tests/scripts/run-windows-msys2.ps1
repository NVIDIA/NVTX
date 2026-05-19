#!/usr/bin/env pwsh

# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

param(
    [Parameter(Mandatory)][string]$MSystem,
    [Parameter(Mandatory)][string]$Compiler
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$NAME = "build-windows-msys2-$($MSystem.ToLower())-$Compiler"

$env:MSYSTEM = $MSystem
$env:CHERE_INVOKING = "yes"

$WinTestsDir = (Resolve-Path "$PSScriptRoot\..").Path -replace '\\', '/'
$DriveLetter = $WinTestsDir.Substring(0, 1).ToLower()
$PosixTestsDir = "/$DriveLetter" + $WinTestsDir.Substring(2)

& "$env:MSYS64\usr\bin\bash" --login -c "cd '$PosixTestsDir/$NAME' && ctest --output-on-failure"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
