# SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://nvidia.github.io/NVTX/LICENSE.txt for license information.

function(add_nvtx_test_library TARGET_NAME TYPE)
    set(sources ${ARGN})
    list(GET sources 0 first_file)
    get_filename_component(first_ext "${first_file}" LAST_EXT)
    if(first_ext STREQUAL ".c")
        set(link_lib nvtx3-c)
    elseif(first_ext STREQUAL ".cpp" OR first_ext STREQUAL ".cu")
        set(link_lib nvtx3-cpp)
    else()
        message(SEND_ERROR "Unrecognized file extension: ${first_file}")
    endif()

    add_library(${TARGET_NAME} ${TYPE} ${sources})
    target_link_libraries(${TARGET_NAME} PRIVATE ${link_lib})
    set_target_properties(${TARGET_NAME} PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
endfunction()

function(set_preinjection_environment TEST_NAME LIBRARY_NAME)
    if(APPLE)
        set_property(TEST "${TEST_NAME}" PROPERTY
            ENVIRONMENT DYLD_INSERT_LIBRARIES=${CMAKE_CURRENT_BINARY_DIR}/lib${LIBRARY_NAME}.dylib)
    elseif(CYGWIN)
        set_property(TEST "${TEST_NAME}" PROPERTY
            ENVIRONMENT LD_PRELOAD=${CMAKE_CURRENT_BINARY_DIR}/cyg${LIBRARY_NAME}.dll)
    elseif(NOT WIN32)
        set_property(TEST "${TEST_NAME}" PROPERTY
            ENVIRONMENT LD_PRELOAD=${CMAKE_CURRENT_BINARY_DIR}/lib${LIBRARY_NAME}.so)
    endif()
endfunction()

if(NOT ENABLE_NVTX3_HPP_TESTS)
    add_compile_definitions(DISABLE_NVTX3_HPP)
endif()

add_executable(runtest "RunTest.cpp")
target_link_libraries(runtest PRIVATE nvtx3-cpp)
if(APPLE)
    target_sources(runtest PRIVATE "AppleHelper.c" "AppleHelper.h")
endif()

add_nvtx_test_library(inj-c SHARED "PrintInjectionC.c" "PrintInjectionImpl.cpp")
target_compile_definitions(inj-c PRIVATE SUPPORT_STATIC_INJECTION SUPPORT_DYNAMIC_INJECTION SUPPORT_PREINJECTION INJECTION_PRINT_PREFIX="inj-c")

add_nvtx_test_library(inj-cpp SHARED "PrintInjectionCpp.cpp" "PrintInjectionImpl.cpp")
target_compile_definitions(inj-cpp PRIVATE SUPPORT_STATIC_INJECTION SUPPORT_DYNAMIC_INJECTION SUPPORT_PREINJECTION INJECTION_PRINT_PREFIX="inj-cpp")

add_nvtx_test_library(inj-c-static STATIC "PrintInjectionC.c" "PrintInjectionImpl.cpp")
if(NOT MSVC)
    target_compile_options(inj-c-static PRIVATE -fPIC)
endif()
target_compile_definitions(inj-c-static PRIVATE SUPPORT_STATIC_INJECTION INJECTION_PRINT_PREFIX="inj-c-static")

add_nvtx_test_library(inj-cpp-static STATIC "PrintInjectionCpp.cpp" "PrintInjectionImpl.cpp")
if(NOT MSVC)
    target_compile_options(inj-cpp-static PRIVATE -fPIC)
endif()
target_compile_definitions(inj-cpp-static PRIVATE SUPPORT_STATIC_INJECTION INJECTION_PRINT_PREFIX="inj-cpp-static")

add_nvtx_test_library(inj-c-dynamic SHARED "PrintInjectionC.c" "PrintInjectionImpl.cpp")
target_compile_definitions(inj-c-dynamic PRIVATE SUPPORT_DYNAMIC_INJECTION INJECTION_PRINT_PREFIX="inj-c-dynamic")

add_nvtx_test_library(inj-cpp-dynamic SHARED "PrintInjectionCpp.cpp" "PrintInjectionImpl.cpp")
target_compile_definitions(inj-cpp-dynamic PRIVATE SUPPORT_DYNAMIC_INJECTION INJECTION_PRINT_PREFIX="inj-cpp-dynamic")

add_nvtx_test_library(inj-c-preinject SHARED "PrintInjectionC.c" "PrintInjectionImpl.cpp")
target_compile_definitions(inj-c-preinject PRIVATE SUPPORT_PREINJECTION INJECTION_PRINT_PREFIX="inj-c-preinject")

add_nvtx_test_library(inj-cpp-preinject SHARED "PrintInjectionCpp.cpp" "PrintInjectionImpl.cpp")
target_compile_definitions(inj-cpp-preinject PRIVATE SUPPORT_PREINJECTION INJECTION_PRINT_PREFIX="inj-cpp-preinject")

if(NVTX_ENABLE_CXX11_TESTS)
    add_nvtx_test_library(self SHARED "TestSelfInjection.cpp" "SelfInjection.cpp")

    add_nvtx_test_library(calls SHARED "Calls.cpp" "SelfInjection.cpp")
endif()

add_nvtx_test_library(coverage-c SHARED "CoverageC.c")

if(NOT MSVC)
    add_nvtx_test_library(coverage-c-static-obj-c SHARED "CoverageC.c" "PrintInjectionC.c" "PrintInjectionImpl.cpp")
    target_compile_definitions(coverage-c-static-obj-c PRIVATE SUPPORT_STATIC_INJECTION INJECTION_PRINT_PREFIX="inj-c-static")

    add_nvtx_test_library(coverage-c-static-obj-cpp SHARED "CoverageC.c" "PrintInjectionCpp.cpp" "PrintInjectionImpl.cpp")
    target_compile_definitions(coverage-c-static-obj-cpp PRIVATE SUPPORT_STATIC_INJECTION INJECTION_PRINT_PREFIX="inj-cpp-static")

    if(NOT APPLE)
        add_nvtx_test_library(coverage-c-static-lib-c SHARED "CoverageC.c")
        target_link_libraries(coverage-c-static-lib-c PRIVATE -Wl,--whole-archive inj-c-static -Wl,--no-whole-archive)

        add_nvtx_test_library(coverage-c-static-lib-cpp SHARED "CoverageC.c")
        target_link_libraries(coverage-c-static-lib-cpp PRIVATE -Wl,--whole-archive inj-cpp-static -Wl,--no-whole-archive)
    endif()
endif()

add_nvtx_test_library(coverage-c-preinject SHARED "CoverageC.c")
target_compile_definitions(coverage-c-preinject PRIVATE NVTX_SUPPORT_ALREADY_INJECTED_LIBRARY=1)

add_nvtx_test_library(coverage-c-disabled SHARED "CoverageC.c")
target_compile_definitions(coverage-c-disabled PRIVATE NVTX_DISABLE)

if(ENABLE_NVTX3_HPP_TESTS)
    add_nvtx_test_library(coverage-cpp SHARED "CoverageCpp.cpp")

    add_executable(explicit-version "ExplicitVersion.cpp")
    target_link_libraries(explicit-version PRIVATE nvtx3-cpp)

    if(NOT MSVC)
        add_nvtx_test_library(coverage-cpp-static-obj-c SHARED "CoverageCpp.cpp" "PrintInjectionC.c" "PrintInjectionImpl.cpp")
        target_compile_definitions(coverage-cpp-static-obj-c PRIVATE SUPPORT_STATIC_INJECTION INJECTION_PRINT_PREFIX="inj-c-static")

        add_nvtx_test_library(coverage-cpp-static-obj-cpp SHARED "CoverageCpp.cpp" "PrintInjectionCpp.cpp" "PrintInjectionImpl.cpp")
        target_compile_definitions(coverage-cpp-static-obj-cpp PRIVATE SUPPORT_STATIC_INJECTION INJECTION_PRINT_PREFIX="inj-cpp-static")

        if(NOT APPLE)
            add_nvtx_test_library(coverage-cpp-static-lib-c SHARED "CoverageCpp.cpp")
            target_link_libraries(coverage-cpp-static-lib-c PRIVATE -Wl,--whole-archive inj-c-static -Wl,--no-whole-archive)

            add_nvtx_test_library(coverage-cpp-static-lib-cpp SHARED "CoverageCpp.cpp")
            target_link_libraries(coverage-cpp-static-lib-cpp PRIVATE -Wl,--whole-archive inj-cpp-static -Wl,--no-whole-archive)
        endif()
    endif()

    add_nvtx_test_library(coverage-cpp-preinject SHARED "CoverageCpp.cpp")
    target_compile_definitions(coverage-cpp-preinject PRIVATE NVTX_SUPPORT_ALREADY_INJECTED_LIBRARY=1)

    add_nvtx_test_library(coverage-cpp-disabled SHARED "CoverageCpp.cpp")
    target_compile_definitions(coverage-cpp-disabled PRIVATE NVTX_DISABLE)

    if(ENABLE_CUDA)
        add_nvtx_test_library(coverage-cu SHARED "CoverageCuda.cu")

        add_nvtx_test_library(coverage-cu-disabled SHARED "CoverageCuda.cu")
        target_compile_definitions(coverage-cu-disabled PRIVATE NVTX_DISABLE)
    endif()
endif()

add_nvtx_test_library(coverage-ext-mem SHARED "CoverageExtMem.c")

add_nvtx_test_library(coverage-ext-mem-disabled SHARED "CoverageExtMem.c")
target_compile_definitions(coverage-ext-mem-disabled PRIVATE NVTX_DISABLE)

if(ENABLE_CUDA)
    add_nvtx_test_library(coverage-ext-memcudart SHARED "CoverageExtMemCudaRt.cu")

    if(NOT MSVC)
        add_nvtx_test_library(coverage-ext-memcudart-disabled SHARED "CoverageExtMemCudaRt.cu")
        target_compile_definitions(coverage-ext-memcudart-disabled PRIVATE NVTX_DISABLE)
    endif()
endif()

add_nvtx_test_library(coverage-ext-payload SHARED "CoverageExtPayload.c")

add_nvtx_test_library(coverage-ext-payload-disabled SHARED "CoverageExtPayload.c")
target_compile_definitions(coverage-ext-payload-disabled PRIVATE NVTX_DISABLE)

set(skip_payload_helper_macros_test OFF)
# Older MSVC cannot expand NVTX payload helper variadic selectors.
if(MSVC AND MSVC_TOOLSET_VERSION LESS 142)
    set(skip_payload_helper_macros_test ON)
endif()

if(NOT skip_payload_helper_macros_test)
    add_nvtx_test_library(payload-helper-macros-c SHARED "PayloadEntriesHelperC.c")
    add_nvtx_test_library(payload-helper-macros-cpp SHARED "PayloadEntriesHelperCpp.cpp")
endif()

add_nvtx_test_library(coverage-ext-counter SHARED "CoverageExtCounter.c")

add_nvtx_test_library(coverage-ext-counter-disabled SHARED "CoverageExtCounter.c")
target_compile_definitions(coverage-ext-counter-disabled PRIVATE NVTX_DISABLE)

add_nvtx_test_library(coverage-nvtxw-c SHARED "CoverageNvtxwC.c")
add_nvtx_test_library(coverage-nvtxw-cpp SHARED "CoverageNvtxwCpp.cpp")

# Recording mock NVTXW backend.  Built with the default backend filename
# (libnvtxw3.so / nvtxw3.dll) so the loader's directory and config search modes
# can find it by the conventional name.  Used by the loader tests below.
# MinGW/MSYS2 default to a "lib" shared-library prefix; clear it on Windows so
# the artifact matches NVTXW_LIB_FILENAME_DEFAULT ("nvtxw3.dll").
add_nvtx_test_library(nvtxw-mock-backend SHARED "MockNvtxwBackend.c")
set_target_properties(nvtxw-mock-backend PROPERTIES OUTPUT_NAME "nvtxw3")
if(WIN32)
    set_target_properties(nvtxw-mock-backend PROPERTIES PREFIX "")
endif()

# The loader is a static library; make it position-independent so it can be
# linked into the shared loader test library on all platforms.  It uses POSIX
# APIs (readlink, getcwd), so build it with GNU extensions to make those
# declarations visible; the source itself is clean under this project's strict
# warning set, so it needs no -Werror relaxation.
set_target_properties(nvtxw3-loader PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    set_target_properties(nvtxw3-loader PROPERTIES C_EXTENSIONS ON)
endif()

# Loader unit tests: nvtxwLoad across its library resolution and error
# paths, nvtxwUnload, and nvtxwGetError, driven against the mock backend.
add_nvtx_test_library(coverage-nvtxw-loader SHARED "CoverageNvtxwLoader.c")
target_link_libraries(coverage-nvtxw-loader PRIVATE nvtxw3-loader)

# End-to-end helper coverage that asserts on data captured by the mock backend.
# Built as C only; the helper headers' C++ compile coverage comes from
# coverage-nvtxw-cpp, and the runtime assertions here are language-independent.
add_nvtx_test_library(coverage-nvtxw-e2e SHARED "CoverageNvtxwE2E.c")

# Round-trip coverage: decode the helper-produced schemas and payloads with the
# standalone NVTX payload parser from tools/sample-injection (a real consumer)
# and assert the decoded output.  Built as C++14 on older MSVC toolsets (VS 2015)
# and C++17 elsewhere.  It was not written for this project's strict warning
# set, so build it as a separate library with warnings relaxed (it is a
# dependency under test, not test source).
if(MSVC AND MSVC_TOOLSET_VERSION LESS_EQUAL 140)
    set(nvtxw_payload_parser_cxx_std 14)
else()
    set(nvtxw_payload_parser_cxx_std 17)
endif()

set(nvtxw_parser_dir "${NVTX3_PROJ_ROOT}/tools/sample-injection")
add_library(nvtxw-payload-parser-fortest STATIC
    "${nvtxw_parser_dir}/Source/Payload/NvtxPayloadRegistry.cpp"
    "${nvtxw_parser_dir}/Source/Payload/NvtxPayloadParser.cpp"
    "${nvtxw_parser_dir}/Source/Payload/NvtxPayloadSchema.cpp"
    "${nvtxw_parser_dir}/Source/Payload/NvtxPayloadTextFormatter.cpp"
    "${nvtxw_parser_dir}/Source/Payload/NvtxPayloadJsonFormatter.cpp"
    "${nvtxw_parser_dir}/Source/NvtxPayloadInjectionAdapter.cpp")
target_include_directories(nvtxw-payload-parser-fortest PUBLIC
    "${nvtxw_parser_dir}/Source/Payload"
    "${nvtxw_parser_dir}/Source")
target_link_libraries(nvtxw-payload-parser-fortest PUBLIC nvtx3-c)
set_target_properties(nvtxw-payload-parser-fortest PROPERTIES
    CXX_STANDARD ${nvtxw_payload_parser_cxx_std}
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    # /W0 alone is not enough: directory-level -WX still promotes any leaked
    # warning (e.g. UCRT C4738 on x86) into an error.
    target_compile_options(nvtxw-payload-parser-fortest PRIVATE /W0 /WX-)
else()
    target_compile_options(nvtxw-payload-parser-fortest PRIVATE -w)
endif()

add_nvtx_test_library(coverage-nvtxw-roundtrip SHARED "CoverageNvtxwRoundTrip.cpp")
target_link_libraries(coverage-nvtxw-roundtrip PRIVATE nvtxw-payload-parser-fortest)
set_target_properties(coverage-nvtxw-roundtrip PROPERTIES
    CXX_STANDARD ${nvtxw_payload_parser_cxx_std}
    CXX_STANDARD_REQUIRED ON)
# Includes parser headers that were not written for this project's /Wall /WX set
# (e.g. MSVC C4623/C4626/C5027 on reference-member aggregates).
if(MSVC)
    target_compile_options(coverage-nvtxw-roundtrip PRIVATE /W0 /WX-)
else()
    target_compile_options(coverage-nvtxw-roundtrip PRIVATE -w)
endif()

if(ENABLE_NVTX3_HPP_TESTS)
    add_nvtx_test_library(attributes SHARED "Attributes.cpp")

    if(NOT MSVC OR MSVC_TOOLSET_VERSION GREATER 140)
        add_nvtx_test_library(payloaddata SHARED "PayloadData.cpp")
    endif()

    add_nvtx_test_library(domains SHARED "Domains.cpp")
    #if(DOMAINS_ERROR_TEST_NAME_IS_MISSING)
    #    target_compile_definitions(domains PRIVATE ERROR_TEST_NAME_IS_MISSING)
    #endif()

    add_nvtx_test_library(categories SHARED "NamedCategories.cpp")

    add_nvtx_test_library(regstrings SHARED "RegisteredStrings.cpp")

    add_nvtx_test_library(linkerdupes SHARED "LinkerDupesMain.cpp" "LinkerDupesFileA.cpp" "LinkerDupesFileB.cpp")

    if(NOT MSVC OR MSVC_TOOLSET_VERSION GREATER 140)
        add_nvtx_test_library(counters SHARED "Counters.cpp")
        add_nvtx_test_library(semantics SHARED "Semantics.cpp")
    endif()
endif()

add_nvtx_test_library(export-api SHARED "ExportApi.c")
target_include_directories(export-api PRIVATE "Imports/cuda_lite" "Imports/opencl_lite")
target_link_options(export-api PRIVATE
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<PLATFORM_ID:Windows>>:-Wl,--kill-at>
)

add_nvtx_test_library(export-api-ext SHARED "ExportApi.c")
target_compile_definitions(export-api-ext PRIVATE SUPPORT_EXTENSIONS)
target_include_directories(export-api-ext PRIVATE "Imports/cuda_lite" "Imports/opencl_lite")
target_link_options(export-api-ext PRIVATE
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<PLATFORM_ID:Windows>>:-Wl,--kill-at>
)

if(NVTX_ENABLE_CXX11_TESTS)
    add_nvtx_test_library(use-exported-api SHARED "UseExportedApi.cpp")
    target_include_directories(use-exported-api PRIVATE "Imports/cuda_lite" "Imports/opencl_lite")
    if(APPLE)
        target_sources(use-exported-api PRIVATE "AppleHelper.c" "AppleHelper.h")
    endif()

    add_nvtx_test_library(use-exported-api-ext SHARED "UseExportedApi.cpp")
    target_compile_definitions(use-exported-api-ext PRIVATE SUPPORT_EXTENSIONS)
    target_include_directories(use-exported-api-ext PRIVATE "Imports/cuda_lite" "Imports/opencl_lite")
    if(APPLE)
        target_sources(use-exported-api-ext PRIVATE "AppleHelper.c" "AppleHelper.h")
    endif()
endif()

if(NVTX_ENABLE_CXX11_TESTS)
    add_test(NAME "Self" COMMAND runtest -t self)
    add_test(NAME "Self with SelfInjection" COMMAND runtest -t self -i self)
    add_test(NAME "Self with PrintInjectionC" COMMAND runtest -t self -i inj-c)
    add_test(NAME "Self with PrintInjectionCpp" COMMAND runtest -t self -i inj-cpp)
    add_test(NAME "Calls" COMMAND runtest -t calls)
    add_test(NAME "Calls with CallsInjection" COMMAND runtest -t calls -i calls)
    add_test(NAME "Calls with PrintInjectionC" COMMAND runtest -t calls -i inj-c)
    add_test(NAME "Calls with PrintInjectionCpp" COMMAND runtest -t calls -i inj-cpp)
endif()
add_test(NAME "CoverageC" COMMAND runtest -t coverage-c)
add_test(NAME "CoverageC with PrintInjectionC" COMMAND runtest -t coverage-c -i inj-c)
add_test(NAME "CoverageC with PrintInjectionCpp" COMMAND runtest -t coverage-c -i inj-cpp)
if(NOT MSVC)
    add_test(NAME "CoverageC with PrintInjectionC as StaticObjectC" COMMAND runtest -t coverage-c-static-obj-c)
    add_test(NAME "CoverageC with PrintInjectionCpp as StaticObjectCpp" COMMAND runtest -t coverage-c-static-obj-cpp)
    if(NOT APPLE)
        add_test(NAME "CoverageC with PrintInjectionC as StaticLibraryC" COMMAND runtest -t coverage-c-static-lib-c)
        add_test(NAME "CoverageC with PrintInjectionCpp as StaticLibraryCpp" COMMAND runtest -t coverage-c-static-lib-cpp)
    endif()
endif()
add_test(NAME "CoverageC with PrintInjectionC as Dynamic" COMMAND runtest -t coverage-c -i inj-c-dynamic)
add_test(NAME "CoverageC with PrintInjectionCpp as Dynamic" COMMAND runtest -t coverage-c -i inj-cpp-dynamic)
add_test(NAME "CoverageC with PrintInjectionC as Preinjection" COMMAND runtest -t coverage-c-preinject)
set_preinjection_environment("CoverageC with PrintInjectionC as Preinjection" inj-c-preinject)
add_test(NAME "CoverageC with PrintInjectionCpp as Preinjection" COMMAND runtest -t coverage-c-preinject)
set_preinjection_environment("CoverageC with PrintInjectionCpp as Preinjection" inj-cpp-preinject)
add_test(NAME "CoverageC with NVTX_DISABLE" COMMAND runtest -t coverage-c-disabled)
if(ENABLE_NVTX3_HPP_TESTS)
    add_test(NAME "ExplicitVersion" COMMAND explicit-version)
    add_test(NAME "CoverageCpp" COMMAND runtest -t coverage-cpp)
    add_test(NAME "CoverageCpp with PrintInjectionC" COMMAND runtest -t coverage-cpp -i inj-c)
    add_test(NAME "CoverageCpp with PrintInjectionCpp" COMMAND runtest -t coverage-cpp -i inj-cpp)
    if(NOT MSVC)
        add_test(NAME "CoverageCpp with PrintInjectionC as StaticObjectC" COMMAND runtest -t coverage-cpp-static-obj-c)
        add_test(NAME "CoverageCpp with PrintInjectionCpp as StaticObjectCpp" COMMAND runtest -t coverage-cpp-static-obj-cpp)
        if(NOT APPLE)
            add_test(NAME "CoverageCpp with PrintInjectionC as StaticLibraryC" COMMAND runtest -t coverage-cpp-static-lib-c)
            add_test(NAME "CoverageCpp with PrintInjectionCpp as StaticLibraryCpp" COMMAND runtest -t coverage-cpp-static-lib-cpp)
        endif()
    endif()
    add_test(NAME "CoverageCpp with PrintInjectionC as Dynamic" COMMAND runtest -t coverage-cpp -i inj-c-dynamic)
    add_test(NAME "CoverageCpp with PrintInjectionCpp as Dynamic" COMMAND runtest -t coverage-cpp -i inj-cpp-dynamic)
    add_test(NAME "CoverageCpp with PrintInjectionC as Preinjection" COMMAND runtest -t coverage-cpp-preinject)
    set_preinjection_environment("CoverageCpp with PrintInjectionC as Preinjection" inj-c-preinject)
    add_test(NAME "CoverageCpp with PrintInjectionCpp as Preinjection" COMMAND runtest -t coverage-cpp-preinject)
    set_preinjection_environment("CoverageCpp with PrintInjectionCpp as Preinjection" inj-cpp-preinject)
    add_test(NAME "CoverageCpp with NVTX_DISABLE" COMMAND runtest -t coverage-cpp-disabled)
    if(ENABLE_CUDA)
        add_test(NAME "CoverageCuda" COMMAND runtest -t coverage-cu)
        add_test(NAME "CoverageCuda with PrintInjectionC" COMMAND runtest -t coverage-cu -i inj-c)
        add_test(NAME "CoverageCuda with PrintInjectionCpp" COMMAND runtest -t coverage-cu -i inj-cpp)
        add_test(NAME "CoverageCuda with NVTX_DISABLE" COMMAND runtest -t coverage-cu-disabled)
    endif()
endif()
add_test(NAME "CoverageExtMem" COMMAND runtest -t coverage-ext-mem)
add_test(NAME "CoverageExtMem with PrintInjectionC" COMMAND runtest -t coverage-ext-mem -i inj-c)
add_test(NAME "CoverageExtMem with PrintInjectionCpp" COMMAND runtest -t coverage-ext-mem -i inj-cpp)
add_test(NAME "CoverageExtMem with NVTX_DISABLE" COMMAND runtest -t coverage-ext-mem-disabled)
if(ENABLE_CUDA)
    add_test(NAME "CoverageExtMemCudaRt" COMMAND runtest -t coverage-ext-memcudart)
    add_test(NAME "CoverageExtMemCudaRt with PrintInjectionC" COMMAND runtest -t coverage-ext-memcudart -i inj-c)
    add_test(NAME "CoverageExtMemCudaRt with PrintInjectionCpp" COMMAND runtest -t coverage-ext-memcudart -i inj-cpp)
    if(NOT MSVC)
        add_test(NAME "CoverageExtMemCudaRt with NVTX_DISABLE" COMMAND runtest -t coverage-ext-memcudart-disabled)
    endif()
endif()
add_test(NAME "CoverageExtPayload" COMMAND runtest -t coverage-ext-payload)
add_test(NAME "CoverageExtPayload with PrintInjectionC" COMMAND runtest -t coverage-ext-payload -i inj-c)
add_test(NAME "CoverageExtPayload with PrintInjectionCpp" COMMAND runtest -t coverage-ext-payload -i inj-cpp)
add_test(NAME "CoverageExtPayload with NVTX_DISABLE" COMMAND runtest -t coverage-ext-payload-disabled)
if(NOT skip_payload_helper_macros_test)
    add_test(NAME "PayloadHelperMacrosC" COMMAND runtest -t payload-helper-macros-c)
    add_test(NAME "PayloadHelperMacrosC with PrintInjectionC" COMMAND runtest -t payload-helper-macros-c -i inj-c)
    add_test(NAME "PayloadHelperMacrosC with PrintInjectionCpp" COMMAND runtest -t payload-helper-macros-c -i inj-cpp)
    add_test(NAME "PayloadHelperMacrosCpp" COMMAND runtest -t payload-helper-macros-cpp)
    add_test(NAME "PayloadHelperMacrosCpp with PrintInjectionC" COMMAND runtest -t payload-helper-macros-cpp -i inj-c)
    add_test(NAME "PayloadHelperMacrosCpp with PrintInjectionCpp" COMMAND runtest -t payload-helper-macros-cpp -i inj-cpp)
endif()
add_test(NAME "CoverageExtCounter" COMMAND runtest -t coverage-ext-counter)
add_test(NAME "CoverageExtCounter with PrintInjectionC" COMMAND runtest -t coverage-ext-counter -i inj-c)
add_test(NAME "CoverageExtCounter with PrintInjectionCpp" COMMAND runtest -t coverage-ext-counter -i inj-cpp)
add_test(NAME "CoverageExtCounter with NVTX_DISABLE" COMMAND runtest -t coverage-ext-counter-disabled)
add_test(NAME "CoverageNvtxwC" COMMAND runtest -t coverage-nvtxw-c)
add_test(NAME "CoverageNvtxwCpp" COMMAND runtest -t coverage-nvtxw-cpp)
add_test(NAME "CoverageNvtxwLoader" COMMAND runtest -t coverage-nvtxw-loader
    "$<TARGET_FILE:nvtxw-mock-backend>"
    "$<TARGET_FILE_DIR:nvtxw-mock-backend>"
    "$<TARGET_FILE:coverage-c>")
add_test(NAME "CoverageNvtxwE2E" COMMAND runtest -t coverage-nvtxw-e2e)
add_test(NAME "CoverageNvtxwRoundTrip" COMMAND runtest -t coverage-nvtxw-roundtrip)
if(ENABLE_NVTX3_HPP_TESTS)
    add_test(NAME "Attributes" COMMAND runtest -t attributes)
    add_test(NAME "Attributes with PrintInjectionC" COMMAND runtest -t attributes -i inj-c)
    add_test(NAME "Attributes with PrintInjectionCpp" COMMAND runtest -t attributes -i inj-cpp)
    if(NOT MSVC OR MSVC_TOOLSET_VERSION GREATER 140)
        add_test(NAME "PayloadData" COMMAND runtest -t payloaddata)
        add_test(NAME "PayloadData with PrintInjectionC" COMMAND runtest -t payloaddata -i inj-c)
        add_test(NAME "PayloadData with PrintInjectionCpp" COMMAND runtest -t payloaddata -i inj-cpp)
    endif()
    add_test(NAME "Domains" COMMAND runtest -t domains)
    add_test(NAME "Domains with PrintInjectionC" COMMAND runtest -t domains -i inj-c)
    add_test(NAME "Domains with PrintInjectionCpp" COMMAND runtest -t domains -i inj-cpp)
    add_test(NAME "NamedCategories" COMMAND runtest -t categories)
    add_test(NAME "NamedCategories with PrintInjectionC" COMMAND runtest -t categories -i inj-c)
    add_test(NAME "NamedCategories with PrintInjectionCpp" COMMAND runtest -t categories -i inj-cpp)
    add_test(NAME "RegisteredStrings" COMMAND runtest -t regstrings)
    add_test(NAME "RegisteredStrings with PrintInjectionC" COMMAND runtest -t regstrings -i inj-c)
    add_test(NAME "RegisteredStrings with PrintInjectionCpp" COMMAND runtest -t regstrings -i inj-cpp)
    add_test(NAME "LinkerDupes" COMMAND runtest -t linkerdupes)
    add_test(NAME "LinkerDupes with PrintInjectionC" COMMAND runtest -t linkerdupes -i inj-c)
    add_test(NAME "LinkerDupes with PrintInjectionCpp" COMMAND runtest -t linkerdupes -i inj-cpp)
    if(NOT MSVC OR MSVC_TOOLSET_VERSION GREATER 140)
        add_test(NAME "Counters" COMMAND runtest -t counters)
        add_test(NAME "Counters with PrintInjectionC" COMMAND runtest -t counters -i inj-c)
        add_test(NAME "Counters with PrintInjectionCpp" COMMAND runtest -t counters -i inj-cpp)
        add_test(NAME "Semantics" COMMAND runtest -t semantics)
        add_test(NAME "Semantics with PrintInjectionC" COMMAND runtest -t semantics -i inj-c)
        add_test(NAME "Semantics with PrintInjectionCpp" COMMAND runtest -t semantics -i inj-cpp)
    endif()
endif()
if(NVTX_ENABLE_CXX11_TESTS)
    add_test(NAME "UseExportedApi" COMMAND runtest -t use-exported-api)
    add_test(NAME "UseExportedApi with PrintInjectionC" COMMAND runtest -t use-exported-api -i inj-c)
    add_test(NAME "UseExportedApi with PrintInjectionCpp" COMMAND runtest -t use-exported-api -i inj-cpp)
    add_test(NAME "UseExportedApiExt" COMMAND runtest -t use-exported-api-ext)
    add_test(NAME "UseExportedApiExt with PrintInjectionC" COMMAND runtest -t use-exported-api-ext -i inj-c)
    add_test(NAME "UseExportedApiExt with PrintInjectionCpp" COMMAND runtest -t use-exported-api-ext -i inj-cpp)
endif()

add_subdirectory(ShouldFail)
