#!/usr/bin/env pwsh

# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

param(
    [Parameter(Mandatory)][string]$VsYear,
    [Parameter(Mandatory)][string]$Arch
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$VcVarsVer = switch ($VsYear) {
    "2015" { "14.0" }
    "2017" { "14.16" }
    "2019" { "14.29" }
    "2022" { "14.44" }
    "2026" { "14.51" }
    default { throw "Unknown VS year: $VsYear" }
}

$SdkArg = if ($VsYear -eq "2015" -or ($VsYear -eq "2019" -and $Arch -eq "x64_arm64")) { "10.0.22621.0 " } else { "" }

Push-Location "$env:VSPATH\VC\Auxiliary\Build"

cmd /c "vcvarsall.bat $Arch ${SdkArg}-vcvars_ver=$VcVarsVer & set" |
foreach {
  if ($_ -match "=") {
    $v = $_.split("=", 2); set-item -force -path "ENV:\$($v[0])" -value "$($v[1])"
  }
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Pop-Location

if ($null -eq (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw "cl.exe not found in PATH after running vcvarsall.bat for '$Arch'."
}

Push-Location "$PSScriptRoot\.."

$NAME = "build-windows-vs$VsYear-$Arch"
New-Item -ItemType Directory -Force -Path $NAME | Out-Null
Set-Location $NAME

$CudaArgs = @("-DENABLE_CUDA=False")
$VsYearInt = [int]$VsYear
$MinCudaVsYear = 2017
$MaxCudaVsYear = 2026
if ($Arch -eq "x64") {
    if ($VsYearInt -lt $MinCudaVsYear -or $VsYearInt -gt $MaxCudaVsYear) {
        Write-Host "CUDA disabled: Visual Studio $VsYear is not supported; CUDA requires VS >= $MinCudaVsYear and <= $MaxCudaVsYear."
    } else {
        $CondaEnv = switch ($VsYear) {
            "2017" { "cuda-12-9-env" }
            default { "cuda-env" }
        }
        $NVCC = "$env:CONDA\envs\$CondaEnv\Library\bin\nvcc.exe"
        if (Test-Path $NVCC) {
            $NvccForward = $NVCC -replace '\\', '/'
            $CudaArgs = @("-DENABLE_CUDA:BOOL=True", "-DCMAKE_CUDA_COMPILER=$NvccForward")
        }
    }
}

$CMakeArgs = @(
    "..",
    "-G",
    "Ninja",
    "-DCMAKE_BUILD_TYPE=Release"
) + $CudaArgs

& "$env:VSPATH\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" @CMakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& "$env:VSPATH\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Pop-Location
