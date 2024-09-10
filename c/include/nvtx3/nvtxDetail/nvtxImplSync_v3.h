/*
 * SPDX-FileCopyrightText: Copyright (c) 2009-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef NVTX_IMPL_GUARD_SYNC
#error Never include this file directly -- it is automatically included by nvToolsExtCuda.h (except when NVTX_NO_IMPL is defined).
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
#ifndef NVTX_DISABLE
    nvtxDomainSyncUserCreate_impl_fntype local = (nvtxDomainSyncUserCreate_impl_fntype)NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainSyncUserCreate_impl_fnptr;
    if(local!=0)
        return (*local)(domain, attribs);
    else
#endif  /*NVTX_DISABLE*/
        return (nvtxSyncUser_t)0;
}

NVTX_DECLSPEC void NVTX_API nvtxDomainSyncUserDestroy(nvtxSyncUser_t handle)
{
#ifndef NVTX_DISABLE
    nvtxDomainSyncUserDestroy_impl_fntype local = (nvtxDomainSyncUserDestroy_impl_fntype)NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainSyncUserDestroy_impl_fnptr;
    if(local!=0)
        (*local)(handle);
#endif /*NVTX_DISABLE*/
}

NVTX_DECLSPEC void NVTX_API nvtxDomainSyncUserAcquireStart(nvtxSyncUser_t handle)
{
#ifndef NVTX_DISABLE
    nvtxDomainSyncUserAcquireStart_impl_fntype local = (nvtxDomainSyncUserAcquireStart_impl_fntype)NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainSyncUserAcquireStart_impl_fnptr;
    if(local!=0)
        (*local)(handle);
#endif /*NVTX_DISABLE*/
}

NVTX_DECLSPEC void NVTX_API nvtxDomainSyncUserAcquireFailed(nvtxSyncUser_t handle)
{
#ifndef NVTX_DISABLE
    nvtxDomainSyncUserAcquireFailed_impl_fntype local = (nvtxDomainSyncUserAcquireFailed_impl_fntype)NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainSyncUserAcquireFailed_impl_fnptr;
    if(local!=0)
        (*local)(handle);
#endif /*NVTX_DISABLE*/
}

NVTX_DECLSPEC void NVTX_API nvtxDomainSyncUserAcquireSuccess(nvtxSyncUser_t handle)
{
#ifndef NVTX_DISABLE
    nvtxDomainSyncUserAcquireSuccess_impl_fntype local = (nvtxDomainSyncUserAcquireSuccess_impl_fntype)NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainSyncUserAcquireSuccess_impl_fnptr;
    if(local!=0)
        (*local)(handle);
#endif /*NVTX_DISABLE*/
}

NVTX_DECLSPEC void NVTX_API nvtxDomainSyncUserReleasing(nvtxSyncUser_t handle)
{
#ifndef NVTX_DISABLE
    nvtxDomainSyncUserReleasing_impl_fntype local = (nvtxDomainSyncUserReleasing_impl_fntype)NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainSyncUserReleasing_impl_fnptr;
    if(local!=0)
        (*local)(handle);
#endif /*NVTX_DISABLE*/
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
