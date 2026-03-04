@echo off
pushd "%~dp0\.."

set NAME=build-windows-vs2017-x64
mkdir %NAME%
cd %NAME%

call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.16

set ENABLE_CUDA=False
set NVCC=%CONDA%\envs\cuda-12-9-env\Library\bin\nvcc.exe
if exist "%NVCC%" set ENABLE_CUDA=True

"%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA:BOOL="%ENABLE_CUDA%" -DCMAKE_CUDA_COMPILER="%NVCC:\=/%"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

"%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

popd
