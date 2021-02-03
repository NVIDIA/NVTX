/*
* Copyright 2009-2020  NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License v2.0 with LLVM Exceptions.
* See https://llvm.org/LICENSE.txt for license information.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#ifndef NVTX_EXT_IMPL_GUARD
#error Never include this file directly -- it is automatically included by nvToolsExt.h (except when NVTX_NO_IMPL is defined).
#endif

#ifndef NVTX_EXT_IMPL_H
#define NVTX_EXT_IMPL_H
/* ---- Include required platform headers ---- */

#if defined(_WIN32) 

#include <Windows.h>

#else
#include <unistd.h>

#if defined(__ANDROID__)
#include <android/api-level.h> 
#endif

#if defined(__linux__) || defined(__CYGWIN__)
#include <sched.h>
#endif

#include <limits.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdlib.h>
#include <wchar.h>

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

// #ifdef __GNUC__
// #pragma GCC visibility push(hidden)
// #endif

#define NVTX_EXTENSION_FRESH 0
#define NVTX_EXTENSION_DISABLED 1
#define NVTX_EXTENSION_STARTING 2
#define NVTX_EXTENSION_LOADED 3

typedef intptr_t (NVTX_API * NvtxExtGetExportFunction_t)(uint32_t exportFunctionId);

typedef struct nvtxExtModuleSegment_t
{
    size_t segmentId;
    size_t slotCount;
    intptr_t* slots;

} nvtxExtModuleSegment_t;

typedef struct nvtxExtModuleInfo_t
{
    uint16_t nvtxVer;
    uint16_t structSize;
    uint16_t moduleId;
    uint16_t compatId;
    size_t segmentsCount;
    nvtxExtModuleSegment_t* segments;
    NvtxExtGetExportFunction_t getExportFunction;
} nvtxExtModuleInfo_t;

typedef int (NVTX_API * NvtxExtInitializeInjectionFunc_t)(nvtxExtModuleInfo_t* moduleInfo);

/* nvtxExtGlobals1_t is for the global storage of slots for function pointers and function tables.
* Slots ranges are pre-assigned to extensions.
* other, potentially larger, globals will be created once there is insufficient room for a new extension.
*/
#define NVTX3EXT_GLOBALS1_SLOT_GROUP_ID 1 /* incrimented with each new ext global we introduce */
#define NVTX3EXT_GLOBALS1_SLOT_COUNT 256
typedef struct nvtxExtGlobals1_t
{
    NvtxExtInitializeInjectionFunc_t injectionFnPtr;
    size_t slotGroupId;
    size_t slotCount;
    intptr_t slots[256];

} nvtxExtGlobals1_t;

NVTX_LINKONCE_DEFINE_GLOBAL nvtxExtGlobals1_t NVTX_VERSIONED_IDENTIFIER(nvtxExtGlobals1) =
{
    (NvtxExtInitializeInjectionFunc_t)0,
    1,
    NVTX3EXT_GLOBALS1_SLOT_COUNT,
    {0}
};



#define NVTX_EXT_INIT_GUARD
#include "nvtxExtInit.h"
#undef NVTX_EXT_INIT_GUARD

// #ifdef __GNUC__
// #pragma GCC visibility pop
// #endif

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* NVTX_EXT_IMPL_H */