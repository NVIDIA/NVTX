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

#ifndef NVTX_IMPL_GUARD_CUDART
#error Never include this file directly -- it is automatically included by nvToolsExtCudaRt.h (except when NVTX_NO_IMPL is defined).
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

typedef void (NVTX_API * nvtxNameCudaDeviceA_impl_fntype)(int device, const char* name);
typedef void (NVTX_API * nvtxNameCudaDeviceW_impl_fntype)(int device, const wchar_t* name);
typedef void (NVTX_API * nvtxNameCudaStreamA_impl_fntype)(cudaStream_t stream, const char* name);
typedef void (NVTX_API * nvtxNameCudaStreamW_impl_fntype)(cudaStream_t stream, const wchar_t* name);
typedef void (NVTX_API * nvtxNameCudaEventA_impl_fntype)(cudaEvent_t event, const char* name);
typedef void (NVTX_API * nvtxNameCudaEventW_impl_fntype)(cudaEvent_t event, const wchar_t* name);

NVTX_DECLSPEC void NVTX_API nvtxNameCudaDeviceA(int device, const char* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)device;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCudaDeviceA_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCudaDeviceA_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(device, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCudaDeviceW(int device, const wchar_t* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)device;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCudaDeviceW_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCudaDeviceW_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(device, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCudaStreamA(cudaStream_t stream, const char* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)stream;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCudaStreamA_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxNameCudaStreamA_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCudaStreamA_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(stream, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCudaStreamW(cudaStream_t stream, const wchar_t* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)stream;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCudaStreamW_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxNameCudaStreamW_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCudaStreamW_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(stream, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCudaEventA(cudaEvent_t event, const char* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)event;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCudaEventA_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxNameCudaEventA_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCudaEventA_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(event, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCudaEventW(cudaEvent_t event, const wchar_t* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)event;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCudaEventW_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxNameCudaEventW_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCudaEventW_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(event, name);
#endif /* NVTX_DISABLE */
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

