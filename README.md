# NVTX (NVIDIA Tools Extension Library)

NVTX is a cross-platform API for annotating source code to provide contextual information to developer tools.

The NVTX API is written in C, with wrappers provided for C++ and Python.

# What does NVTX do?

By default, NVTX API calls do _nothing_.  When you launch a program from a developer tool, NVTX calls in that program are redirected to functions in the tool.  Developer tools are free to implement NVTX API calls however they wish.

Here are some examples of what a tool might do with NVTX calls:
- Print a message to the console
- Record a trace of when NVTX calls occur, and display them on a timeline
- Build a statistical profile of NVTX calls, or time spent in ranges between calls
- Enable/disable tool features in ranges bounded by NVTX calls matching some criteria
- Forward the data to other logging APIs or event systems

# Example: Visualize loop iterations on a timeline

This C++ example annotates `some_function` with an NVTX range using the function's name.  This range begins at the top of the function body, and automatically ends when the function returns.  The function performs a loop, sleeping for one second in each iteration.  A local `nvtx3::scoped_range` annotates the scope of the loop body.  The loop iteration ranges are nested within the function range.

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

Launch it from [NVIDIA Nsight Systems](https://developer.nvidia.com/nsight-systems), and you'll see this execution on a timeline:

![Example NVTX Ranges in Nsight Systems](https://raw.githubusercontent.com/NVIDIA/NVTX/release-v3/docs/images/example_range.png "Example NVTX Ranges in Nsight Systems")

The NVTX row shows the function's name "some_function" in the top-level range and the "loop range" message in the nested ranges.  The loop iterations each last for the expected one second.

# What kinds of annotation does NVTX provide?

## Markers
**Markers** annotate a specific point in a program's execution with a message.  Optional extra fields may be provided: a category, a color, and a payload value.

## Ranges
**Ranges** annotate a range between two points in a program's execution, like a related pair of markers.  There are two types of ranges:
- Push/Pop ranges, which can be nested to form a stack
    - The Pop call is automatically associated with a prior Push call on the same thread
- Start/End ranges, which may overlap with other ranges arbitrarily
    - The Start call returns a handle which must be passed to the End call
    - These ranges can start and end on different threads

The C++ and Python interfaces provide objects and decorators for automatically managing the lifetimes of ranges.

## Resource naming/tracking
**Resource naming** associates a displayable name string with an object.  For example, naming CPU threads allows a tool that displays thread activity on a timeline to have more meaningful labels for its rows than a numeric thread ID.

**Resource tracking** extends the idea of naming to include object lifetime tracking, as well as important usage of the object.  For example, a mutex provided by platform API (e.g. pthread_mutex, CriticalSection) can be tracked by a tool that intercepts its lock/unlock API calls, so using NVTX to name these mutex objects would be sufficient to see the names of mutexes being locked/unlocked on a timeline.  However, manually implemented spin-locks may not have an interceptible API, so tools can't automatically detect when they are used.  Use NVTX to annotate these types of mutexes where they are locked/unlocked to enable tools to track them just like standard platform API mutexes.

# How do I use NVTX in my code?

## C and C++

For C and C++, NVTX is a header-only library with no dependencies.  Simply #include the header(s) you want to use, and call NVTX functions!  NVTX initializes automatically during the first call to any NVTX function.

It is not necessary to link against a binary library.  On POSIX platforms, adding the `-ldl` option to the linker command-line is required.

_NOTE:_ Older versions of NVTX did require linking against a dynamic library.  NVTX version 3 provides the same API, but removes the need to link with any library.  Ensure you are including NVTX v3 by using the `nvtx3` directory as a prefix in your #includes:

**C**:
```c
#include <nvtx3/nvToolsExt.h>

void example()
{
    nvtxMark("Hello world!");
}
```
**C++**:
```c++
#include <nvtx3/nvtx3.hpp>

void example()
{
    nvtx3::mark("Hello world!");
}
```

The NVTX C++ API is a set of wrappers around the C API, so the C API functions are usable from C++ as well.

Since the C and C++ APIs are header-only, dependency-free, and don't require explicit initialization, they are suitable for annotating other header-only libraries.  Libraries using different versions of the NVTX headers in the same translation unit or different translation units will not have conflicts, as long as best practices are followed.

See more details in [the `c` directory](/c) of this repo, and in the API reference guides:
- [NVTX C API Reference](https://nvidia.github.io/NVTX/doxygen/index.html)
- [NVTX C++ API Reference](https://nvidia.github.io/NVTX/doxygen-cpp/index.html)

### CMake

For projects that use CMake, the CMake scripts included with NVTX provide targets `nvtx3-c` and `nvtx3-cpp`.  Use `target_link_libraries` to make any CMake target use `nvtx3-c` for the C API only and `nvtx3-cpp` for both the C and C++ APIs.  Since NVTX is a header-only library, these targets simply add the include search path for the NVTX headers and add the `-ldl` linker option where required.  Example usage:

```cmake
# Example C program
add_executable(some_c_program main.c)
target_link_libraries(some_c_program PRIVATE nvtx3-c)
# main.c can now do #include <nvtx3/nvToolsExt.h>

# Example C++ program
add_executable(some_cpp_program main.cpp)
target_link_libraries(some_cpp_program PRIVATE nvtx3-cpp)
# main.cpp can now do #include <nvtx3/nvtx3.hpp>
```

NVTX provides two different ways to define the CMake targets:

#### Normal CMake targets (non-IMPORTED)

Non-IMPORTED targets are global to the entire build.  In a typical CMake codebase, `add_subdirectory` is used to include every directory in a source tree, where each contains a `CMakeLists.txt` file that defines targets usable anywhere in the build.  The NVTX `CMakeLists.txt` file defines normal (non-IMPORTED) targets when `add_subdirectory` is called on that directory.

This example code layout has a few imported third-party libraries and a separate directory for its own source.  It shows that adding the NVTX directory to CMake allows the `nvtx3-cpp` to be used elsewhere in the source tree:

- CMakeLists.txt
    ```cmake
    add_subdirectory(Imports)
    add_subdirectory(Source)
    ```
- Imports/
    - CMakeLists.txt
        ```cmake
        add_subdirectory(SomeLibrary)
        add_subdirectory(NVTX)
        add_subdirectory(SomeOtherLibrary)
        ```
    - SomeLibrary/
    - NVTX/ *(this is the downloaded copy of NVTX)*
        - CMakeLists.txt  *(defines* `nvtx3-c` *and* `nvtx3-cpp` *targets)*
        - nvtxImportedTargets.cmake *(helper script)*
        - include/
            - nvtx3/ *(all NVTX headers)*
    - SomeOtherLibrary/
- Source/
    - CMakeLists.txt
        ```cmake
        add_executable(my_program main.cpp)
        target_link_libraries(my_program PRIVATE nvtx3-cpp)
        ```
    - main.cpp *(does* `#include <nvtx3/nvtx3.hpp>`*)*

Another example is when the NVTX directory must be added with a relative path that is not a subdirectory.  In this case, CMake requires a second parameter to `add_subdirectory` to give a unique name for the directory where build output goes:
- Utils/
    - SomeLibrary/
    - NVTX/ *(this is the downloaded copy of NVTX)*
        - CMakeLists.txt  *(defines* `nvtx3-c` *and* `nvtx3-cpp` *targets)*
        - nvtxImportedTargets.cmake *(helper script)*
        - include/
            - nvtx3/ *(all NVTX headers)*
    - SomeOtherLibrary/
- Project1/
- Project2/
- Project3/
    - CMakeLists.txt
        ```cmake
        add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../Utils/NVTX" "ImportNVTX")

        add_executable(my_program main.cpp)
        target_link_libraries(my_program PRIVATE nvtx3-cpp)
        ```
    - main.cpp *(does* `#include <nvtx3/nvtx3.hpp>`*)*

When defining normal (non-IMPORTED) targets, the NVTX CMake scripts avoid target-already-defined errors by checking if the targets exist before attempting to define them.  This enables the following scenarios:
- The same NVTX directory can be added more than once
- Multiple directories with copies of the same NVTX version can be added
- Multiple directories different versions of NVTX can be added
    - If newest version is added first, everything should work:
        - The `nvtx3-c`/`nvtx3-cpp` targets will point to the newest version
    - If a new version is added after an old version:
        - The `nvtx3-c`/`nvtx3-cpp` targets will point to an old version
        - If features of the newest version are used, compilation will fail
        - The NVTX CMake scripts print a warning for this case

Normal (non-IMPORTED) targets will be defined when using CPM (CMake Package Manager) to fetch NVTX directly from the internet.  Thus, NVTX targets defined via CPM follow the behavior described above.  This example shows usage of CPM instead of a local copy of NVTX:

- Source/
    - CMakeLists.txt
        ```cmake
        include(path/to/CPM.cmake)

        CPMAddPackage(
            NAME NVTX
            GITHUB_REPOSITORY NVIDIA/NVTX
            GIT_TAG v3.1.0-c-cpp
            GIT_SHALLOW TRUE)

        add_executable(my_program main.cpp)
        target_link_libraries(my_program PRIVATE nvtx3-cpp)
        ```
    - main.cpp *(does* `#include <nvtx3/nvtx3.hpp>`*)*

See CPM section below in "How do I get NVTX?" for more details.

#### IMPORTED CMake targets

`IMPORTED` targets are scoped to the directory where they are defined.  These are useful when defining targets for dependencies of a `SHARED` or `STATIC` library, because they won't conflict with targets of the same name elsewhere in the build.  This lets a library ensure it is using the expected version of its own dependencies without imposing that version on other parts of the build.  NVTX provides the `nvtxImportedTargets.cmake` script to define the targets `nvtx3-c` and `nvtx3-cpp` as `IMPORTED`.  Use `include("path/to/nvtxImportedTargets.cmake")` from any CMakeLists.txt file to define the NVTX targets with scope locally to that directory.

This example shows a program that imports multiple third-party libraries, which each import their own copy of NVTX:

- CMakeLists.txt
    ```cmake
    add_subdirectory(Imports)
    add_subdirectory(Source)
    ```
- Imports/
    - CMakeLists.txt
        ```cmake
        add_subdirectory(Foo)
        add_subdirectory(Bar)
        ```
    - Foo/
        - CMakeLists.txt
            ```cmake
            include(Detail/NVTX/nvtxImportedTargets.cmake)
            add_library(foo STATIC foo.cpp)
            target_link_libraries(foo PRIVATE nvtx3-cpp)
            ```
        - Detail/
            - NVTX/ *(downloaded copy of NVTX, ***version 3.1***)*
        - foo.cpp  *(does* `#include <nvtx3/nvtx3.hpp>`*)*
    - Bar/
        - CMakeLists.txt
            ```cmake
            include(Detail/NVTX/nvtxImportedTargets.cmake)
            add_library(bar SHARED bar.cpp)
            target_link_libraries(bar PRIVATE nvtx3-cpp)
            ```
        - Detail/
            - NVTX/ *(downloaded copy of NVTX, ***version 3.2***)*
        - bar.cpp  *(does* `#include <nvtx3/nvtx3.hpp>`*)*
- Source/
    - CMakeLists.txt
        ```cmake
        add_executable(my_program main.cpp)
        target_link_libraries(my_program PRIVATE foo bar)
        ```

Note that in this example, Foo uses an older version of NVTX than Bar, and Foo is added before Bar.  Since the NVTX CMake target definitions are local within the Foo and Bar directories, both libraries will use their own copies.  Bar can safely use NVTX version 3.2 features, even though Foo used version 3.1 earlier.  There will be no warnings printed that an older NVTX version was added before a newer one, unlike the case with global (non-IMPORTED) target definitions (see above). 

## Python

Install NVTX for Python using `pip` or `conda`, and use `import nvtx` in your code:
```python
import nvtx

nvtx.mark(message="Hello world!")
```

See more details in [the `python` directory](/python) in this repo.

# How do I get NVTX?

## C/C++
### Get NVTX from GitHub

The C/C++ NVTX headers are provided by the NVIDIA NVTX GitHub repo, in the `c` directory.  This is the most up-to-date copy of NVTX.  Copying that directory into your codebase is sufficient to use NVTX.

The `release-v3` branch is the version officially supported by NVIDIA tools.  Other branches may incur breaking changes at any time and are not recommended for use in production code.  The `release-v3-c-cpp` branch is maintained as a copy of the `c` directory from `release-v3`, so downloading `release-v3-c-cpp` is a lightweight way to get all that is needed to build C/C++ programs with NVTX.

### Get NVTX with NVIDIA Developer Tools

Some NVIDIA developer tools include the NVTX v3 library as part of the installation.  See the documentation of the tools for details about where the NVTX headers are installed.

### Get NVTX with the CUDA Toolkit

The CUDA toolkit provides NVTX v3.

Note that the toolkit may also include older versions for backwards compatibility, so be sure to use version 3 (the `nvtx3` subdirectory of headers) for best performance, convenience, and support.  Use `#include <nvtx3/nvToolsExt.h>` instead of `#include <nvToolsExt.h>` to ensure code is including v3!

### Get NVTX using CMake Package Manager (CPM)

[CMake Package Manager (CPM)](https://github.com/cpm-cmake/CPM.cmake) is a utility that automatically downloads dependencies when CMake first runs on a project, and adds their `CMakeLists.txt` to the build.  The downloaded files can be stored in an external cache directory to avoid redownloading during clean builds, and to enable offline builds once the cache is populated.

To use CPM, download `CPM.cmake` from [CPM's repo](https://github.com/cpm-cmake/CPM.cmake/blob/master/cmake/CPM.cmake) and save it in your project.  Then you can fetch NVTX directly from GitHub with CMake code like this (CMake 3.14 or greater is required):

```cmake
include(path/to/CPM.cmake)

CPMAddPackage(
    NAME NVTX
    GITHUB_REPOSITORY NVIDIA/NVTX
    GIT_TAG v3.1.0-c-cpp
    GIT_SHALLOW TRUE)

# Example C program
add_executable(some_c_program main.c)
target_link_libraries(some_c_program PRIVATE nvtx3-c)

# Example C++ program
add_executable(some_cpp_program main.cpp)
target_link_libraries(some_cpp_program PRIVATE nvtx3-cpp)
```

Note that this downloads from GitHub using a version tag with the suffix `-c-cpp`.  The `v3.x.y` tags points to the `release-v3` branch, which contains the entire repo.  The `v3.x.y-c-cpp` tags point to a separate branch called `release-v3-c-cpp` containing only the `c` directory of the repo, which is the bare minimum needed to use NVTX in a C or C++ project.  If you specify `GIT_TAG v3.x.y` to download the full repo, the `SOURCE_SUBDIR c` option is also needed to tell CMake where `CMakeLists.txt` is in the downloaded repo.  Also, avoid downloading the full history of the repo by using `GIT_SHALLOW TRUE` to download only the requested version.

## Python
### Get NVTX using Conda
```
conda install -c conda-forge nvtx
```
### Get NVTX using PIP
```
python3 -m pip install nvtx
```

# What tools support NVTX?

These NVIDIA tools provide built-in support for NVTX:

- [Nsight Systems](https://developer.nvidia.com/nsight-systems) logs NVTX calls and shows them on a timeline alongside driver/OS/hardware events
- [Nsight Compute](https://developer.nvidia.com/nsight-compute) uses NVTX ranges to focus where deep-dive GPU performance analysis occurs
- [Nsight Graphics](https://developer.nvidia.com/nsight-graphics) uses NVTX ranges to set bounds for range profiling in the Frame Debugger
- The [CUPTI](https://developer.nvidia.com/cupti) API supports recording traces of NVTX calls

Other tools may provide NVTX support as well -- see the tool documentation for details.

# Which platforms does NVTX support?

NVTX was designed to work on:
- Windows
- Linux and other POSIX-like platforms (including cygwin)
- Android

Both 64-bit and 32-bit processes are supported.  There are no restrictions on CPU architecture.

NVTX *may* work on other POSIX-like platforms, but support is not guaranteed.  NVTX relies on the platform's standard API to load a dynamic library (.dll) or shared object (.so).  Platforms without dynamic library functionality cannot support NVTX.

NVTX is _not_ supported in GPU code, such as `__device__` functions in CUDA.  While NVTX for GPU may intuitively seem useful, keep in mind that GPUs are best utilized with thousands or millions of threads running the same function in parallel.  A tool tracing ranges in every thread would produce an unreasonably large amount of data, and would incur large performance overhead to manage this data.  Efficient instrumentation of CUDA GPU code is possible with the [pmevent](https://docs.nvidia.com/cuda/parallel-thread-execution/index.html#miscellaneous-instructions-pmevent) PTX instruction, which can be monitored by hardware performance counters with no overhead.

See the documentation for individual tools to see which platforms they support.

# Which languages/compilers does NVTX support?

## C

The NVTX C API is a header-only library, implemented using **standard C89/C90**.  The headers can be compiled with `-std=gnu90` or newer using many common compilers.  Tested compilers include:
- GNU gcc
- clang
- Microsoft Visual C++
- NVIDIA nvcc

C89 support in these compilers has not changed in many years, so even very old compiler versions should work.

See more details in [the `c` directory](/c) of this repo, and the [NVTX C API Reference](https://nvidia.github.io/NVTX/doxygen/index.html).

## C++

The NVTX C++ API is a header-only library, implemented as a wrapper over the NVTX C API, using **standard C++11**.  The C++ headers are provided alongside the C headers.  NVTX C++ is implemented , and can be compiled with `-std=c++11` or newer using many common compilers.  Tested compilers include:
- GNU g++ (4.8.5 to 11.1)
- clang (3.5.2 to 12.0)
- Microsoft Visual C++ (VS 2015 to VS 2022)
    - On VS 2017.7 and newer, NVTX enables better error message output
- NVIDIA nvcc (CUDA 7.0 and newer)

See more details in [the `c` directory](/c) of this repo, and the [NVTX C++ API Reference](https://nvidia.github.io/NVTX/doxygen-cpp/index.html).

## Python

The NVTX Python API provides native Python wrappers for a subset of the NVTX C API.  NVTX Python requires **Python 3.6 or newer**.  It has been tested on Linux, with Python 3.6 to 3.9.

See more details in [the `python` directory](/python) of this repo.

## Other languages

Any language that can call into C with normal calling conventions can work with the NVTX C API.  There are two general approaches to implement NVTX wrappers in other languages:

1. Write C code that #includes and exposes NVTX functionality through a language binding interface.  Since the NVTX C API uses pointers and unions, wrappers for other languages may benefit from a more idiomatic API for ease of use.  NVTX for Python uses this approach, based on Cython.
2. Make a dynamic library that exports the NVTX C API directly, and use C interop bindings from the other language to call into this dynamic library.  To create a dynamic library from the NVTX v3 C headers, simply compile this .c file as a dynamic library:
```c
    #define NVTX_EXPORT_API
    #include <nvtx3/nvToolsExt.h>
    // #include any other desired NVTX C API headers here to export them
```
Older versions of NVTX distributed a dynamic library with C API exported.  Projects depending on that library can use the code above to recreate a compatible library from NVTX v3.

_NOTE:_ Official Fortran support coming soon!

# How much overhead does NVTX add to my code?

The first call to any NVTX API function in a process will trigger initialization of the library.  The implementation checks an environment variable to see if a tool wishes to intercept the NVTX calls.

When no tool is present, initialization disables all the NVTX API functions.  Subsequent NVTX API calls are a handful of instructions in a likely-inlined function to jump over the disabled call.

When a tool is present, initialization configures the NVTX API so all subsequent calls jump directly into that tool's implementation.  Overhead in this case is entirely determined by what the tool does.

The first NVTX call can incur significant overhead while loading and initializing the tool.  If this first call happens in a latency-sensitive part of the program (e.g. a game with low frame-rate detection), it may cause the program to behave differently with the tool vs. without the tool.  The `nvtxInitialize` C API function is provided for this situation, to allow force-initializing NVTX at a convenient time, without any other contextual meaning like a marker.  It is not necessary to use `nvtxInitialize` in other cases.

# How do I disable all NVTX calls at compile-time?

Providing non-public information to tools via NVTX is helpful in internal builds, but may not be acceptable for public release builds.  The entire NVTX C and C++ APIs can be preprocessed out with a single macro before including any NVTX headers:
```c
#define NVTX_DISABLE
```
Or add `-DNVTX_DISABLE` to the compiler command line, only in the configuration for public builds.  This avoids having to manually add `#if`s around NVTX calls solely for the purpose of disabling all of them in specific build configurations.

# General Usage Guidelines

## Add ranges around important sections of code

Developer tools often show low-level information about what the hardware or operating system is doing, but without correlation to the high-level structure of your program.  Annotate sections of your code with NVTX ranges to add contextual information, so the information reported by tools can be extended to show where in your program the low-level events occur.  This also enables some tools to target only these important parts of your program, and to choose which parts to target in the tool options -- no need to recompile your code to target other sections!

## Give, don't take

NVTX is primarily a *one-way* API.  Your program gives information to the tool, but it does not get actionable information back from the tool.  Some NVTX functions return values, but these should only be used as inputs to other NVTX functions.  Programs should not behave differently based on these values, because it is important that tools can see programs behaving the same way they would without any tools present!

## Avoid depending on any particular tool

Do not use NVTX for any functionality that is required for your program to work correctly.  If a program depends on a particular tool being present to work, then it would be impossible to use any *other* NVTX tools with this program.  NVTX does not currently support multiple tools being attached to the same program.

## Isolate NVTX annotations in a library using a Domain

It is possible for a program to use many libraries, all of which include NVTX annotations.  When running such a program in a tool, it is helpful if the user can keep these libraries' annotations separate.  A library should isolate its annotations from other libraries by creating a "domain", and performing all marker/range/naming annotations within that domain.  Tools can provide options for which domains to enable, and use domains to group annotation data by library.

The domain also acts as a namespace:  Different domains may use the same hard-coded values for category IDs without conflict.  The NVTX C++ API provides initialize-on-first-use for domains to avoid the need for up-front initialization.

## Use categories to organize annotations

While domains are meant to keep the annotations from different libraries separate, it may be useful within a library to have separate categories for annotations.  NVTX markers and ranges provide a "category ID" field for this purpose.  This integer may be hard-coded, like an `enum` in C/C++.  NVTX provides API functions to name to a category ID value, so tools can display meaningful names for categories.  Tools are encouraged to logically group annotations into categories.  Using slashes in category names like filesystem paths allows the user to create a hierarchy of categories, and tools should handle these as a hierarchy.

## Avoid slow processing to prepare arguments for NVTX calls

When tools are not present, the first NVTX call quickly configures the API to make all subsequent NVTX calls into no-ops.  However, any processing done before making an NVTX call to prepare the arguments for the call is not disabled.  Using a function like `sprintf` to generate a message string dynamically for each call will add overhead even in the case when no tool is present!  Instead of generating message strings, is more efficient to pass a hard-coded string for the message, and variable as a _payload_.

## Register strings that will be used many times

In each NVTX marker or range, tools may copy the message string into a log file, or test the string (e.g. with a regex) to see if it matches some criteria for triggering other functionality.  If the same message string is used repeatedly, this work in the tool would be redundant.  To reduce the tool overhead and help keep log files smaller, NVTX provides functions to "register" a message string.  These functions return a handle that can be used in markers and ranges in place of a message string.  This allows tools to log or test message strings just once, when they are registered.  Logs will be smaller when storing handle values instead of large strings, and string tests reduce to lookup of precomputed answers. The `NVTX3_FUNC_RANGE` macros, for example, register the function's name and save the handle in a local static variable for efficient reuse in subsequent calls to that function.  Some tools may require using registered strings for overhead-sensitive functionality, such as using NVTX ranges to start/stop data collection in Nsight Systems.