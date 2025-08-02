#=============================================================================
# SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
cmake_minimum_required(VERSION 3.19)

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

# Project declaration needs to be before the configuration
project(nvtx3 VERSION 3.2.2)

# Check if standalone or part of another project:
set(NVTX3_C_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
string(REGEX REPLACE "/c$" "" NVTX3_PROJ_ROOT "${NVTX3_C_ROOT}")
set(NVTX3_TESTS_ROOT "${NVTX3_PROJ_ROOT}/tests")
set(NVTX3_TOP_LEVEL_PROJECT OFF)
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${NVTX3_PROJ_ROOT}" OR
    "${CMAKE_SOURCE_DIR}" STREQUAL "${NVTX3_C_ROOT}" OR
    "${CMAKE_SOURCE_DIR}" STREQUAL "${NVTX3_TESTS_ROOT}")
  set(NVTX3_TOP_LEVEL_PROJECT ON)
endif()

option(NVTX3_TARGETS_NOT_USING_IMPORTED "Turn off to mark NVTX targets as IMPORTED." ON)

option(NVTX3_INSTALL "Install NVTX3" ${NVTX3_TOP_LEVEL_PROJECT})

include(nvtxImportedTargets.cmake)
set_target_properties(nvtx3-c PROPERTIES EXPORT_PROPERTIES VERSION)

if(NVTX3_INSTALL)
    # Add installation rules
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Configure the config file
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/nvtx3-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/nvtx3-config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/nvtx3"
    )

    # Generate version file
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/nvtx3-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # Install headers
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    # Install the config files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/nvtx3-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/nvtx3-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/nvtx3"
    )

    # Install the targets
    install(TARGETS nvtx3-c nvtx3-cpp EXPORT nvtx3-targets)
    install(EXPORT nvtx3-targets
        FILE nvtx3-targets.cmake
        NAMESPACE nvtx3::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/nvtx3")
    export(EXPORT nvtx3-targets
        FILE nvtx3-targets.cmake
        NAMESPACE nvtx3::)
endif()
