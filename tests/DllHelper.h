/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 * See LICENSE.txt for license information.
 */

/* Adapted from Niall Douglas's explanation of visibility in the GCC docs */
#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef __GNUC__
#define DLL_EXPORT __attribute__((dllexport))
#else
#define DLL_EXPORT __declspec(dllexport)
#endif
#else
#if __GNUC__ >= 4
#define DLL_EXPORT __attribute__((visibility ("default")))
#else
#define DLL_EXPORT
#endif
#endif

/* It's best to build with -fvisibility=hidden.  In CMake, that can be done with:
  set_target_properties(MyTarget PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

  If you can't build with that flag, then push visibility=hidden and never pop it:

	#ifdef __GNUC__
	#pragma GCC visibility push(hidden)
	#endif
*/

#if defined(_WIN32)
#include <Windows.h>
#define LOAD_DLL(x)    LoadLibraryA(x)
#define GET_DLL_FUNC   GetProcAddress
#define DLL_PREFIX     ""
#define DLL_SUFFIX     ".dll"
#elif defined(__GNUC__)
#include <dlfcn.h>
#define LOAD_DLL(x)    dlopen(x, RTLD_LAZY)
#define GET_DLL_FUNC   dlsym
#define DLL_PREFIX     "lib"
#ifdef __APPLE__
#define DLL_SUFFIX     ".dylib"
#else
#define DLL_SUFFIX     ".so"
#endif
#endif
