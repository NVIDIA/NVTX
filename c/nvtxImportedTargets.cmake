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

set(NVTX3_VERSION 3.1.0)

# This CMake script defines targets the NVTX C and C++ APIs.
# By default, these targets are defined as IMPORTED, so libraries can include
# this file to define the NVTX targets scoped locally to their directory, and
# ensure they get the expected version even if other libraries also use older
# NVTX versions (which define the same target names).
#
# To use one import of NVTX for the whole build, call add_subdirectory on this
# directory.  The CMakeLists.txt sets NVTX3_TARGETS_NOT_USING_IMPORTED before
# including this script, so it will remove the IMPORTED option, and define the
# targets at global scope.  If multiple NVTX versions are added this way, the
# newest version must be added first, or a warning is printed when CMake runs.

if (NVTX3_TARGETS_NOT_USING_IMPORTED)
    set(OPTIONALLY_IMPORTED "")
else()
    set(OPTIONALLY_IMPORTED "IMPORTED")
endif()

if (TARGET nvtx3-c AND NVTX3_TARGETS_NOT_USING_IMPORTED)
    # NVTX CMake targets are already defined, so skip redefining them.
    # Emit a warning if the already-defined version is older, since
    # a project is getting an older NVTX version than it expected.
    # When defining IMPORTED targets, they are scoped to a directory,
    # so only perform this test when not using IMPORTED.
    get_target_property(NVTX3_ALREADY_DEFINED_VERSION nvtx3-c VERSION)
    if (${NVTX3_ALREADY_DEFINED_VERSION} VERSION_LESS ${NVTX3_VERSION})
        message(WARNING
            "NVTX version ${NVTX3_VERSION} is not defining CMake targets, because they were already defined by version ${NVTX3_ALREADY_DEFINED_VERSION}.  This means a library is using an older version of NVTX than expected.  Reorder CMake add_subdirectory or include commands to ensure the newest NVTX version is first, and the NVTX targets will be defined by that version.")
    endif()
    unset(NVTX3_ALREADY_DEFINED_VERSION)
else()
    #-------------------------------------------------------
    # Define "nvtx3-c" library for the NVTX v3 C API
    add_library(nvtx3-c INTERFACE ${OPTIONALLY_IMPORTED})
    set_target_properties(nvtx3-c PROPERTIES VERSION ${NVTX3_VERSION})
    target_include_directories(nvtx3-c INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
    target_link_libraries(nvtx3-c INTERFACE ${CMAKE_DL_LIBS})

    #-------------------------------------------------------
    # Define "nvtx3-cpp" library for the NVTX v3 C++ API
    # Separate target allows attaching independent compiler requirements if needed
    add_library(nvtx3-cpp INTERFACE ${OPTIONALLY_IMPORTED})
    set_target_properties(nvtx3-cpp PROPERTIES VERSION ${NVTX3_VERSION})
    target_link_libraries(nvtx3-cpp INTERFACE nvtx3-c)
endif()

unset(OPTIONALLY_IMPORTED)
unset(NVTX3_VERSION)
