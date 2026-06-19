#!/usr/bin/env pwsh

# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

param(
    [Parameter(Mandatory)][string]$VsYear
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Arch = "x64"

$VcVarsVer = switch ($VsYear) {
    "2015" { "14.0" }
    "2017" { "14.16" }
    "2019" { "14.29" }
    "2022" { "14.44" }
    "2026" { "14.51" }
    default { throw "Unknown VS year: $VsYear" }
}

Push-Location "$env:VSPATH\VC\Auxiliary\Build"

cmd /c "vcvarsall.bat $Arch -vcvars_ver=$VcVarsVer & set" |
foreach {
  if ($_ -match "=") {
    $v = $_.split("=", 2); set-item -force -path "ENV:\$($v[0])" -value "$($v[1])"
  }
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Pop-Location


Push-Location "$PSScriptRoot\.."

$NAME = "build-windows-vs$VsYear-$Arch-clang-cl"
Set-Location $NAME

& "$env:VSPATH\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Pop-Location
