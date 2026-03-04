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
    "2026" { "14.50" }
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


Push-Location "$PSScriptRoot\.."

$NAME = "build-windows-vs$VsYear-$Arch"
Set-Location $NAME

& "$env:VSPATH\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Pop-Location
