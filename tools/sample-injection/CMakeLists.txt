#=============================================================================
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
#=============================================================================

cmake_minimum_required(VERSION 3.16)

project(nvtx_sample_injection CXX)

set(NVTX_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/Import/NVTX/include)

# NVTX Sample Injection
add_library(nvtx_sample_injection SHARED
    Source/NvtxSampleInjection.cpp
)
target_include_directories(nvtx_sample_injection PRIVATE ${NVTX_INCLUDE_PATH})
set_target_properties(nvtx_sample_injection PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    CXX_STANDARD 11
)

# Test
add_executable(test
    Test/NvtxTest.cpp
)
target_include_directories(test PRIVATE ${NVTX_INCLUDE_PATH})
set_target_properties(test PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    CXX_STANDARD 11
)
