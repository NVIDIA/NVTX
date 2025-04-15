cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

project(NVTXAddSubdirTest CUDA)

# Default to building for the GPU on the current system
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
  set(CMAKE_CUDA_ARCHITECTURES native)
endif()

if (NOT DEFINED nvtx_root)
  message(FATAL_ERROR "The nvtx_root variable must be defined. "
                      "Please set it to the path of the NVTX CMake directory to test.")
endif()

# Import the NVTX CMake targets using add_subdirectory:
add_subdirectory("${nvtx_root}" "${CMAKE_CURRENT_BINARY_DIR}/nvtx")

# Check that the nvtx-c(pp) targets exists and are IMPORTED targets:
if (NOT TARGET nvtx3-c)
  message(FATAL_ERROR "nvtx3-c target not defined by add_subdirectory(${nvtx_root}).")
endif()
get_target_property(NVTX3_C_IMPORTED nvtx3-c IMPORTED)
if (NOT NVTX3_C_IMPORTED)
  message(FATAL_ERROR "nvtx3-c target is not an IMPORTED target.")
endif()

if (NOT TARGET nvtx3-cpp)
  message(FATAL_ERROR "nvtx3-cpp target not defined by add_subdirectory(${nvtx_root}).")
endif()
get_target_property(NVTX3_CPP_IMPORTED nvtx3-cpp IMPORTED)
if (NOT NVTX3_CPP_IMPORTED)
  message(FATAL_ERROR "nvtx3-cpp target is not an IMPORTED target.")
endif()

# Create a simple test program to check that the headers are found:
add_executable(nvtx_smoke_c smoke.cu)
target_link_libraries(nvtx_smoke_c PRIVATE nvtx3-c)

add_executable(nvtx_smoke_cpp smoke.cu)
target_link_libraries(nvtx_smoke_cpp PRIVATE nvtx3-cpp)

include(CTest)
enable_testing()
add_test(NAME nvtx_smoke_c COMMAND nvtx_smoke_c)
add_test(NAME nvtx_smoke_cpp COMMAND nvtx_smoke_cpp)
