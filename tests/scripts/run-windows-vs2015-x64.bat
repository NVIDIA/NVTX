@echo off
pushd "%~dp0\.."

set NAME=build-windows-vs2015-x64
cd %NAME%

call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.22621.0 -vcvars_ver=14.0

"%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --output-on-failure
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

popd
