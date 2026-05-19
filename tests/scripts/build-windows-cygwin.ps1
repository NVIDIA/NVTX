#!/usr/bin/env pwsh

# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

param(
    [Parameter(Mandatory)][string]$Arch,
    [Parameter(Mandatory)][string]$Compiler
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$CygBits = switch ($Arch) {
    "x86" { "32" }
    "x64" { "64" }
    default { throw "Unknown arch: $Arch" }
}
$CygRoot = if ($CygBits -eq "32") { $env:CYGWIN32 } else { $env:CYGWIN64 }
$NAME = "build-windows-cygwin$CygBits-$Compiler"

$WinPath = $PSScriptRoot -replace '\\', '/'
$DriveLetter = $WinPath.Substring(0, 1).ToLower()
$PosixScript = "/cygdrive/$DriveLetter" + $WinPath.Substring(2) + "/build-windows-$Compiler.sh"

& "$CygRoot\bin\bash" --login $PosixScript $NAME
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
