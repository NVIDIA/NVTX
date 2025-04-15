cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

# Unused, but required by cmake. Real project is in c/.
project(nvtx_dummy_project)

# All of the cmake logic lives in c/:
add_subdirectory(c)
