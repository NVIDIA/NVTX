# SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://nvidia.github.io/NVTX/LICENSE.txt for license information.

cmake_minimum_required (VERSION 3.19)

if(NOT DEFINED ENABLE_CUDA)
    set(ENABLE_CUDA True)
endif()
if(APPLE)
    set(ENABLE_CUDA False)
endif()

# Enforce standard C/C++ with sensible warnings and minimal compiler output on all platforms
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NVTX_LANGUAGES C CXX)
if(ENABLE_CUDA)
    set(CMAKE_CUDA_HOST_COMPILER "${CMAKE_CXX_COMPILER}")
    set(NVTX_LANGUAGES ${NVTX_LANGUAGES} CUDA)
endif()

project ("NvtxTests" LANGUAGES ${NVTX_LANGUAGES})

string(REGEX REPLACE "/tests$" "" NVTX3_PROJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(NVTX3_C_ROOT "${NVTX3_PROJ_ROOT}/c")

if(MSVC)
    # Must use - instead of / for option prefix when using NVCC, because it forwards args
    # it doesn't know to the host compiler, but being a UNIX-heritage program, it thinks
    # an argument like "/nologo" is an input file path.  Luckily, MSVC accepts - prefixes.
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS "19.14.0.0")
        # Enable options to behave closer to standard
    else()
        add_compile_options(-permissive-)
    endif()
    # The following line can be uncommented to test with WIN32_LEAN_AND_MEAN
    #add_compile_definitions(WIN32_LEAN_AND_MEAN)
endif()

# Build with minimal or no dependencies on installed C/C++ runtime libraries
if(MSVC)
    # For Non-debug, change /MD (MultiThreadedDLL) to /MT (MultiThreaded)
    # For Debug, change /MDd (MultiThreadedDebugDLL) to /MTd ((MultiThreadedDebug)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    # Statically link libstdc++ and libgcc.  Do not statically link libc, though.
    # Use an old sysroot if compatibility with old GLIBC versions is required.
    # In non-DEBUG builds, use `-s` (or `-x -S`) to strip unneeded symbols
    add_link_options(
        $<$<CXX_COMPILER_ID:GNU>:-static-libstdc++>
        $<$<CXX_COMPILER_ID:GNU>:-static-libgcc>
        $<$<AND:$<CONFIG:Release,MinSizeRel>,$<PLATFORM_ID:Darwin>>:-Wl,-x,-S>
        $<$<AND:$<CONFIG:Release,MinSizeRel>,$<NOT:$<PLATFORM_ID:Darwin>>>:-Wl,-s>
    )
endif()

# Compiler-specific idiosyncracies
if(MSVC)
    # Must use - instead of / for option prefix when using NVCC, because it forwards args
    # it doesn't know to the host compiler, but being a UNIX-heritage program, it thinks
    # an argument like "/nologo" is an input file path.  Luckily, MSVC accepts - prefixes.
    add_compile_options(-nologo)
    #add_compile_options(-wd26812) # Disable warning: prefer enum class over unscoped enum
    add_link_options(-NOLOGO -INCREMENTAL:NO)
    # On some platforms, CMake doesn't automatically add C++ flags to enable RTTI (/GR) or
    # configure C++ exceptions to the commonly preferred value (/EHsc or /GX).  Add these
    # if they are missing.
    if(NOT CMAKE_CXX_FLAGS MATCHES "(/|-)GR( |$)")
        string(APPEND CMAKE_CXX_FLAGS " -GR")
    endif()
    if(NOT CMAKE_CXX_FLAGS MATCHES "(/|-)(EHsc|GX)( |$)")
        string(APPEND CMAKE_CXX_FLAGS " -EHsc")
    endif()
    # Improve debugging
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        # This for some reason also adds "MDd" even though above we asked for MTd,
        # so add the /JMC option manually
        #set(CMAKE_VS_JUST_MY_CODE_DEBUGGING ON)
        add_compile_options(-JMC)
    endif()
endif()


add_subdirectory("../c" "ImportNvtx")

#if(DOMAINS_ERROR_TEST_NAME_IS_MISSING)
#    target_compile_definitions(domains PRIVATE ERROR_TEST_NAME_IS_MISSING)
#endif()

add_executable(runtest "RunTest.cpp")
target_link_libraries(runtest PRIVATE nvtx3-cpp)

