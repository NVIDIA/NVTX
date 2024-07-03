# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
# See LICENSE.txt for license information.

cmake_minimum_required (VERSION 3.19)

set(NVTX_LANGUAGES C CXX)
if(NOT APPLE)
    set(NVTX_LANGUAGES ${NVTX_LANGUAGES} CUDA)
    set(NVTX_CUDA True)
else()
    set(NVTX_CUDA False)
endif()

project ("NvtxTests" LANGUAGES ${NVTX_LANGUAGES})

# Enforce standard C/C++ with sensible warnings and minimal compiler output on all platforms
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CUDA_HOST_COMPILER "${CMAKE_CXX_COMPILER}")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -ccbin \"${CMAKE_CXX_COMPILER}\"")
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
        $<$<CXX_COMPILER_ID:GNU,Clang>:-static-libgcc>
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
else()
    # Stop compiling immediately after first error
    add_compile_options(-Wfatal-errors)
    # Check for initializing unions without required braces
    add_compile_options(-Wmissing-braces)
endif()


add_subdirectory("../c" "ImportNvtx")

#if(DOMAINS_ERROR_TEST_NAME_IS_MISSING)
#    target_compile_definitions(domains PRIVATE ERROR_TEST_NAME_IS_MISSING)
#endif()

add_executable(runtest "RunTest.cpp")
target_link_libraries(runtest PRIVATE nvtx3-cpp)

add_library(inj SHARED "PrintInjection.cpp")
target_link_libraries(inj PRIVATE nvtx3-cpp)
set_target_properties(inj PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(self SHARED "TestSelfInjection.cpp" "SelfInjection.cpp")
target_link_libraries(self PRIVATE nvtx3-cpp)
set_target_properties(self PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(calls SHARED "Calls.cpp" "SelfInjection.cpp")
target_link_libraries(calls PRIVATE nvtx3-cpp)
set_target_properties(calls PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(coverage SHARED "Coverage.cpp")
target_link_libraries(coverage PRIVATE nvtx3-cpp)
set_target_properties(coverage PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(coveragec SHARED "CoverageC.c")
target_link_libraries(coveragec PRIVATE nvtx3-c)
set_target_properties(coveragec PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

if(NVTX_CUDA)
    add_library(coverage-cu SHARED "CoverageCuda.cu")
    target_link_libraries(coverage-cu PRIVATE nvtx3-cpp)
    set_target_properties(coverage-cu PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
endif()

add_library(coverage-mem SHARED "CoverageMem.c")
target_link_libraries(coverage-mem PRIVATE nvtx3-c)
set_target_properties(coverage-mem PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

if(NVTX_CUDA)
    add_library(coverage-memcudart SHARED "CoverageMemCudaRt.cu")
    target_link_libraries(coverage-memcudart PRIVATE nvtx3-c)
    set_target_properties(coverage-memcudart PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
endif()

add_library(coverage-payload SHARED "CoveragePayload.c")
target_link_libraries(coverage-payload PRIVATE nvtx3-c)
set_target_properties(coverage-payload PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(coverage-counter SHARED "CoverageCounter.c")
target_link_libraries(coverage-counter PRIVATE nvtx3-c)
set_target_properties(coverage-counter PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

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

enable_testing()
add_test(NAME "Self" COMMAND runtest -t self)
add_test(NAME "Self with SelfInjection" COMMAND runtest -t self -i self)
add_test(NAME "Self with PrintInjection" COMMAND runtest -t self -i inj)
add_test(NAME "Calls" COMMAND runtest -t calls)
add_test(NAME "Calls with CallsInjection" COMMAND runtest -t calls -i calls)
add_test(NAME "Calls with PrintInjection" COMMAND runtest -t calls -i inj)
add_test(NAME "Coverage" COMMAND runtest -t coverage)
add_test(NAME "Coverage with PrintInjection" COMMAND runtest -t coverage -i inj)
add_test(NAME "CoverageC" COMMAND runtest -t coveragec)
add_test(NAME "CoverageC with PrintInjection" COMMAND runtest -t coveragec -i inj)
if(NVTX_CUDA)
    add_test(NAME "CoverageCuda" COMMAND runtest -t coverage-cu)
    add_test(NAME "CoverageCuda with PrintInjection" COMMAND runtest -t coverage-cu -i inj)
endif()
add_test(NAME "CoverageMem" COMMAND runtest -t coverage-mem)
add_test(NAME "CoverageMem with PrintInjection" COMMAND runtest -t coverage-mem -i inj)
if(NVTX_CUDA)
    add_test(NAME "CoverageMemCudaRt" COMMAND runtest -t coverage-memcudart)
    add_test(NAME "CoverageMemCudaRt with PrintInjection" COMMAND runtest -t coverage-memcudart -i inj)
endif()
add_test(NAME "CoveragePayload" COMMAND runtest -t coverage-payload)
add_test(NAME "CoveragePayload with PrintInjection" COMMAND runtest -t coverage-payload -i inj)
add_test(NAME "CoverageCounter" COMMAND runtest -t coverage-counter)
add_test(NAME "CoverageCounter with PrintInjection" COMMAND runtest -t coverage-counter -i inj)
add_test(NAME "Attributes" COMMAND runtest -t attributes)
add_test(NAME "Attributes with PrintInjection" COMMAND runtest -t attributes -i inj)
add_test(NAME "Domains" COMMAND runtest -t domains)
add_test(NAME "Domains with PrintInjection" COMMAND runtest -t domains -i inj)
add_test(NAME "NamedCategories" COMMAND runtest -t categories)
add_test(NAME "NamedCategories with PrintInjection" COMMAND runtest -t categories -i inj)
add_test(NAME "RegisteredStrings" COMMAND runtest -t regstrings)
add_test(NAME "RegisteredStrings with PrintInjection" COMMAND runtest -t regstrings -i inj)
add_test(NAME "LinkerDupes" COMMAND runtest -t linkerdupes)
add_test(NAME "LinkerDupes with PrintInjection" COMMAND runtest -t linkerdupes -i inj)
