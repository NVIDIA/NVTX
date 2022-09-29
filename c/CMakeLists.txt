#=============================================================================
# Copyright (c) 2022, NVIDIA CORPORATION.
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
#=============================================================================
cmake_minimum_required(VERSION 3.10)

# This file is typically used by calling add_subdirectory on this directory or add
# this directory as an external package.  In this case, the NVTX library targets
# should not be defined as IMPORTED, because they would be scoped to this directory,
# and not accessible elsewhere.  Set NVTX3_TARGETS_NOT_USING_IMPORTED to instruct
# nvtxImportedTargets.cmake not to use IMPORTED.  If multiple versions of NVTX are
# used this way, the newest version must be added first, or a warning message will
# be printed when CMake runs.
#
# If multiple libraries in the same build need to use a specific version of NVTX,
# they should use "include(path/to/nvtxImportedTargets.cmake)" instead of adding
# this directory.  By default, nvtxImportedTargets.cmake defines targets with the
# IMPORTED option, which only defines the targets within the scope of the library.
# This allows the same target name to be used for different NVTX versions.

set(NVTX3_TARGETS_NOT_USING_IMPORTED ON)
include(nvtxImportedTargets.cmake)
