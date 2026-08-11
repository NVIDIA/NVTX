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

# Assembles compiler warning flags based on the detected compiler and version.
# Included from tests/CMakeLists.txt after project().

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")

    set(_gcc_warn -Wall -Wextra -Wmissing-braces -Wpointer-arith -Wwrite-strings)

    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "4.1")
        list(APPEND _gcc_warn -Wattributes)
    endif()
    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "4.4")
        list(APPEND _gcc_warn -Wunused-result)
    endif()
    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "4.5")
        list(APPEND _gcc_warn -Wlogical-op -Wcast-qual)
    endif()
    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "6")
        list(APPEND _gcc_warn -Wduplicated-cond -Wnull-dereference)
    endif()
    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "7")
        list(APPEND _gcc_warn -Wduplicated-branches)
    endif()
    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "8")
        list(APPEND _gcc_warn -Warray-bounds=2)
    endif()
    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "9")
        list(APPEND _gcc_warn -Wmultistatement-macros)
    endif()

    add_compile_options(
        ${_gcc_warn}
        $<$<COMPILE_LANGUAGE:C>:-Wmissing-prototypes>
        $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Werror>
    )

elseif(CMAKE_C_COMPILER_ID STREQUAL "NVHPC")

    add_compile_options(
        -Wall
        $<$<COMPILE_LANGUAGE:C,CXX>:-Werror>
    )

elseif(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")

    set(_appleclang_warn
        -Weverything
        -Wno-c++98-compat -Wno-c++98-compat-pedantic
        -Wno-variadic-macros -Wno-long-long
        -Wno-sign-conversion -Wno-float-equal -Wno-padded
        -Wno-covered-switch-default -Wno-atomic-implicit-seq-cst
        -Wno-exit-time-destructors -Wno-global-constructors
        -Wno-missing-variable-declarations
    )

    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "12")
        list(APPEND _appleclang_warn -Wno-poison-system-directories)
    endif()

    if(CMAKE_C_COMPILER_VERSION VERSION_LESS "13.1")
        list(APPEND _appleclang_warn -Wno-reserved-id-macro)
    else()
        list(APPEND _appleclang_warn -Wno-reserved-identifier)
    endif()

    if(CMAKE_C_COMPILER_VERSION VERSION_LESS "14")
        list(APPEND _appleclang_warn -Wno-c++17-extensions -Wno-documentation-unknown-command)
    else()
        list(APPEND _appleclang_warn -Wno-c++17-attribute-extensions)
    endif()

    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "15")
        list(APPEND _appleclang_warn -Wno-unsafe-buffer-usage)
    endif()

    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "16")
        list(APPEND _appleclang_warn -Wno-cast-function-type-strict)
    endif()

    if(CMAKE_CXX_STANDARD STREQUAL "98")
        list(APPEND _appleclang_warn -Wno-pedantic)
    endif()

    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "16")
        list(APPEND _appleclang_warn -Wno-pre-c23-compat -Wno-pre-c11-compat)
    elseif(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "14")
        list(APPEND _appleclang_warn -Wno-pre-c2x-compat)
    endif()

    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "21")
        list(APPEND _appleclang_warn -Wno-c++-keyword)
    endif()

    add_compile_options(${_appleclang_warn} $<$<COMPILE_LANGUAGE:C,CXX>:-Werror>)

elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")

    if(MSVC)
        # clang-cl: no custom warning flags
    else()
        set(_clang_warn
            -Weverything
            -Wno-c++98-compat -Wno-c++98-compat-pedantic
            -Wno-variadic-macros -Wno-long-long
            -Wno-sign-conversion -Wno-float-equal -Wno-padded
            -Wno-covered-switch-default -Wno-atomic-implicit-seq-cst
            -Wno-exit-time-destructors -Wno-global-constructors
            -Wno-missing-variable-declarations
        )

        if(CMAKE_C_COMPILER_VERSION VERSION_LESS "13")
            list(APPEND _clang_warn -Wno-reserved-id-macro)
        else()
            list(APPEND _clang_warn -Wno-reserved-identifier)
        endif()

        if(CMAKE_C_COMPILER_VERSION VERSION_LESS "14")
            list(APPEND _clang_warn -Wno-c++17-extensions -Wno-documentation-unknown-command)
        else()
            list(APPEND _clang_warn -Wno-c++17-attribute-extensions)
        endif()

        if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "16")
            list(APPEND _clang_warn -Wno-unsafe-buffer-usage -Wno-cast-function-type-strict)
        endif()

        if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "18")
            list(APPEND _clang_warn -Wno-pre-c23-compat)
        elseif(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "14")
            list(APPEND _clang_warn -Wno-pre-c2x-compat)
        endif()

        if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "19")
            list(APPEND _clang_warn -Wno-pre-c11-compat)
        endif()

        if(WIN32 OR APPLE OR NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "21")
            list(APPEND _clang_warn -Wno-c++-keyword)
        endif()

        if(CMAKE_CXX_STANDARD STREQUAL "98")
            list(APPEND _clang_warn -Wno-pedantic)
        endif()

        add_compile_options(${_clang_warn} $<$<COMPILE_LANGUAGE:C,CXX>:-Werror>)

        if(ENABLE_CUDA)
            set(_cuda_host_extras
                -Wno-undef -Wno-newline-eof -Wno-old-style-cast
                -Wno-missing-noreturn -Wno-deprecated-dynamic-exception-spec
                -Wno-unused-template -Wno-zero-as-null-pointer-constant
                -Wno-used-but-marked-unused -Wno-extra-semi-stmt
                -Wno-disabled-macro-expansion -Wno-duplicate-enum -Wno-unused-function
                -Wno-nested-anon-types -Wno-shift-sign-overflow
            )
            if(CMAKE_C_COMPILER_VERSION VERSION_LESS "13")
                list(APPEND _cuda_host_extras -Wno-unknown-warning-option)
            endif()
            if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "15")
                list(APPEND _cuda_host_extras -Wno-gnu-line-marker)
            endif()
            foreach(_f IN LISTS _cuda_host_extras)
                add_compile_options("$<$<COMPILE_LANGUAGE:CUDA>:${_f}>")
            endforeach()
        endif()

    endif()

elseif(MSVC)

    set(_msvc_warn
        -Wall -WX
        -wd4191 -wd4255 -wd4355 -wd4365 -wd4514
        -wd4668 -wd4710 -wd4711 -wd4820
        -wd5039 -wd5045 -wd5220
    )

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64"
       OR CMAKE_C_COMPILER_ARCHITECTURE_ID STREQUAL "ARM64")
        list(APPEND _msvc_warn -wd4746)
    endif()

    # UCRT float helpers emit C4738 on 32-bit x86 under /Wall.
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        list(APPEND _msvc_warn -wd4738)
    endif()

    if(MSVC_TOOLSET_VERSION LESS 143)
        # VS2019 and earlier
        list(APPEND _msvc_warn -wd4013)
    endif()
    if(MSVC_TOOLSET_VERSION LESS 142)
        # VS2017 and earlier
        list(APPEND _msvc_warn -wd4571 -wd4623 -wd4625 -wd4626 -wd4774 -wd5026 -wd5027)
    endif()
    if(MSVC_TOOLSET_VERSION LESS 141)
        # VS2015 only
        list(APPEND _msvc_warn -wd4628)
    endif()
    if(NOT MSVC_TOOLSET_VERSION LESS 144)
        # VS2026+
        list(APPEND _msvc_warn -wd4865)
    endif()

    if(MSVC_TOOLSET_VERSION LESS 141)
        # VS2015: no conformant preprocessor support
    elseif(MSVC_TOOLSET_VERSION LESS 142)
        # VS2017
        list(APPEND _msvc_warn -experimental:preprocessor)
    else()
        # VS2019+
        list(APPEND _msvc_warn -Zc:preprocessor)
    endif()

    add_compile_options(${_msvc_warn})

    if(ENABLE_CUDA)
        add_compile_options($<$<COMPILE_LANGUAGE:CUDA>:-wd4555>)
    endif()

endif()

if(ENABLE_CUDA)
    string(APPEND CMAKE_CUDA_FLAGS " --Wno-deprecated-gpu-targets --Werror all-warnings")
endif()
