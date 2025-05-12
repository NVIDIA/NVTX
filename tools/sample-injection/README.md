# NVTX Sample Injection

[NVTX](https://github.com/NVIDIA/NVTX/) annotations are no-op instructions unless a profiling
tool is activated to collect them. [Nsight Systems](https://developer.nvidia.com/nsight-systems)/[Compute](https://developer.nvidia.com/nsight-compute)/[Graphics](https://developer.nvidia.com/nsight-graphics) are typically used for this.
While these packages provide a fast and reliable (and generally recommended) method for
collecting NVTX data, there may be situations where a custom implementation is more suitable.

This repository contains a code sample, which is a partial, but representative, NVTX injection
that shows how to implement library functions, how to make them available in runtime, and lists the
most important code points/structures from the library. It also includes annotated app examples,
both native and Python-based, that can be launched on Linux and Windows.
The implementation is focused exclusively on NVTX and doesn't cover some general programming best
practices, like code structuring or compiler flags, it also uses a very basic approach for
multithreading. So, while the sample is technically correct and is ready to be used "as is",
some of its methods might require reconsideration base on specific needs.

# Key points
NVTX expects the injection to be provided via a dynamic library, path to the library file
should be set in the `NVTX_INJECTION64_PATH` env variable prior to calling an NVTX function.
The `NVTX_INJECTION32_PATH` variable can also be specified (together with its 64-bit counterpart),
so that if the process tree contains a 32-bit process, the appropriate library will be picked up.
Then, the `InitializeInjectionNvtx2(NvtxGetExportTableFunc_t)` function
is called to make NVTX aware of the custom function implementations.

These functions are stored in callback tables, which are retrieved by the `NvtxGetExportTableFunc_t`
argument and filled with function references. (See the `InitializeInjectionNvtx2` implementation
in [NvtxSampleInjection.cpp](Source/NvtxSampleInjection.cpp).) It is important to assign callbacks
into the proper table using the corresponding indices, e.g. for the `NVTX_CB_MODULE_CORE2` table,
`NVTX_CBID_CORE2_*` index constants must be used.

Also, be wary when using wrappers (like the [Python one](https://github.com/NVIDIA/NVTX/tree/release-v3/python)), because they may hide the real callback usage. For example, the Python wrapper uses
[`nvtxDomainMarkEx(nvtxDomainHandle_t, const nvtxEventAttributes_t*)`](https://nvidia.github.io/NVTX/doxygen/group___m_a_r_k_e_r_s___a_n_d___r_a_n_g_e_s.html#ga9e31d7977bcd3b4e64da577908f20e70)
even when in the code it may be `nvtx.mark("Mark")` (more like
[`nvtxMarkA(const char*)`](https://nvidia.github.io/NVTX/doxygen/group___m_a_r_k_e_r_s___a_n_d___r_a_n_g_e_s.html#gaa8b4b68acc37bdaf14349b25752b26f9)).


# Dev
You will need a C++ compiler and [CMake](https://cmake.org/) to build the injection and the test app.
[Git](https://git-scm.com/) is also used to get NVTX headers. However, the only strict requirement is the compiler.
Everything else was used for convenience and can be replaced with other tools, depending on specific needs of your
project and the existing environment (e.g. one might want to use Makefiles and download NVTX headers manually).

## Setup
```sh
git clone --depth 1 --branch release-v3-c-cpp https://github.com/NVIDIA/NVTX.git Import/NVTX
```

## Build
### Linux/Windows
```sh
cmake -B Build -S .
cmake --build Build
```

## Run tests
### Compiled native (Linux)
```sh
NVTX_INJECTION64_PATH=$PWD/Build/libnvtx_sample_injection.so Build/test
```
Output:
```
[NVTX][303997][180777689] InitializeInjectionNvtx2()
[NVTX][303997][180777689] PUSH Test push/pop range
[NVTX][303997][180777689] MARK Test mark
[NVTX][303997][180778189] POP
[NVTX][303997][180778190] DOMAIN CREATE Domain #1
[NVTX][303997][180778190] MARK No name@Domain #1
[NVTX][303997][180778190] DOMAIN DESTROY Domain #1
```

### Compiled native (Windows)
```powershell
$env:NVTX_INJECTION64_PATH="$PWD\Build\Debug\nvtx_sample_injection.dll"
Build\Debug\test.exe
```

### Python (Linux)
```sh
NVTX_INJECTION64_PATH=$PWD/Build/libnvtx_sample_injection.so python Test/NvtxTest.py
```
