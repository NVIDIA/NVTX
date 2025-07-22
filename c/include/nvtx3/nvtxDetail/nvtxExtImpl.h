/*
 * SPDX-FileCopyrightText: Copyright (c) 2009-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef NVTX_EXT_IMPL_GUARD
#error Never include this file directly -- it is automatically included by nvToolsExt.h (except when NVTX_NO_IMPL is defined).
#endif

#if defined(NVTX_AS_SYSTEM_HEADER)
#if defined(__clang__)
#pragma clang system_header
#elif defined(__GNUC__) || defined(__NVCOMPILER)
#pragma GCC system_header
#elif defined(_MSC_VER)
#pragma system_header
#endif
#endif

#ifndef NVTX_EXT_IMPL_H
#define NVTX_EXT_IMPL_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* ---- Include required platform headers ---- */

#if defined(_WIN32)

#include <windows.h>

#else
#include <unistd.h>

#if defined(__ANDROID__)
#include <android/api-level.h>
#endif

#if defined(__linux__) || defined(__CYGWIN__)
#include <sched.h>
#endif

#include <sys/types.h>
#include <limits.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#endif

/* ---- Define macros used in this file ---- */

#ifdef NVTX_DEBUG_PRINT
#ifdef __ANDROID__
#include <android/log.h>
#define NVTX_ERR(...) __android_log_print(ANDROID_LOG_ERROR, "NVTOOLSEXT", __VA_ARGS__);
#define NVTX_INFO(...) __android_log_print(ANDROID_LOG_INFO, "NVTOOLSEXT", __VA_ARGS__);
#else
#include <stdio.h>
#define NVTX_ERR(...) fprintf(stderr, "NVTX_ERROR: " __VA_ARGS__)
#define NVTX_INFO(...) fprintf(stderr, "NVTX_INFO: " __VA_ARGS__)
#endif
#else /* !defined(NVTX_DEBUG_PRINT) */
#define NVTX_ERR(...)
#define NVTX_INFO(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __GNUC__
#pragma GCC visibility push(hidden)
#endif

#define NVTX_EXTENSION_FRESH 0    /* Uninitialized extension or function slot */
#define NVTX_EXTENSION_DISABLED 1 /* Disabled extension or function slot */
#define NVTX_EXTENSION_STARTING 2 /* Extension is being initialized. */
#define NVTX_EXTENSION_LOADED 3   /* Extension is initialized successfully. */
#define NVTX_EXTENSION_INIT_FN_FAILED 4 /* Extension init function returned failure. */

/* Function slots are local to each extension */
typedef struct nvtxExtGlobals1_t
{
    NvtxExtInitializeInjectionFunc_t injectionFnPtr;
} nvtxExtGlobals1_t;

NVTX_LINKONCE_DEFINE_GLOBAL nvtxExtGlobals1_t NVTX_VERSIONED_IDENTIFIER(nvtxExtGlobals1) =
{
    NVTX_NULLPTR
};

#define NVTX_EXT_INIT_GUARD
#include "nvtxExtInit.h"
#undef NVTX_EXT_INIT_GUARD

#ifdef __GNUC__
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* NVTX_EXT_IMPL_H */
