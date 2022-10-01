# NVTX for C and C++

This README covers NVTX topics specific to C/C++.  For general NVTX information, see the README in the [NVTX repo root](https://github.com/NVIDIA/NVTX).

The NVTX API is written in C, and the NVTX C++ API is implemented as wrappers for parts of the C API.  In C++, both the NVTX C and NVTX C++ APIs can be used.

## NVTX API Reference Guides

[NVTX C API Reference](https://nvidia.github.io/NVTX/doxygen/index.html)

[NVTX C++ API Reference](https://nvidia.github.io/NVTX/doxygen-cpp/index.html)

The NVTX C and C++ header files include Doxygen comments to provide reference documentation.  The API references were generated from these Doxygen comments.

# NVTX C/C++ Examples

## Simple NVTX C++ with Nsight Systems
This C++ example annotates `some_function` with a Push/Pop range using the function's name.  This range begins at the top of the function body, and automatically ends when the function returns.  The function performs a loop, sleeping for one second in each iteration.  A local `nvtx3::scoped_range` annotates the scope of the loop body with a Push/Pop range.  The loop iteration ranges are nested within the function range.

```c++
#include <nvtx3/nvtx3.hpp>

void some_function()
{
    NVTX3_FUNC_RANGE();  // Range around the whole function

    for (int i = 0; i < 6; ++i) {
        nvtx3::scoped_range loop{"loop range"};  // Range for iteration

        // Make each iteration last for one second
        std::this_thread::sleep_for(std::chrono::seconds{1});
    }
}
```

Normally, this program waits for 6 seconds, and does nothing else.

Launch it from **NVIDIA Nsight Systems**, and you'll see this execution on a timeline:

![alt text](https://raw.githubusercontent.com/jrhemstad/nvtx_wrappers/master/docs/example_range.png "Example NVTX Ranges in Nsight Systems")

The NVTX row shows the function's name "some_function" in the top-level range and the "loop range" message in the nested ranges.  The loop iterations each last for the expected one second.

Using the NVTX C API, the following example would produce the same timeline:

```c
#include <nvtx3/nvToolsExt.h>

void some_function()
{
    nvtxRangePush(__func__);  // Range around the whole function

    for (int i = 0; i < 6; ++i) {
        nvtxRangePush("loop range");  // Range for iteration

        // Make each iteration last for one second
        std::this_thread::sleep_for(std::chrono::seconds{1});

        nvtxRangePop();  // End the inner range
    }

    nvtxRangePop();  // End the outer range
}
```

If the function gets to a `return` or `throw` before the `nvtxRangePop` call, the range will be left unclosed, and tool behavior is undefined for this case.  Using the C++ API is safer, since the local `scoped_range` variable calls `nvtxRangePop` in its destructor.

## Markers

**C**:
```c
nvtxMark("This is a marker");
```
**C++**:
```c++
nvtx3::mark("This is a marker");
```

## Push/Pop Ranges

**C**:
```c
nvtxRangePush("This is a push/pop range");
// Do something interesting in the range
nvtxRangePop();  // Pop must be on same thread as corresponding Push
```
**C++**:
```c++
{
    nvtx3::scoped_range range("This is a push/pop range");
    // Do something interesting in the range
    // Range is popped when scoped_range object goes out of scope
}
```

## Start/End Ranges

**C**:
```c
// Somewhere in the code:
nvtxRangeHandle_t handle = nvtxRangeStart("This is a start/end range");
// Somewhere else in the code, not necessarily same thread as Start call:
nvtxRangeEnd(handle);
```
**C++**:
```c++
// Automatically start and end a range around an object's lifetime:
class SomeResource // movable, but not copyable
{
    // class members, methods, etc.

    // Range starts at construction, ends at destruction
    nvtx3::unique_range range(objectInstanceName);
};
```

## Resource naming

The NVTX C API is used for all resource naming.

```c
// Name the current CPU thread
nvtxNameOsThread(pthread_self(), "Network I/O");
```

```c
// Name CUDA streams
cudaStream_t graphicsStream, aiStream;
cudaStreamCreate(&graphicsStream);
cudaStreamCreate(&aiStream);
nvtxNameCudaStreamA(graphicsStream, "Graphics");
nvtxNameCudaStreamA(aiStream, "AI");
```

# How do I use NVTX in my code?

For C and C++, NVTX is a header-only library with no dependencies.  Simply #include the header(s) you want to use, and call NVTX functions!  NVTX initializes automatically during the first call to any NVTX function.

It is not necessary to link against a binary library.  On POSIX platforms, adding the `-ldl` option to the linker command-line is required.

_NOTE:_ Older versions of NVTX did require linking against a dynamic library.  NVTX version 3 provides the same API, but removes the need to link with any library.  Ensure you are including NVTX v3 by using the `nvtx3` directory as a prefix in your #includes:
**C**:
```c
#include <nvtx3/nvToolsExt.h>
```
**C++**:
```c++
#include <nvtx3/nvtx3.hpp>
```

Since the C and C++ APIs are header-only, dependency-free, and don't require explicit initialization, they are suitable for annotating other header-only libraries.  Libraries using different versions of the NVTX headers in the same translation unit or different translation units will not have conflicts, as long as best practices are followed.

# Use NVTX with CMake
For projects that use CMake, the included `CMakeLists.txt` provides targets `nvtx3-c` and `nvtx3-cpp` that set the include search paths and the `-ldl` linker option where required.

## Use a local copy of NVTX

Suppose your project layout looks like the following:
```
    CMakeLists.txt
    imports/
        CMakeLists.txt
        Other 3rd party libraries here...
        NVTX/   (a copy of this directory from github)
            CMakeLists.txt
            include/
                nvtx3/
                    (all NVTX v3 headers here)
    source/
        CMakeLists.txt
        main.cpp
```
The root `CMakeLists.txt` file contains:
```cmake
    add_subdirectory(imports)
    add_subdirectory(source)
```
The `imports/CMakeLists.txt` file contains:
```cmake
    add_subdirectory(NVTX)
    add_subdirectory(...)   # Other imported libraries
```
The `source/CMakeLists.txt` file can now use CMake targets defined by NVTX:
```cmake
    add_executable(my_program main.cpp)
    target_link_libraries(my_program PRIVATE nvtx3-cpp)
```

## Use CMake Package Manager (CPM)

[CMake Package Manager (CPM)](https://github.com/cpm-cmake/CPM.cmake) is a utility that automatically downloads dependencies when CMake first runs on a project.  Since NVTX v3 is just a few headers, the download will be fast.  The downloaded files can be stored in an external cache directory to avoid redownloading during clean builds, and to enable offline builds.  First, download `CPM.cmake` from CPM's repo and save it in your project.  Then you can fetch NVTX directly from GitHub with CMake code like this (CMake 3.14 or greater is required):

```cmake
include(path/to/CPM.cmake)

CPMAddPackage(
    NAME NVTX
    GITHUB_REPOSITORY NVIDIA/NVTX
    GIT_TAG release-v3
    SOURCE_SUBDIR c
    )

add_executable(some_c_program main.c)
target_link_libraries(some_c_program PRIVATE nvtx3-c)

add_executable(some_cpp_program main.cpp)
target_link_libraries(some_cpp_program PRIVATE nvtx3-cpp)
```

# C/C++ versions and compilers

## C

The NVTX C API is a header-only library, implemented using **standard C89**.  The headers can be compiled with `-std=gnu90` or newer using many common compilers.  Tested compilers include:
- GNU gcc
- clang
- Microsoft Visual C++
- NVIDIA nvcc

C89 support in these compilers has not changed in many years, so even very old compiler versions should work.

## C++

The NVTX C++ API is a header-only library, implemented as a wrapper over the NVTX C API, using **standard C++11**.  The C++ headers are provided alongside the C headers.  NVTX C++ is implemented , and can be compiled with `-std=c++11` or newer using many common compilers.  Tested compilers include:
- GNU g++ (4.8.5 to 11.1)
- clang (3.5.2 to 12.0)
- Microsoft Visual C++ (VS 2015 to VS 2022)
    - On VS 2017.7 and newer, NVTX enables better error message output
- NVIDIA nvcc (CUDA 7.0 and newer)
