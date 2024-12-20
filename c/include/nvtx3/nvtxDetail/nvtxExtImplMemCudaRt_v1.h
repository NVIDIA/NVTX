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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NVTX_EXT_FN_IMPL(ret_type, fn_name, signature, arg_names) \
typedef ret_type ( * fn_name##_impl_fntype )signature; \
    NVTX_DECLSPEC ret_type NVTX_API fn_name signature { \
    intptr_t* pSlot = &NVTX_EXT_MEM_VERSIONED_ID(nvtxExtMemSlots)[NVTX3EXT_CBID_##fn_name]; \
    intptr_t slot = *pSlot; \
    if (slot != NVTX_EXTENSION_DISABLED) { \
        if (slot != NVTX_EXTENSION_FRESH) { \
            return (*(fn_name##_impl_fntype)slot) arg_names; \
        } else { \
            NVTX_EXT_MEM_VERSIONED_ID(nvtxExtMemInitOnce)(); \
            /* Re-read function slot after extension initialization. */ \
            slot = *pSlot; \
            if (slot != NVTX_EXTENSION_DISABLED && slot != NVTX_EXTENSION_FRESH) { \
                return (*(fn_name##_impl_fntype)slot) arg_names; \
            } \
        } \
    } \
    NVTX_EXT_FN_RETURN_INVALID(ret_type) \
}

/* Non-void functions. */
#define NVTX_EXT_FN_RETURN_INVALID(rtype) return (rtype)0;

NVTX_EXT_FN_IMPL(nvtxMemPermissionsHandle_t, nvtxMemCudaGetProcessWidePermissions, (nvtxDomainHandle_t domain), (domain))

NVTX_EXT_FN_IMPL(nvtxMemPermissionsHandle_t, nvtxMemCudaGetDeviceWidePermissions, (nvtxDomainHandle_t domain, int device), (domain, device))

#undef NVTX_EXT_FN_RETURN_INVALID
/* END: Non-void functions. */

/* void functions. */
#define NVTX_EXT_FN_RETURN_INVALID(rtype)
#define return

NVTX_EXT_FN_IMPL(void, nvtxMemCudaSetPeerAccess, (nvtxDomainHandle_t domain, nvtxMemPermissionsHandle_t permissions, int devicePeer, uint32_t flags), (domain, permissions, devicePeer, flags))

NVTX_EXT_FN_IMPL(void, nvtxMemCudaMarkInitialized, (nvtxDomainHandle_t domain, cudaStream_t stream, uint8_t isPerThreadStream, nvtxMemMarkInitializedBatch_t const* desc), (domain, stream, isPerThreadStream, desc))

#undef return
#undef NVTX_EXT_FN_RETURN_INVALID
/* END: void functions. */

#undef NVTX_EXT_FN_IMPL

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
