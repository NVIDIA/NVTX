@echo off
pushd "%~dp0\.."

set NAME=build-windows-vs2015-x64
mkdir %NAME%
cd %NAME%

call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.22621.0 -vcvars_ver=14.0

set FLAGS=-Wall -wd4013 -wd4191 -wd4255 -wd4355 -wd4365 -wd4514 -wd4571 -wd4623 -wd4625 -wd4626 -wd4628 -wd4668 -wd4710 -wd4711 -wd4774 -wd4820 -wd5026 -wd5027 -wd5039 -wd5045 -wd5220 -WX

"%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA=False -DCMAKE_C_FLAGS="-O2 %FLAGS%" -DCMAKE_CXX_FLAGS="-O2 %FLAGS%"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

"%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

popd
