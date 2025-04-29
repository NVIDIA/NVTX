# Windows CI builds have issues finding compilers:
if (MSVC)
  return()
endif()

set(cmake_opts
  -D "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  -D "CMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
  -D "CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
  -D "CMAKE_CUDA_HOST_COMPILER=${CMAKE_CUDA_HOST_COMPILER}"
)

function(add_cmake_subdir_test test_name variant nvtx_root)
  set(src_dir "${CMAKE_CURRENT_SOURCE_DIR}/add_subdir_test")
  set(build_dir "${CMAKE_CURRENT_BINARY_DIR}/add_subdir_test/${variant}")

  add_test(NAME "${test_name}"
    COMMAND "${CMAKE_CTEST_COMMAND}"
      --build-and-test "${src_dir}" "${build_dir}"
      --build-generator "${CMAKE_GENERATOR}"
      --build-options
        -D "nvtx_root=${nvtx_root}"
        ${ARGN}
  )
endfunction()

add_cmake_subdir_test(
  "Test CMake add_subdirectory using project root."
  "project_root"
  "${NVTX3_PROJ_ROOT}"
  ${cmake_opts}
)

add_cmake_subdir_test(
  "Test CMake add_subdirectory using c/ subdirectory."
  "c_subdir"
  "${NVTX3_C_ROOT}"
  ${cmake_opts}
)
