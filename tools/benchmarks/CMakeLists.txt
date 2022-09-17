#=============================================================================
# Copyright (c) 2020-2022, NVIDIA CORPORATION.
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

CPMAddPackage(
  NAME benchmark
  GITHUB_REPOSITORY google/benchmark
  VERSION 1.5.2
  GIT_SHALLOW TRUE
  OPTIONS
    "BENCHMARK_ENABLE_TESTING Off"
    "BENCHMARK_ENABLE_INSTALL Off"
    # The REGEX feature test fails when gbench's cmake is run under CPM w/ gcc5.4 because it doesn't assume C++11
    # Additionally, attempting to set the CMAKE_CXX_VERSION here doesn't propogate to the feature test build
    # Therefore, we just disable the feature test and assume platforms we care about have a regex impl available
    "RUN_HAVE_STD_REGEX 0" #
)

if (benchmark_ADDED)
  # patch google benchmark target
  set_target_properties(benchmark PROPERTIES CXX_STANDARD 11)
endif()

###################################################################################################
# - compiler function -----------------------------------------------------------------------------

function(ConfigureBench CMAKE_BENCH_NAME CMAKE_BENCH_SRC)
    add_executable(${CMAKE_BENCH_NAME}
                   ${CMAKE_BENCH_SRC})
    set_target_properties(${CMAKE_BENCH_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(${CMAKE_BENCH_NAME} benchmark benchmark_main pthread nvtx3-cpp)
    set_target_properties(${CMAKE_BENCH_NAME} PROPERTIES
                            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/gbenchmarks")
endfunction(ConfigureBench)

###################################################################################################
### test sources ##################################################################################
###################################################################################################

###################################################################################################
# - example benchmark --------------------------------------------------------------------------------

set(EXAMPLE_BENCH_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/example/example_benchmark.cpp")

ConfigureBench(EXAMPLE_BENCH "${EXAMPLE_BENCH_SRC}")

# - nvtx benchmark --------------------------------------------------------------------------------
set(NVTX_BENCH_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/nvtx/nvtx_benchmark.cpp")

ConfigureBench(NVTX_BENCH "${NVTX_BENCH_SRC}")


###################################################################################################