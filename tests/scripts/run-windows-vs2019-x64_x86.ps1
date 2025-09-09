$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest


Push-Location "$env:VSPATH\VC\Auxiliary\Build"

cmd /c "vcvarsall.bat x64_x86 -vcvars_ver=14.29 & set" |
foreach {
  if ($_ -match "=") {
    $v = $_.split("=", 2); set-item -force -path "ENV:\$($v[0])" -value "$($v[1])"
  }
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Pop-Location


Push-Location "$PSScriptRoot\.."

$NAME = "build-windows-vs2019-x64_x86"
Set-Location $NAME

& "$env:VSPATH\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Pop-Location

