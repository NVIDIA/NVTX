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

#ifndef NVTX_EXT_IMPL_MEM_CUDART_GUARD
#error Never include this file directly -- it is automatically included by nvToolsExtMemCudaRt.h (except when NVTX_NO_IMPL is defined).
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

#ifndef NVTX_EXT_IMPL_MEM_CUDART_V1
#define NVTX_EXT_IMPL_MEM_CUDART_V1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* NVTX_EXT_MEM_IMPL_FN_V1 defined in nvtxExtImplMem_v1.h */

/* Non-void functions. */
#define NVTX_EXT_FN_RETURN return
#define NVTX_EXT_FN_RETURN_INVALID(rtype) return NVTX_NULLPTR;

NVTX_EXT_MEM_IMPL_FN_V1(nvtxMemPermissionsHandle_t, nvtxMemCudaGetProcessWidePermissions, (nvtxDomainHandle_t domain), (domain))

NVTX_EXT_MEM_IMPL_FN_V1(nvtxMemPermissionsHandle_t, nvtxMemCudaGetDeviceWidePermissions, (nvtxDomainHandle_t domain, int device), (domain, device))

#undef NVTX_EXT_FN_RETURN
#undef NVTX_EXT_FN_RETURN_INVALID
/* END: Non-void functions. */

/* void functions. */
#define NVTX_EXT_FN_RETURN
#define NVTX_EXT_FN_RETURN_INVALID(rtype)

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemCudaSetPeerAccess, (nvtxDomainHandle_t domain, nvtxMemPermissionsHandle_t permissions, int devicePeer, uint32_t flags), (domain, permissions, devicePeer, flags))

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemCudaMarkInitialized, (nvtxDomainHandle_t domain, cudaStream_t stream, uint8_t isPerThreadStream, nvtxMemMarkInitializedBatch_t const* desc), (domain, stream, isPerThreadStream, desc))

#undef NVTX_EXT_FN_RETURN
#undef NVTX_EXT_FN_RETURN_INVALID
/* END: void functions. */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* NVTX_EXT_IMPL_MEM_CUDART_V1 */
