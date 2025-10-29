@echo off
pushd "%~dp0\.."

set NAME=build-windows-vs2022-x64
mkdir %NAME%
cd %NAME%

call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64

set FLAGS=-Wall -wd4191 -wd4255 -wd4355 -wd4365 -wd4514 -wd4668 -wd4710 -wd4711 -wd4820 -wd5039 -wd5045 -wd5220 -WX -Zc:preprocessor

"%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_COMPILER="%CONDA:\=/%/envs/cuda-env/Library/bin/nvcc.exe" -DCMAKE_C_FLAGS="-O2 %FLAGS%" -DCMAKE_CXX_FLAGS="-O2 %FLAGS%" -DCMAKE_CUDA_FLAGS="%FLAGS% -wd4555 --Wno-deprecated-gpu-targets --Werror all-warnings"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

"%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

popd
