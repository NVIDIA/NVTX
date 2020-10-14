#=============================================================================
# Copyright (c) 2020, NVIDIA CORPORATION.
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
cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

project(NVTX VERSION 0.1.0 LANGUAGES CXX)

include(cmake/CPM.cmake)

###################################################################################################
# - build type ------------------------------------------------------------------------------------

# Set a default build type if none was specified
set(DEFAULT_BUILD_TYPE "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' since none specified.")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()


###################################################################################################
# - nvtx target -----------------------------------------------------------------------------------
add_library(nvtx3-cpp INTERFACE)
target_include_directories(nvtx3-cpp INTERFACE
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                           "$<INSTALL_INTERFACE:include>")

target_compile_features(nvtx3-cpp INTERFACE cxx_std_11)                           
target_link_libraries(nvtx3-cpp INTERFACE ${CMAKE_DL_LIBS})

# Build options
option(BUILD_TESTS "Configure CMake to build tests" ON)
option(BUILD_BENCHMARKS "Configure CMake to build (google) benchmarks" ON)

###################################################################################################
# - cmake modules ---------------------------------------------------------------------------------

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/" ${CMAKE_MODULE_PATH})

include(FeatureSummary)
include(CheckIncludeFiles)
include(CheckLibraryExists)

###################################################################################################
# - add gtest -------------------------------------------------------------------------------------

if(BUILD_TESTS)
    add_subdirectory(tests)
endif(BUILD_TESTS)

###################################################################################################
# - add google benchmark --------------------------------------------------------------------------

if(BUILD_BENCHMARKS)
  include(ConfigureGoogleBenchmark)
  if(GBENCH_FOUND)
    message(STATUS "Google C++ Benchmarking Framework (Google Benchmark) found in ${GBENCH_ROOT}")
    include_directories(${GBENCH_INCLUDE_DIR})
    add_subdirectory(${CMAKE_SOURCE_DIR}/benchmarks)
  else()
    message(AUTHOR_WARNING "Google C++ Benchmarking Framework (Google Benchmark) not found: automated tests are disabled.")
  endif(GBENCH_FOUND)
endif(BUILD_BENCHMARKS)


###################################################################################################
# - build doxygen ---------------------------------------------------------------------------------
add_custom_command(OUTPUT BUILD_DOXYGEN
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   COMMAND doxygen doxygen/Doxyfile
                   VERBATIM)

add_custom_target(docs DEPENDS BUILD_DOXYGEN)