add_library(inj-c SHARED "PrintInjectionC.c" "PrintInjectionImpl.cpp")
target_compile_definitions(inj-c PRIVATE SUPPORT_STATIC_INJECTION SUPPORT_DYNAMIC_INJECTION SUPPORT_PREINJECTION INJECTION_PRINT_PREFIX="inj-c")
target_link_libraries(inj-c PRIVATE nvtx3-c)
set_target_properties(inj-c PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(inj-cpp SHARED "PrintInjectionCpp.cpp" "PrintInjectionImpl.cpp")
target_compile_definitions(inj-cpp PRIVATE SUPPORT_STATIC_INJECTION SUPPORT_DYNAMIC_INJECTION SUPPORT_PREINJECTION INJECTION_PRINT_PREFIX="inj-cpp")
target_link_libraries(inj-cpp PRIVATE nvtx3-cpp)
set_target_properties(inj-cpp PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(inj-c-static STATIC "PrintInjectionC.c" "PrintInjectionImpl.cpp")
if(NOT MSVC)
    target_compile_options(inj-c-static PRIVATE -fPIC)
endif()
target_compile_definitions(inj-c-static PRIVATE SUPPORT_STATIC_INJECTION INJECTION_PRINT_PREFIX="inj-c-static")
target_link_libraries(inj-c-static PRIVATE nvtx3-c)
set_target_properties(inj-c-static PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(inj-cpp-static STATIC "PrintInjectionCpp.cpp" "PrintInjectionImpl.cpp")
if(NOT MSVC)
    target_compile_options(inj-cpp-static PRIVATE -fPIC)
endif()
target_compile_definitions(inj-cpp-static PRIVATE SUPPORT_STATIC_INJECTION INJECTION_PRINT_PREFIX="inj-cpp-static")
target_link_libraries(inj-cpp-static PRIVATE nvtx3-cpp)
set_target_properties(inj-cpp-static PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(inj-c-dynamic SHARED "PrintInjectionC.c" "PrintInjectionImpl.cpp")
target_compile_definitions(inj-c-dynamic PRIVATE SUPPORT_DYNAMIC_INJECTION INJECTION_PRINT_PREFIX="inj-c-dynamic")
target_link_libraries(inj-c-dynamic PRIVATE nvtx3-c)
set_target_properties(inj-c-dynamic PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(inj-cpp-dynamic SHARED "PrintInjectionCpp.cpp" "PrintInjectionImpl.cpp")
target_compile_definitions(inj-cpp-dynamic PRIVATE SUPPORT_DYNAMIC_INJECTION INJECTION_PRINT_PREFIX="inj-cpp-dynamic")
target_link_libraries(inj-cpp-dynamic PRIVATE nvtx3-cpp)
set_target_properties(inj-cpp-dynamic PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(inj-c-preinject SHARED "PrintInjectionC.c" "PrintInjectionImpl.cpp")
target_compile_definitions(inj-c-preinject PRIVATE SUPPORT_PREINJECTION INJECTION_PRINT_PREFIX="inj-c-preinject")
target_link_libraries(inj-c-preinject PRIVATE nvtx3-c)
set_target_properties(inj-c-preinject PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(inj-cpp-preinject SHARED "PrintInjectionCpp.cpp" "PrintInjectionImpl.cpp")
target_compile_definitions(inj-cpp-preinject PRIVATE SUPPORT_PREINJECTION INJECTION_PRINT_PREFIX="inj-cpp-preinject")
target_link_libraries(inj-cpp-preinject PRIVATE nvtx3-cpp)
set_target_properties(inj-cpp-preinject PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(self SHARED "TestSelfInjection.cpp" "SelfInjection.cpp")
target_link_libraries(self PRIVATE nvtx3-cpp)
set_target_properties(self PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(calls SHARED "Calls.cpp" "SelfInjection.cpp")
target_link_libraries(calls PRIVATE nvtx3-cpp)
set_target_properties(calls PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(coverage-c SHARED "CoverageC.c")
target_link_libraries(coverage-c PRIVATE nvtx3-c)
set_target_properties(coverage-c PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

if(NOT MSVC)
    add_library(coverage-c-static-obj-c SHARED "CoverageC.c" "PrintInjectionC.c" "PrintInjectionImpl.cpp")
    target_compile_definitions(coverage-c-static-obj-c PRIVATE SUPPORT_STATIC_INJECTION INJECTION_PRINT_PREFIX="inj-c-static")
    target_link_libraries(coverage-c-static-obj-c PRIVATE nvtx3-c)
    set_target_properties(coverage-c-static-obj-c PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

    add_library(coverage-c-static-obj-cpp SHARED "CoverageC.c" "PrintInjectionCpp.cpp" "PrintInjectionImpl.cpp")
    target_compile_definitions(coverage-c-static-obj-cpp PRIVATE SUPPORT_STATIC_INJECTION INJECTION_PRINT_PREFIX="inj-cpp-static")
    target_link_libraries(coverage-c-static-obj-cpp PRIVATE nvtx3-c)
    set_target_properties(coverage-c-static-obj-cpp PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

    if(NOT APPLE)
        add_library(coverage-c-static-lib-c SHARED "CoverageC.c")
        target_link_libraries(coverage-c-static-lib-c PRIVATE nvtx3-c inj-c-static)
        target_link_libraries(coverage-c-static-lib-c PRIVATE -Wl,--whole-archive inj-c-static -Wl,--no-whole-archive)
        set_target_properties(coverage-c-static-lib-c PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

        add_library(coverage-c-static-lib-cpp SHARED "CoverageC.c")
        target_link_libraries(coverage-c-static-lib-cpp PRIVATE nvtx3-c inj-cpp-static)
        target_link_libraries(coverage-c-static-lib-cpp PRIVATE -Wl,--whole-archive inj-cpp-static -Wl,--no-whole-archive)
        set_target_properties(coverage-c-static-lib-cpp PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
    endif()
endif()

add_library(coverage-c-preinject SHARED "CoverageC.c")
target_compile_definitions(coverage-c-preinject PRIVATE NVTX_SUPPORT_ALREADY_INJECTED_LIBRARY=1)
target_link_libraries(coverage-c-preinject PRIVATE nvtx3-c)
set_target_properties(coverage-c-preinject PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(coverage-c-disabled SHARED "CoverageC.c")
target_compile_definitions(coverage-c-disabled PRIVATE NVTX_DISABLE)
target_link_libraries(coverage-c-disabled PRIVATE nvtx3-c)
set_target_properties(coverage-c-disabled PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(coverage-cpp SHARED "CoverageCpp.cpp")
target_link_libraries(coverage-cpp PRIVATE nvtx3-cpp)
set_target_properties(coverage-cpp PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

if(NOT MSVC)
    add_library(coverage-cpp-static-obj-c SHARED "CoverageCpp.cpp" "PrintInjectionC.c" "PrintInjectionImpl.cpp")
    target_compile_definitions(coverage-cpp-static-obj-c PRIVATE SUPPORT_STATIC_INJECTION INJECTION_PRINT_PREFIX="inj-c-static")
    target_link_libraries(coverage-cpp-static-obj-c PRIVATE nvtx3-cpp)
    set_target_properties(coverage-cpp-static-obj-c PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

    add_library(coverage-cpp-static-obj-cpp SHARED "CoverageCpp.cpp" "PrintInjectionCpp.cpp" "PrintInjectionImpl.cpp")
    target_compile_definitions(coverage-cpp-static-obj-cpp PRIVATE SUPPORT_STATIC_INJECTION INJECTION_PRINT_PREFIX="inj-cpp-static")
    target_link_libraries(coverage-cpp-static-obj-cpp PRIVATE nvtx3-cpp)
    set_target_properties(coverage-cpp-static-obj-cpp PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

    if(NOT APPLE)
        add_library(coverage-cpp-static-lib-c SHARED "CoverageCpp.cpp")
        target_link_libraries(coverage-cpp-static-lib-c PRIVATE nvtx3-cpp)
        target_link_libraries(coverage-cpp-static-lib-c PRIVATE -Wl,--whole-archive inj-c-static -Wl,--no-whole-archive)
        set_target_properties(coverage-cpp-static-lib-c PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

        add_library(coverage-cpp-static-lib-cpp SHARED "CoverageCpp.cpp")
        target_link_libraries(coverage-cpp-static-lib-cpp PRIVATE nvtx3-cpp)
        target_link_libraries(coverage-cpp-static-lib-cpp PRIVATE -Wl,--whole-archive inj-cpp-static -Wl,--no-whole-archive)
        set_target_properties(coverage-cpp-static-lib-cpp PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
    endif()
endif()

add_library(coverage-cpp-preinject SHARED "CoverageCpp.cpp")
target_compile_definitions(coverage-cpp-preinject PRIVATE NVTX_SUPPORT_ALREADY_INJECTED_LIBRARY=1)
target_link_libraries(coverage-cpp-preinject PRIVATE nvtx3-cpp)
set_target_properties(coverage-cpp-preinject PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(coverage-cpp-disabled SHARED "CoverageCpp.cpp")
target_compile_definitions(coverage-cpp-disabled PRIVATE NVTX_DISABLE)
target_link_libraries(coverage-cpp-disabled PRIVATE nvtx3-cpp)
set_target_properties(coverage-cpp-disabled PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

if(ENABLE_CUDA)
    add_library(coverage-cu SHARED "CoverageCuda.cu")
    target_link_libraries(coverage-cu PRIVATE nvtx3-cpp)
    set_target_properties(coverage-cu PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

    add_library(coverage-cu-disabled SHARED "CoverageCuda.cu")
    target_compile_definitions(coverage-cu-disabled PRIVATE NVTX_DISABLE)
    target_link_libraries(coverage-cu-disabled PRIVATE nvtx3-cpp)
    set_target_properties(coverage-cu-disabled PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
endif()

add_library(coverage-ext-mem SHARED "CoverageExtMem.c")
target_link_libraries(coverage-ext-mem PRIVATE nvtx3-c)
set_target_properties(coverage-ext-mem PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(coverage-ext-mem-disabled SHARED "CoverageExtMem.c")
target_compile_definitions(coverage-ext-mem-disabled PRIVATE NVTX_DISABLE)
target_link_libraries(coverage-ext-mem-disabled PRIVATE nvtx3-c)
set_target_properties(coverage-ext-mem-disabled PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

if(ENABLE_CUDA)
    add_library(coverage-ext-memcudart SHARED "CoverageExtMemCudaRt.cu")
    target_link_libraries(coverage-ext-memcudart PRIVATE nvtx3-c)
    set_target_properties(coverage-ext-memcudart PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

    if(NOT MSVC)
        add_library(coverage-ext-memcudart-disabled SHARED "CoverageExtMemCudaRt.cu")
        target_compile_definitions(coverage-ext-memcudart-disabled PRIVATE NVTX_DISABLE)
        target_link_libraries(coverage-ext-memcudart-disabled PRIVATE nvtx3-c)
        set_target_properties(coverage-ext-memcudart-disabled PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
    endif()
endif()

add_library(coverage-ext-payload SHARED "CoverageExtPayload.c")
target_link_libraries(coverage-ext-payload PRIVATE nvtx3-c)
set_target_properties(coverage-ext-payload PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(coverage-ext-payload-disabled SHARED "CoverageExtPayload.c")
target_compile_definitions(coverage-ext-payload-disabled PRIVATE NVTX_DISABLE)
target_link_libraries(coverage-ext-payload-disabled PRIVATE nvtx3-c)
set_target_properties(coverage-ext-payload-disabled PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(coverage-ext-counter SHARED "CoverageExtCounter.c")
target_link_libraries(coverage-ext-counter PRIVATE nvtx3-c)
set_target_properties(coverage-ext-counter PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(coverage-ext-counter-disabled SHARED "CoverageExtCounter.c")
target_compile_definitions(coverage-ext-counter-disabled PRIVATE NVTX_DISABLE)
target_link_libraries(coverage-ext-counter-disabled PRIVATE nvtx3-c)
set_target_properties(coverage-ext-counter-disabled PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(attributes SHARED "Attributes.cpp")
target_link_libraries(attributes PRIVATE nvtx3-cpp)
set_target_properties(attributes PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(domains SHARED "Domains.cpp")
target_link_libraries(domains PRIVATE nvtx3-cpp)
set_target_properties(domains PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(categories SHARED "NamedCategories.cpp")
target_link_libraries(categories PRIVATE nvtx3-cpp)
set_target_properties(categories PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(regstrings SHARED "RegisteredStrings.cpp")
target_link_libraries(regstrings PRIVATE nvtx3-cpp)
set_target_properties(regstrings PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(linkerdupes SHARED "LinkerDupesMain.cpp" "LinkerDupesFileA.cpp" "LinkerDupesFileB.cpp")
target_link_libraries(linkerdupes PRIVATE nvtx3-cpp)
set_target_properties(linkerdupes PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(export-api SHARED "ExportApi.c")
target_link_libraries(export-api PRIVATE nvtx3-c)
target_include_directories(export-api PRIVATE "Imports/cuda_lite" "Imports/opencl_lite")
set_target_properties(export-api PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

target_link_options(export-api PRIVATE
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<PLATFORM_ID:Windows>>:-Wl,--kill-at>
)

add_library(export-api-ext SHARED "ExportApi.c")
target_compile_definitions(export-api-ext PRIVATE SUPPORT_EXTENSIONS)
target_link_libraries(export-api-ext PRIVATE nvtx3-c)
target_include_directories(export-api-ext PRIVATE "Imports/cuda_lite" "Imports/opencl_lite")
set_target_properties(export-api-ext PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

target_link_options(export-api-ext PRIVATE
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<PLATFORM_ID:Windows>>:-Wl,--kill-at>
)

add_library(use-exported-api SHARED "UseExportedApi.cpp")
target_link_libraries(use-exported-api PRIVATE nvtx3-cpp)
target_include_directories(use-exported-api PRIVATE "Imports/cuda_lite" "Imports/opencl_lite")
set_target_properties(use-exported-api PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(use-exported-api-ext SHARED "UseExportedApi.cpp")
target_compile_definitions(use-exported-api-ext PRIVATE SUPPORT_EXTENSIONS)
target_link_libraries(use-exported-api-ext PRIVATE nvtx3-cpp)
target_include_directories(use-exported-api-ext PRIVATE "Imports/cuda_lite" "Imports/opencl_lite")
set_target_properties(use-exported-api-ext PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

enable_testing()
add_test(NAME "Self" COMMAND runtest -t self)
add_test(NAME "Self with SelfInjection" COMMAND runtest -t self -i self)
add_test(NAME "Self with PrintInjectionC" COMMAND runtest -t self -i inj-c)
add_test(NAME "Self with PrintInjectionCpp" COMMAND runtest -t self -i inj-cpp)
add_test(NAME "Calls" COMMAND runtest -t calls)
add_test(NAME "Calls with CallsInjection" COMMAND runtest -t calls -i calls)
add_test(NAME "Calls with PrintInjectionC" COMMAND runtest -t calls -i inj-c)
add_test(NAME "Calls with PrintInjectionCpp" COMMAND runtest -t calls -i inj-cpp)
add_test(NAME "CoverageC" COMMAND runtest -t coverage-c)
add_test(NAME "CoverageC with PrintInjectionC" COMMAND runtest -t coverage-c -i inj-c)
add_test(NAME "CoverageC with PrintInjectionCpp" COMMAND runtest -t coverage-c -i inj-cpp)
if(NOT MSVC)
    add_test(NAME "CoverageC with PrintInjectionC as StaticObjectC" COMMAND runtest -t coverage-c-static-obj-c)
    add_test(NAME "CoverageC with PrintInjectionCpp as StaticObjectCpp" COMMAND runtest -t coverage-c-static-obj-cpp)
    if(NOT APPLE)
        add_test(NAME "CoverageC with PrintInjectionC as StaticLibraryC" COMMAND runtest -t coverage-c-static-lib-c)
        add_test(NAME "CoverageC with PrintInjectionCpp as StaticLibraryCpp" COMMAND runtest -t coverage-c-static-lib-cpp)
    endif()
endif()
add_test(NAME "CoverageC with PrintInjectionC as Dynamic" COMMAND runtest -t coverage-c -i inj-c-dynamic)
add_test(NAME "CoverageC with PrintInjectionCpp as Dynamic" COMMAND runtest -t coverage-c -i inj-cpp-dynamic)
add_test(NAME "CoverageC with PrintInjectionC as Preinjection" COMMAND runtest -t coverage-c-preinject)
add_test(NAME "CoverageC with PrintInjectionCpp as Preinjection" COMMAND runtest -t coverage-c-preinject)
add_test(NAME "CoverageC with NVTX_DISABLE" COMMAND runtest -t coverage-c-disabled)
add_test(NAME "CoverageCpp" COMMAND runtest -t coverage-cpp)
add_test(NAME "CoverageCpp with PrintInjectionC" COMMAND runtest -t coverage-cpp -i inj-c)
add_test(NAME "CoverageCpp with PrintInjectionCpp" COMMAND runtest -t coverage-cpp -i inj-cpp)
if(NOT MSVC)
    add_test(NAME "CoverageCpp with PrintInjectionC as StaticObjectC" COMMAND runtest -t coverage-cpp-static-obj-c)
    add_test(NAME "CoverageCpp with PrintInjectionCpp as StaticObjectCpp" COMMAND runtest -t coverage-cpp-static-obj-cpp)
    if(NOT APPLE)
        add_test(NAME "CoverageCpp with PrintInjectionC as StaticLibraryC" COMMAND runtest -t coverage-cpp-static-lib-c)
        add_test(NAME "CoverageCpp with PrintInjectionCpp as StaticLibraryCpp" COMMAND runtest -t coverage-cpp-static-lib-cpp)
    endif()
endif()
add_test(NAME "CoverageCpp with PrintInjectionC as Dynamic" COMMAND runtest -t coverage-cpp -i inj-c-dynamic)
add_test(NAME "CoverageCpp with PrintInjectionCpp as Dynamic" COMMAND runtest -t coverage-cpp -i inj-cpp-dynamic)
add_test(NAME "CoverageCpp with PrintInjectionC as Preinjection" COMMAND runtest -t coverage-cpp-preinject)
add_test(NAME "CoverageCpp with PrintInjectionCpp as Preinjection" COMMAND runtest -t coverage-cpp-preinject)
add_test(NAME "CoverageCpp with NVTX_DISABLE" COMMAND runtest -t coverage-cpp-disabled)
if(ENABLE_CUDA)
    add_test(NAME "CoverageCuda" COMMAND runtest -t coverage-cu)
    add_test(NAME "CoverageCuda with PrintInjectionC" COMMAND runtest -t coverage-cu -i inj-c)
    add_test(NAME "CoverageCuda with PrintInjectionCpp" COMMAND runtest -t coverage-cu -i inj-cpp)
    add_test(NAME "CoverageCuda with NVTX_DISABLE" COMMAND runtest -t coverage-cu-disabled)
endif()
add_test(NAME "CoverageExtMem" COMMAND runtest -t coverage-ext-mem)
add_test(NAME "CoverageExtMem with PrintInjectionC" COMMAND runtest -t coverage-ext-mem -i inj-c)
add_test(NAME "CoverageExtMem with PrintInjectionCpp" COMMAND runtest -t coverage-ext-mem -i inj-cpp)
add_test(NAME "CoverageExtMem with NVTX_DISABLE" COMMAND runtest -t coverage-ext-mem-disabled)
if(ENABLE_CUDA)
    add_test(NAME "CoverageExtMemCudaRt" COMMAND runtest -t coverage-ext-memcudart)
    add_test(NAME "CoverageExtMemCudaRt with PrintInjectionC" COMMAND runtest -t coverage-ext-memcudart -i inj-c)
    add_test(NAME "CoverageExtMemCudaRt with PrintInjectionCpp" COMMAND runtest -t coverage-ext-memcudart -i inj-cpp)
    if(NOT MSVC)
        add_test(NAME "CoverageExtMemCudaRt with NVTX_DISABLE" COMMAND runtest -t coverage-ext-memcudart-disabled)
    endif()
endif()
add_test(NAME "CoverageExtPayload" COMMAND runtest -t coverage-ext-payload)
add_test(NAME "CoverageExtPayload with PrintInjectionC" COMMAND runtest -t coverage-ext-payload -i inj-c)
add_test(NAME "CoverageExtPayload with PrintInjectionCpp" COMMAND runtest -t coverage-ext-payload -i inj-cpp)
add_test(NAME "CoverageExtPayload with NVTX_DISABLE" COMMAND runtest -t coverage-ext-payload-disabled)
add_test(NAME "CoverageExtCounter" COMMAND runtest -t coverage-ext-counter)
add_test(NAME "CoverageExtCounter with PrintInjectionC" COMMAND runtest -t coverage-ext-counter -i inj-c)
add_test(NAME "CoverageExtCounter with PrintInjectionCpp" COMMAND runtest -t coverage-ext-counter -i inj-cpp)
add_test(NAME "CoverageExtCounter with NVTX_DISABLE" COMMAND runtest -t coverage-ext-counter-disabled)
add_test(NAME "Attributes" COMMAND runtest -t attributes)
add_test(NAME "Attributes with PrintInjectionC" COMMAND runtest -t attributes -i inj-c)
add_test(NAME "Attributes with PrintInjectionCpp" COMMAND runtest -t attributes -i inj-cpp)
add_test(NAME "Domains" COMMAND runtest -t domains)
add_test(NAME "Domains with PrintInjectionC" COMMAND runtest -t domains -i inj-c)
add_test(NAME "Domains with PrintInjectionCpp" COMMAND runtest -t domains -i inj-cpp)
add_test(NAME "NamedCategories" COMMAND runtest -t categories)
add_test(NAME "NamedCategories with PrintInjectionC" COMMAND runtest -t categories -i inj-c)
add_test(NAME "NamedCategories with PrintInjectionCpp" COMMAND runtest -t categories -i inj-cpp)
add_test(NAME "RegisteredStrings" COMMAND runtest -t regstrings)
add_test(NAME "RegisteredStrings with PrintInjectionC" COMMAND runtest -t regstrings -i inj-c)
add_test(NAME "RegisteredStrings with PrintInjectionCpp" COMMAND runtest -t regstrings -i inj-cpp)
add_test(NAME "LinkerDupes" COMMAND runtest -t linkerdupes)
add_test(NAME "LinkerDupes with PrintInjectionC" COMMAND runtest -t linkerdupes -i inj-c)
add_test(NAME "LinkerDupes with PrintInjectionCpp" COMMAND runtest -t linkerdupes -i inj-cpp)
add_test(NAME "UseExportedApi" COMMAND runtest -t use-exported-api)
add_test(NAME "UseExportedApi with PrintInjectionC" COMMAND runtest -t use-exported-api -i inj-c)
add_test(NAME "UseExportedApi with PrintInjectionCpp" COMMAND runtest -t use-exported-api -i inj-cpp)
add_test(NAME "UseExportedApiExt" COMMAND runtest -t use-exported-api-ext)
add_test(NAME "UseExportedApiExt with PrintInjectionC" COMMAND runtest -t use-exported-api-ext -i inj-c)
add_test(NAME "UseExportedApiExt with PrintInjectionCpp" COMMAND runtest -t use-exported-api-ext -i inj-cpp)

if(APPLE)
    set_property(TEST "CoverageC with PrintInjectionC as Preinjection"
        PROPERTY
        ENVIRONMENT DYLD_INSERT_LIBRARIES=${CMAKE_BINARY_DIR}/libinj-c-preinject.dylib)
    set_property(TEST "CoverageC with PrintInjectionCpp as Preinjection"
        PROPERTY
        ENVIRONMENT DYLD_INSERT_LIBRARIES=${CMAKE_BINARY_DIR}/libinj-cpp-preinject.dylib)
    set_property(TEST "CoverageCpp with PrintInjectionC as Preinjection"
        PROPERTY
        ENVIRONMENT DYLD_INSERT_LIBRARIES=${CMAKE_BINARY_DIR}/libinj-c-preinject.dylib)
    set_property(TEST "CoverageCpp with PrintInjectionCpp as Preinjection"
        PROPERTY
        ENVIRONMENT DYLD_INSERT_LIBRARIES=${CMAKE_BINARY_DIR}/libinj-cpp-preinject.dylib)
elseif(CYGWIN)
    set_property(TEST "CoverageC with PrintInjectionC as Preinjection"
        PROPERTY
        ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/cyginj-c-preinject.dll)
    set_property(TEST "CoverageC with PrintInjectionCpp as Preinjection"
        PROPERTY
        ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/cyginj-cpp-preinject.dll)
    set_property(TEST "CoverageCpp with PrintInjectionC as Preinjection"
        PROPERTY
        ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/cyginj-c-preinject.dll)
    set_property(TEST "CoverageCpp with PrintInjectionCpp as Preinjection"
        PROPERTY
        ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/cyginj-cpp-preinject.dll)
elseif(NOT WIN32)
    set_property(TEST "CoverageC with PrintInjectionC as Preinjection"
        PROPERTY
        ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/libinj-c-preinject.so)
    set_property(TEST "CoverageC with PrintInjectionCpp as Preinjection"
        PROPERTY
        ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/libinj-cpp-preinject.so)
    set_property(TEST "CoverageCpp with PrintInjectionC as Preinjection"
        PROPERTY
        ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/libinj-c-preinject.so)
    set_property(TEST "CoverageCpp with PrintInjectionCpp as Preinjection"
        PROPERTY
        ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/libinj-cpp-preinject.so)
endif()

add_subdirectory(cmake)
