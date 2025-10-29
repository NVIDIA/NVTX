/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Licensed under the Apache License v2.0 with LLVM Exceptions.
 * See https://nvidia.github.io/NVTX/LICENSE.txt for license information.
 */

#pragma once

/* To export a function from a DLL, include nvtx3/nvToolsExt.h and use:
 * - Use extern "C" (if C++) and NVTX_DYNAMIC_EXPORT in front of the function declaration/definition
 * - Use NVTX_EXPORT_UNMANGLED_FUNCTION_NAME inside the function body to prevent name-mangling
 *
 * On GCC and similar compilers, it's best to build with -fvisibility=hidden.  This ensures normal
 * functions will not be dynamic exports.  In CMake, that can be done with:
 *   set_target_properties(MyTarget PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
 *
 * If you can't build with that flag, then push visibility=hidden and never pop it:
 *   #ifdef __GNUC__
 *   #pragma GCC visibility push(hidden)
 *   #endif
 *
 * Note that NVTX_DYNAMIC_EXPORT will export a function even if the default visibility is hidden.
 * NVTX_EXPORT_UNMANGLED_FUNCTION_NAME isn't necessary on many platforms, but using it will ensure
 * success when loading function pointers via GET_DLL_FUNC (see below) on any platform, and from
 * other languages' C bindings.
 */

#if defined(_WIN32)

#include <windows.h>

/* Don't try to use wide chars here -- stick with char* for simpler cross-plat coding */
#define DLL_HANDLE     HMODULE
#define DLL_OPEN(x)    LoadLibraryA(x)
#define DLL_CLOSE(x)   FreeLibraryA(x)
#define GET_DLL_FUNC(h, x) reinterpret_cast<void(*)(void)>(GetProcAddress((h), (x)))
#if defined(_MSC_VER)
#define DLL_PREFIX     ""
#else
#define DLL_PREFIX     "lib"
#endif
#define DLL_SUFFIX     ".dll"

#else /* Assume GCC-like compiler, but don't require defined(__GNUC__) */

#include <dlfcn.h>

#define DLL_HANDLE     void*
#define DLL_OPEN(lib)  dlopen(lib, RTLD_LAZY)
#define DLL_CLOSE(h)   dlclose(h)
#define GET_DLL_FUNC(h, x) dlsym((h), (x))
#define DLL_PREFIX     "lib"
#if defined(__APPLE__)
#define DLL_SUFFIX     ".dylib"
#else
#define DLL_SUFFIX     ".so"
#endif

#endif
