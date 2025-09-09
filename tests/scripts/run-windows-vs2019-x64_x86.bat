@echo off
pushd "%~dp0\.."

set NAME=build-windows-vs2019-x64_x86
cd %NAME%

call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64_x86 -vcvars_ver=14.29

"%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --output-on-failure
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

popd
