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

![Example NVTX Ranges in Nsight Systems](https://raw.githubusercontent.com/NVIDIA/NVTX/release-v3/docs/images/example_range.png "Example NVTX Ranges in Nsight Systems")

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

Prefer `nvtx3::scoped_range` in normal C++ code. If a range begins and ends in
different callback scopes on the same thread, use the manual C++ push/pop
wrappers:

```c++
void begin_callback()
{
    nvtx3::push_range("This is a push/pop range");
}

void end_callback()
{
    nvtx3::pop_range();
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

## Thread safety

NVTX is thread safe.
All NVTX functions can be called concurrently, including initialization, both for C and C++.

### Using sanitizers

Sanitizers may report conflicts in `nvtxImplCore.h` / `nvtxInitDefs.h`.  This is due to optimizations to avoid memory barriers in the hot path.  The implementation ensures that all race outcomes lead to the same result.  The race condition is benign: the worst case is seeing an old initialization function pointer, which will simply trigger re-initialization that immediately detects completion.

### Implementing tools

Tools should be implemented in a thread-safe way.  They should assume that any function may be called concurrently.  Setting callback function pointers should be done atomically, but relaxed memory consistency is sufficient for correctness.

# How do I use NVTX in my code?

For C and C++, NVTX is a header-only library with no dependencies.  Simply #include the header(s) you want to use, and call NVTX functions!  NVTX initializes automatically during the first call to any NVTX function.

It is not necessary to link against a binary library or add any link-time parameters.  On older POSIX platforms with glibc versions prior to 2.34, adding the `-ldl` option to the linker command is required.

_NOTE:_ Older versions of NVTX did require linking against a dynamic library.  NVTX version 3 provides the same API, but removes the need to link with any library.  Ensure you are including NVTX v3 by using the `nvtx3` directory as a prefix in your #includes:
**C**:
```c
#include <nvtx3/nvToolsExt.h>
```
**C++**:
```c++
#include <nvtx3/nvtx3.hpp>
```

Since the C and C++ APIs are header-only, dependency-free, and don't require explicit initialization, they are suitable for annotating other header-only libraries.

# Use NVTX with CMake

For projects that use CMake, the included `CMakeLists.txt` provides targets `nvtx3-c` and `nvtx3-cpp` that set the include search paths (and add the `-ldl` linker option if applicable).  It also provides the `nvtxw3-loader` target described in [NVTXW writer loader](#nvtxw-writer-loader).

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

# NVTXW writer loader

The NVTXW ("NVTX Writer") API in `nvtxw3/nvtxw3.h` accepts payload, counter, and timing data that was produced *outside* the live NVTX injection path.  The actual writing is performed by a separate backend library (for example `libnvtxw3.so` / `nvtxw3.dll`, typically provided by a tool such as Nsight Systems), which is located and loaded at runtime.

The headers are split by responsibility: `nvtxw3/nvtxw3.h` is the focused producer/backend contract (result codes, the single `nvtxwGetInterface` entry point, and the `nvtxwInterface_v2_t` function table), while `nvtxw3/nvtxw3_loader.h` holds the optional reference loader (`nvtxwLoad`, `nvtxwUnload`, `nvtxwGetError`, and the config-string utility for session configuration).  Including `nvtxw3_loader.h` transitively includes `nvtxw3.h`.

`nvtxwLoad` takes a single `library` argument and resolves the backend in priority order: the `NVTXW3_LIBRARY` environment variable (if set to a non-empty value), then the `library` argument (a filename or path) if non-NULL, and finally — when `library` is NULL — a default search of the executable directory, the standard dynamic-library search paths, and the current working directory.  The `NVTXW3_LIBRARY` override lets a tool or launcher redirect the backend without rebuilding the instrumented application; when it (or the `library` argument) names a specific library, that library is the only candidate and its failure is reported rather than falling back.

Unlike the rest of the NVTX C/C++ API, the loader is **not** header-only: it has a compiled translation unit (`src/nvtxw3_loader.c`) that defines `nvtxwLoad` and `nvtxwUnload`.  The included `CMakeLists.txt` builds this into a small static library exposed as the `nvtxw3-loader` target (alias `nvtx3::nvtxw3-loader`).  It links `nvtx3-c` transitively, so it also brings in the NVTX include paths and the platform's dynamic-loader library (`dlopen`/`dlsym`).

The loader in `src/nvtxw3_loader.c` is a **reference implementation** and **not** required.  It is provided as a convenient, ready-to-use way to locate and load an NVTXW backend library, but applications are free to ignore it and load the backend themselves.  All that is required to use the NVTXW API is to obtain a backend library, resolve its exported `nvtxwGetInterface` symbol, and call it with the desired interface version (e.g. `NVTXW_INTERFACE_VERSION`) to get the matching `nvtxwInterface_*` function table — by whatever loading mechanism best fits the application.  Use the provided loader, adapt it, or replace it as needed.

There is **no** separate `nvtx3::nvtxw3` target: the NVTXW headers are header-only and already provided by `nvtx3::nvtx3-c` (its include path covers both `nvtx3/` and `nvtxw3/`).  To use NVTXW without the loader, link `nvtx3::nvtx3-c` and load the backend yourself; the optional loader is the only compiled piece, exposed as `nvtx3::nvtxw3-loader`.

Note that `nvtxw3-loader` is the loader you link into your application; it is distinct from the runtime backend library it loads.

Link it like any other NVTX target:
```cmake
add_executable(my_writer main.c)
target_link_libraries(my_writer PRIVATE nvtx3::nvtxw3-loader)
```

If you are not using CMake, simply compile `src/nvtxw3_loader.c` as part of your build with the `include/` directory on the header search path (and link the dynamic-loader library, e.g. `-ldl`, where required).

# C/C++ versions and compilers

## C

The NVTX C API is a header-only library, implemented using **standard C89**.  The headers can be compiled with `-std=gnu90` or newer using many common compilers.  Tested compilers include:
- GNU gcc
- clang
- Microsoft Visual C++
- NVIDIA nvcc

C89 support in these compilers has not changed in many years, so even very old compiler versions should work.

### C version compatibility notes

Using different versions of the NVTX headers in the same translation unit or different translation units is supported, as long as best practices are followed.

### Payload helper macros and MSVC

The convenience macros in `nvToolsExtPayloadHelper.h` (`NVTX_DEFINE_SCHEMA_FOR_STRUCT`, `NVTX_DEFINE_STRUCT_WITH_SCHEMA`, `NVTX_DEFINE_STRUCT_WITH_SCHEMA_AND_REGISTER`, `NVTX_DEFINE_SCHEMA_FOR_STRUCT_AND_REGISTER`, and `NVTX_DEFINE_STRUCT`) rely on variadic macro argument counting, which requires a standards-conforming preprocessor.  Microsoft Visual C++'s traditional preprocessor does not expand `__VA_ARGS__` correctly for these patterns.

To use these macros with MSVC, enable the conforming preprocessor:
- **Visual Studio 2019 and newer:** `/Zc:preprocessor`
- **Visual Studio 2017 (v15.5+):** `/experimental:preprocessor`

Visual Studio versions older than 2017 do not support the conforming preprocessor and cannot use these macros.  GCC, Clang, and other compilers with conforming preprocessors work without any additional flags.

## C++

The NVTX C++ API is a header-only library, implemented as a wrapper over the NVTX C API, using **standard C++11**.  The C++ headers are provided alongside the C headers.  NVTX C++ is implemented , and can be compiled with `-std=c++11` or newer using many common compilers.  Tested compilers include:
- GNU g++ (4.8.5 to 11.1)
- clang (3.5.2 to 12.0)
- Microsoft Visual C++ (VS 2015 to VS 2022)
    - On VS 2017.7 and newer, NVTX enables better error message output
- NVIDIA nvcc (CUDA 7.0 and newer)

### C++ version compatibility notes

Minor versions of NVTX releases may introduce new features into the `nvtx3::v1` namespace.
To use these features, ensure that within each compilation unit, the first inclusion of `nvtx3.hpp` is based at least on the latest required release.
If an older version is included first, the new features will not be available.

It is supported to link together multiple minor versions of NVTX in different objects.

For maximum compatibility in header-only libraries or other scenarios with complex NVTX dependencies, use symbols of a specific major version, e.g. `nvtx3::v1::domain`.

### NVTX3_DEFINE_SCHEMA_GET and MSVC

The `NVTX3_DEFINE_SCHEMA_GET` macro (in `nvtx3.hpp`) internally uses the C payload helper macros, which rely on variadic macro argument counting.  The same MSVC preprocessor requirement described [above](#payload-helper-macros-and-msvc) applies: enable `/Zc:preprocessor` (VS 2019+) or `/experimental:preprocessor` (VS 2017 v15.5+).

### C++ version history

- v3.3: Add `payload_data` wrapper for `nvtxPayloadData_t` in support of extended payloads.
