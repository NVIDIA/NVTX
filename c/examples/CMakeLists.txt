cmake_minimum_required(VERSION 3.19)
project(nvtx3_example)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find NVTX3 package
find_package(nvtx3 REQUIRED)

# Show where the package is installed
message(STATUS "NVTX3 package is installed in: ${nvtx3_DIR}")

# Show the include directory of target nvtx3-cpp
get_target_property(nvtx3_include_dir nvtx3::nvtx3-c INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "NVTX3 include directory: ${nvtx3_include_dir}")

# Create an example executable
add_executable(example main.cpp)
target_link_libraries(example PRIVATE nvtx3::nvtx3-cpp)
