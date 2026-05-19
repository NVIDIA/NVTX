# SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

# Default to minimal required versions, but allow higher versions to test optional features.
if(NOT DEFINED CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 90)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT DEFINED CMAKE_C_EXTENSIONS)
    set(CMAKE_C_EXTENSIONS OFF)
endif()
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT DEFINED CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(NVTX_LANGUAGES C CXX)
if(ENABLE_CUDA)
    set(CMAKE_CUDA_HOST_COMPILER "${CMAKE_CXX_COMPILER}")
    set(NVTX_LANGUAGES ${NVTX_LANGUAGES} CUDA)
endif()

project ("NvtxTests" LANGUAGES ${NVTX_LANGUAGES})

if(ENABLE_CUDA)
    if(NOT DEFINED CMAKE_CUDA_STANDARD)
        set(NVTX_CUDA_STANDARD_REQUESTED ${CMAKE_CXX_STANDARD})
        if(NVTX_CUDA_STANDARD_REQUESTED STREQUAL "98")
            set(NVTX_CUDA_STANDARD_REQUESTED 03)
        endif()

        set(NVTX_CUDA_STANDARD_MAX 0)
        foreach(feature IN LISTS CMAKE_CUDA_COMPILE_FEATURES)
            if(feature MATCHES "^cuda_std_([0-9]+)$")
                if(CMAKE_MATCH_1 GREATER NVTX_CUDA_STANDARD_MAX)
                    set(NVTX_CUDA_STANDARD_MAX ${CMAKE_MATCH_1})
                endif()
            endif()
        endforeach()

        if(NVTX_CUDA_STANDARD_MAX GREATER 0 AND
            NVTX_CUDA_STANDARD_REQUESTED GREATER NVTX_CUDA_STANDARD_MAX)
            set(CMAKE_CUDA_STANDARD ${NVTX_CUDA_STANDARD_MAX})
        else()
            set(CMAKE_CUDA_STANDARD ${NVTX_CUDA_STANDARD_REQUESTED})
        endif()
    endif()
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    if(NOT DEFINED CMAKE_CUDA_EXTENSIONS)
        set(CMAKE_CUDA_EXTENSIONS OFF)
    endif()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/CompilerWarnings.cmake")

if(CMAKE_CXX_STANDARD STREQUAL "98" OR CMAKE_CXX_STANDARD LESS 11)
    set(NVTX_ENABLE_CXX11_TESTS False)
else()
    set(NVTX_ENABLE_CXX11_TESTS True)
endif()
if(NOT DEFINED ENABLE_NVTX3_HPP_TESTS)
    set(ENABLE_NVTX3_HPP_TESTS ${NVTX_ENABLE_CXX11_TESTS})
endif()

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
enable_testing()
add_subdirectory(src)
add_subdirectory(cmake)
