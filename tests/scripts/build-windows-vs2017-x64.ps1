$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest


Push-Location "$env:VSPATH\VC\Auxiliary\Build"

cmd /c "vcvarsall.bat x64 -vcvars_ver=14.16 & set" |
foreach {
  if ($_ -match "=") {
    $v = $_.split("=", 2); set-item -force -path "ENV:\$($v[0])" -value "$($v[1])"
  }
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Pop-Location


Push-Location "$PSScriptRoot\.."

$NAME = "build-windows-vs2017-x64"
New-Item -ItemType Directory -Force -Path $NAME | Out-Null
Set-Location $NAME

$ENABLE_CUDA = "False"
$NVCC = "$env:CONDA\envs\cuda-12-9-env\Library\bin\nvcc.exe"
if (Test-Path $NVCC) {
    $ENABLE_CUDA = "True"
}
$NVCC_FORWARD = $NVCC -replace '\\', '/'

& "$env:VSPATH\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA:BOOL="$ENABLE_CUDA" -DCMAKE_CUDA_COMPILER="$NVCC_FORWARD"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& "$env:VSPATH\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Pop-Location

