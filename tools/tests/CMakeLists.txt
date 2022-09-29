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

include(CTest)

CPMAddPackage(
  NAME GTest
  GITHUB_REPOSITORY google/googletest
  GIT_TAG release-1.10.0
  VERSION 1.10.0
  GIT_SHALLOW TRUE
  OPTIONS "INSTALL_GTEST OFF"
  # googletest >= 1.10.0 provides a cmake config file -- use it if it exists
  FIND_PACKAGE_ARGUMENTS "CONFIG")

###################################################################################################
# - compiler function -----------------------------------------------------------------------------

function(ConfigureTest CMAKE_TEST_NAME CMAKE_TEST_SRC)
    add_executable(${CMAKE_TEST_NAME} ${CMAKE_TEST_SRC})
    set_target_properties(${CMAKE_TEST_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(${CMAKE_TEST_NAME} gmock gtest gmock_main 
                          gtest_main pthread nvtx3-cpp )
    set_target_properties(${CMAKE_TEST_NAME} PROPERTIES
                            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/gtests")
    add_test(NAME ${CMAKE_TEST_NAME} COMMAND ${CMAKE_TEST_NAME})
endfunction(ConfigureTest)


###################################################################################################
# - nvtx tests -------------------------------------------------------------------------------------

set(NVTX_TEST_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/nvtx_tests.cpp")

ConfigureTest(NVTX_TEST "${NVTX_TEST_SRC}")

###################################################################################################

###################################################################################################
### enable testing ################################################################################
###################################################################################################

enable_testing()

###################################################################################################