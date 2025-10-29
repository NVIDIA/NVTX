@echo off
pushd "%~dp0\.."

set NAME=build-windows-vs2022-x64-clang-cl
mkdir %NAME%
cd %NAME%

call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64

"%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA=False -DCMAKE_C_COMPILER="%VSPATH:\=/%/VC/Tools/Llvm/x64/bin/clang-cl.exe" -DCMAKE_CXX_COMPILER="%VSPATH:\=/%/VC/Tools/Llvm/x64/bin/clang-cl.exe" -DCMAKE_LINKER="%VSPATH:\=/%/VC/Tools/Llvm/x64/bin/lld-link.exe"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

"%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

popd
