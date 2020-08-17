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

