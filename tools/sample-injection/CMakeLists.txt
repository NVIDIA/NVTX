#=============================================================================
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
#=============================================================================

cmake_minimum_required(VERSION 3.16)

project(nvtx_sample_injection C CXX)

set(NVTX_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/Import/NVTX/include)

# NVTX Payload Parser - self-contained decode/format module, reusable for
# post-processing (e.g. trace readers) without any injection dependencies.
add_library(nvtx_payload_parser OBJECT
    Source/Payload/NvtxPayloadRegistry.cpp
    Source/Payload/NvtxPayloadParser.cpp
    Source/Payload/NvtxPayloadSchema.cpp
    Source/Payload/NvtxPayloadTextFormatter.cpp
    Source/Payload/NvtxPayloadJsonFormatter.cpp
)
target_include_directories(nvtx_payload_parser PUBLIC
    ${NVTX_INCLUDE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Payload
)
# Use C++14 on older MSVC (VS 2015); C++17 elsewhere.
if(MSVC AND MSVC_TOOLSET_VERSION LESS_EQUAL 140)
    set(nvtx_payload_parser_cxx_std 14)
else()
    set(nvtx_payload_parser_cxx_std 17)
endif()

set_target_properties(nvtx_payload_parser PROPERTIES
    CXX_STANDARD ${nvtx_payload_parser_cxx_std}
    POSITION_INDEPENDENT_CODE ON
)

# NVTX Sample Injection
add_library(nvtx_sample_injection SHARED
    Source/NvtxSampleInjection.cpp
    Source/NvtxPayloadInjectionAdapter.cpp
)
target_link_libraries(nvtx_sample_injection PRIVATE nvtx_payload_parser)
target_include_directories(nvtx_sample_injection PRIVATE
    ${NVTX_INCLUDE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Payload
)
set_target_properties(nvtx_sample_injection PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    CXX_STANDARD 17
)

# Test
add_executable(test
    Test/NvtxTest.cpp
)
target_include_directories(test PRIVATE ${NVTX_INCLUDE_PATH})
set_target_properties(test PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    CXX_STANDARD 17
)

add_executable(test_payloads_api
    Test/NvtxPayloadsApiTests.c
)
target_include_directories(test_payloads_api PRIVATE ${NVTX_INCLUDE_PATH})
set_target_properties(test_payloads_api PROPERTIES
    C_VISIBILITY_PRESET hidden
    C_STANDARD 11
)

add_executable(test_payloads_cpp_api
    Test/NvtxPayloadsCppApiTests.cpp
)
target_include_directories(test_payloads_cpp_api PRIVATE ${NVTX_INCLUDE_PATH})
set_target_properties(test_payloads_cpp_api PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    CXX_STANDARD 17
)
