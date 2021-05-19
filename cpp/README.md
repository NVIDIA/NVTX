# NVTX++

Provides C++ convenience wrappers for NVIDIA Tool Extension (NVTX) C APIs. 

See https://jrhemstad.github.io/nvtx_wrappers/html/index.html for Doxygen documentation.

  # Quick Start
 
  To add NVTX ranges to your code, use the `nvtx3::thread_range` RAII object. A
  range begins when the object is created, and ends when the object is
  destroyed.
 
  ```c++
  #include "nvtx3.hpp"
  void some_function(){
     // Begins a NVTX range with the messsage "some_function"
     // The range ends when some_function() returns and `r` is destroyed
     nvtx3::thread_range r{"some_function"};
  
     for(int i = 0; i < 6; ++i){
        nvtx3::thread_range loop{"loop range"};
        std::this_thread::sleep_for(std::chrono::seconds{1});
     }
  } // Range ends when `r` is destroyed
  ```

  The example code above generates the following timeline view in Nsight Systems:

  ![alt text](https://raw.githubusercontent.com/jrhemstad/nvtx_wrappers/master/docs/example_range.png "Example NVTX Ranges")
  
  Alternatively, use the macro `NVTX3_FUNC_RANGE()` to add
  ranges to your code that automatically use the name of the enclosing function
  as the range's message.
 
 ```c++
 #include "nvtx3.hpp"
  void some_function(){
     // Creates a range with a message "some_function" that ends when the enclosing
     // function returns
     NVTX3_FUNC_RANGE();
     ...
  }
  ```

  ## Getting NVTX++

  The NVTX C++ wrappers are header-only and implemented in a single header file.
  This header can be incorporated manually into your project by downloading the headers and placing them in your source tree. 
  However, we strongly recommend using CMake or CMake Package Manager (CPM) to fetch and link NVTX++ to your project.

  ### Adding NVTX++ to a CMake Project

  NVTX++ is designed to make it easy to include within another CMake project. 
  The `CMakeLists.txt` exports a `nvtx3-cpp` target that can be linked into a target to setup include directories, dependencies, and compile flags necessary to use NVTX++ in your project.

  We recommend using [CMake Package Manager (CPM)](https://github.com/cpm-cmake/CPM.cmake#adding-cpm) to fetch NVTX++ into your project. With CPM, getting NVTX++ is easy:

  ```
  cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

  include(path/to/CPM.cmake) // You'll need to download this file from the CPM repo

  CPMAddPackage(
  NAME NVTX
  GITHUB_REPOSITORY NVIDIA/NVTX
  GIT_TAG dev
  SOURCE_SUBDIR cpp
  OPTIONS
     "BUILD_TESTS OFF"
     "BUILD_BENCHMARKS OFF"
)

target_link_libraries(my_library nvtx3-cpp)
```

You can then include `<nvtx3.hpp>` in your project files part of `my_library` and use the NVTX++ wrappers.


  ## Dependencies
  - NVTX C API headers (in the CUDA Toolkit, these are located in `cuda/include/nvtx3`)
     - If fetching NVTX++ via CMake, the NVTX C headers will automatically be fetched from GitHub

