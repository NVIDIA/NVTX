$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest


Push-Location "$env:VSPATH\VC\Auxiliary\Build"

cmd /c "vcvarsall.bat x64_arm64 -vcvars_ver=14.44 & set" |
foreach {
  if ($_ -match "=") {
    $v = $_.split("=", 2); set-item -force -path "ENV:\$($v[0])" -value "$($v[1])"
  }
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Pop-Location


Push-Location "$PSScriptRoot\.."

$NAME = "build-windows-vs2022-x64_arm64"
New-Item -ItemType Directory -Force -Path $NAME | Out-Null
Set-Location $NAME

$FLAGS = "-Wall -wd4191 -wd4255 -wd4355 -wd4365 -wd4514 -wd4668 -wd4710 -wd4711 -wd4746 -wd4820 -wd5039 -wd5045 -wd5220 -WX -Zc:preprocessor"

& "$env:VSPATH\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA=False -DCMAKE_C_FLAGS="-O2 $FLAGS" -DCMAKE_CXX_FLAGS="-O2 $FLAGS"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& "$env:VSPATH\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Pop-Location

