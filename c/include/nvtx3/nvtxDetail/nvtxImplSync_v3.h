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

#ifndef NVTX_IMPL_GUARD_SYNC
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

typedef nvtxSyncUser_t (NVTX_API * nvtxDomainSyncUserCreate_impl_fntype)(nvtxDomainHandle_t domain, const nvtxSyncUserAttributes_t* attribs);
typedef void (NVTX_API * nvtxDomainSyncUserDestroy_impl_fntype)(nvtxSyncUser_t handle);
typedef void (NVTX_API * nvtxDomainSyncUserAcquireStart_impl_fntype)(nvtxSyncUser_t handle);
typedef void (NVTX_API * nvtxDomainSyncUserAcquireFailed_impl_fntype)(nvtxSyncUser_t handle);
typedef void (NVTX_API * nvtxDomainSyncUserAcquireSuccess_impl_fntype)(nvtxSyncUser_t handle);
typedef void (NVTX_API * nvtxDomainSyncUserReleasing_impl_fntype)(nvtxSyncUser_t handle);

NVTX_DECLSPEC nvtxSyncUser_t NVTX_API nvtxDomainSyncUserCreate(nvtxDomainHandle_t domain, const nvtxSyncUserAttributes_t* attribs)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)domain;
    (void)attribs;
#else /* NVTX_DISABLE */
    nvtxDomainSyncUserCreate_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxDomainSyncUserCreate_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainSyncUserCreate_impl_fnptr);
    if (local != NVTX_NULLPTR)
        return (*local)(domain, attribs);
    else
#endif /* NVTX_DISABLE */
        return NVTX_NULLPTR;
}

NVTX_DECLSPEC void NVTX_API nvtxDomainSyncUserDestroy(nvtxSyncUser_t handle)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)handle;
#else /* NVTX_DISABLE */
    nvtxDomainSyncUserDestroy_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxDomainSyncUserDestroy_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainSyncUserDestroy_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(handle);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxDomainSyncUserAcquireStart(nvtxSyncUser_t handle)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)handle;
#else /* NVTX_DISABLE */
    nvtxDomainSyncUserAcquireStart_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxDomainSyncUserAcquireStart_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainSyncUserAcquireStart_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(handle);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxDomainSyncUserAcquireFailed(nvtxSyncUser_t handle)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)handle;
#else /* NVTX_DISABLE */
    nvtxDomainSyncUserAcquireFailed_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxDomainSyncUserAcquireFailed_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainSyncUserAcquireFailed_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(handle);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxDomainSyncUserAcquireSuccess(nvtxSyncUser_t handle)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)handle;
#else /* NVTX_DISABLE */
    nvtxDomainSyncUserAcquireSuccess_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxDomainSyncUserAcquireSuccess_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainSyncUserAcquireSuccess_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(handle);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxDomainSyncUserReleasing(nvtxSyncUser_t handle)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)handle;
#else /* NVTX_DISABLE */
    nvtxDomainSyncUserReleasing_impl_fntype local = NVTX_REINTERPRET_CAST(nvtxDomainSyncUserReleasing_impl_fntype, NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainSyncUserReleasing_impl_fnptr);
    if (local != NVTX_NULLPTR)
        (*local)(handle);
#endif /* NVTX_DISABLE */
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
