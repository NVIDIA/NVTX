@echo off
pushd "%~dp0\.."

set NAME=build-windows-vs2022-arm64
cd %NAME%

call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" arm64 -vcvars_ver=14.44

"%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --output-on-failure
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

popd
