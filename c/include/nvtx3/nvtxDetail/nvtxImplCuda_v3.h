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

#ifndef NVTX_IMPL_GUARD_CUDA
#error Never include this file directly -- it is automatically included by nvToolsExtCuda.h (except when NVTX_NO_IMPL is defined).
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


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (NVTX_API * nvtxNameCuDeviceA_impl_fntype)(CUdevice device, const char* name);
typedef void (NVTX_API * nvtxNameCuDeviceW_impl_fntype)(CUdevice device, const wchar_t* name);
typedef void (NVTX_API * nvtxNameCuContextA_impl_fntype)(CUcontext context, const char* name);
typedef void (NVTX_API * nvtxNameCuContextW_impl_fntype)(CUcontext context, const wchar_t* name);
typedef void (NVTX_API * nvtxNameCuStreamA_impl_fntype)(CUstream stream, const char* name);
typedef void (NVTX_API * nvtxNameCuStreamW_impl_fntype)(CUstream stream, const wchar_t* name);
typedef void (NVTX_API * nvtxNameCuEventA_impl_fntype)(CUevent event, const char* name);
typedef void (NVTX_API * nvtxNameCuEventW_impl_fntype)(CUevent event, const wchar_t* name);

NVTX_DECLSPEC void NVTX_API nvtxNameCuDeviceA(CUdevice device, const char* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)device;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCuDeviceA_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCuDeviceA_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(device, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCuDeviceW(CUdevice device, const wchar_t* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)device;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCuDeviceW_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCuDeviceW_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(device, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCuContextA(CUcontext context, const char* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)context;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCuContextA_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxNameCuContextA_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCuContextA_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(context, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCuContextW(CUcontext context, const wchar_t* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)context;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCuContextW_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxNameCuContextW_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCuContextW_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(context, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCuStreamA(CUstream stream, const char* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)stream;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCuStreamA_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxNameCuStreamA_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCuStreamA_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(stream, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCuStreamW(CUstream stream, const wchar_t* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)stream;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCuStreamW_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxNameCuStreamW_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCuStreamW_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(stream, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCuEventA(CUevent event, const char* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)event;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCuEventA_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxNameCuEventA_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCuEventA_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(event, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCuEventW(CUevent event, const wchar_t* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)event;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCuEventW_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxNameCuEventW_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCuEventW_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(event, name);
#endif /* NVTX_DISABLE */
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

