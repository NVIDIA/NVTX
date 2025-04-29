cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

project(NVTXAddSubdirTest)

if (NOT DEFINED nvtx_root)
  message(FATAL_ERROR "The nvtx_root variable must be defined. "
                      "Please set it to the path of the NVTX CMake directory to test.")
endif()

# Import the NVTX CMake targets using add_subdirectory:
add_subdirectory("${nvtx_root}" "${CMAKE_CURRENT_BINARY_DIR}/nvtx")

# Check that the nvtx-c(pp) targets exists
if (NOT TARGET nvtx3-c)
  message(FATAL_ERROR "nvtx3-c target not defined by add_subdirectory(${nvtx_root}).")
else()
  message(STATUS "nvtx3-c target found.")
endif()

if (NOT TARGET nvtx3-cpp)
  message(FATAL_ERROR "nvtx3-cpp target not defined by add_subdirectory(${nvtx_root}).")
else()
  message(STATUS "nvtx3-cpp target found.")
endif()

# The targets should not be marked as imported when using add_subdirectory:
get_target_property(NVTX3_C_IMPORTED nvtx3-c IMPORTED)
if (NVTX3_C_IMPORTED)
  message(FATAL_ERROR "nvtx3-c target is an IMPORTED target.")
else()
  message(STATUS "nvtx3-c target is not an IMPORTED target.")
endif()

get_target_property(NVTX3_CPP_IMPORTED nvtx3-cpp IMPORTED)
if (NVTX3_CPP_IMPORTED)
  message(FATAL_ERROR "nvtx3-cpp target is an IMPORTED target.")
else()
  message(STATUS "nvtx3-cpp target is not an IMPORTED target.")
endif()
