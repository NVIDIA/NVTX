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

#ifndef NVTX_EXT_IMPL_MEM_GUARD
#error Never include this file directly -- it is automatically included by nvToolsExtMem.h (except when NVTX_NO_IMPL is defined).
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

#define NVTX_EXT_IMPL_GUARD
#include "nvtxExtImpl.h"
#undef NVTX_EXT_IMPL_GUARD

#ifndef NVTX_EXT_IMPL_MEM_V1
#define NVTX_EXT_IMPL_MEM_V1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef NVTX_DISABLE

#include "nvtxExtHelperMacros.h"

#define NVTX_EXT_MEM_IMPL_FN_V1(ret_type, fn_name, signature, arg_names) \
NVTX_DECLSPEC ret_type NVTX_API fn_name signature { \
    NVTX_SET_NAME_MANGLING_OPTIONS \
    NVTX_EXT_HELPER_UNUSED_ARGS arg_names \
    NVTX_EXT_FN_RETURN_INVALID(ret_type) \
}

#else /* NVTX_DISABLE */

/*
 * Function slots for the memory extension. First entry is the module
 * state, initialized to `0` (`NVTX_EXTENSION_FRESH`).
 */
#define NVTX_EXT_MEM_SLOT_COUNT 63

NVTX_LINKONCE_DEFINE_GLOBAL intptr_t
NVTX_EXT_MEM_VERSIONED_ID(nvtxExtMemSlots)[NVTX_EXT_MEM_SLOT_COUNT + 1]
    = {0};

/* Avoid warnings about missing prototype. */
NVTX_LINKONCE_FWDDECL_FUNCTION void NVTX_EXT_MEM_VERSIONED_ID(nvtxExtMemInitOnce)(void);
NVTX_LINKONCE_DEFINE_FUNCTION void NVTX_EXT_MEM_VERSIONED_ID(nvtxExtMemInitOnce)(void)
{
    intptr_t* fnSlots = NVTX_EXT_MEM_VERSIONED_ID(nvtxExtMemSlots) + 1;
    nvtxExtModuleSegment_t segment = {
        1, /* only one segment, hard-code ID */
        NVTX_EXT_MEM_SLOT_COUNT,
        NVTX_NULLPTR /* function slots */
    };

    nvtxExtModuleInfo_t module_info = {
        NVTX_VERSION, sizeof(nvtxExtModuleInfo_t),
        NVTX_EXT_MODULEID_MEM, NVTX_EXT_COMPATID_MEM,
        1, NVTX_NULLPTR, /* number of segments, segments */
        NVTX_NULLPTR, /* no export function needed */
        NVTX_NULLPTR /* no extension private info */
    };

    segment.functionSlots = fnSlots;
    module_info.segments = &segment;

    NVTX_INFO( "%s\n", __FUNCTION__  );

    NVTX_VERSIONED_IDENTIFIER(nvtxExtInitOnce)(&module_info,
        NVTX_EXT_MEM_VERSIONED_ID(nvtxExtMemSlots));
}

#define NVTX_EXT_MEM_IMPL_FN_V1(ret_type, fn_name, signature, arg_names) \
typedef ret_type (*fn_name##_impl_fntype)signature; \
NVTX_DECLSPEC ret_type NVTX_API fn_name signature { \
    NVTX_SET_NAME_MANGLING_OPTIONS \
    intptr_t* pSlot = &NVTX_EXT_MEM_VERSIONED_ID(nvtxExtMemSlots)[NVTX3EXT_CBID_##fn_name + 1]; \
    intptr_t slot = *pSlot; \
    if (slot != NVTX_EXTENSION_DISABLED) { \
        if (slot != NVTX_EXTENSION_FRESH) { \
            NVTX_EXT_FN_RETURN (*NVTX_REINTERPRET_CAST(fn_name##_impl_fntype, slot)) arg_names; \
        } else { \
            NVTX_EXT_MEM_VERSIONED_ID(nvtxExtMemInitOnce)(); \
            /* Re-read function slot after extension initialization. */ \
            slot = *pSlot; \
            if (slot != NVTX_EXTENSION_DISABLED && slot != NVTX_EXTENSION_FRESH) { \
                NVTX_EXT_FN_RETURN (*NVTX_REINTERPRET_CAST(fn_name##_impl_fntype, slot)) arg_names; \
            } \
        } \
    } \
    NVTX_EXT_FN_RETURN_INVALID(ret_type) /* No tool attached. */ \
}

#endif /* NVTX_DISABLE */

/* Non-void functions. */
#define NVTX_EXT_FN_RETURN return
#define NVTX_EXT_FN_RETURN_INVALID(rtype) return NVTX_NULLPTR;

NVTX_EXT_MEM_IMPL_FN_V1(nvtxMemHeapHandle_t, nvtxMemHeapRegister, (nvtxDomainHandle_t domain, nvtxMemHeapDesc_t const* desc), (domain, desc))

NVTX_EXT_MEM_IMPL_FN_V1(nvtxMemPermissionsHandle_t, nvtxMemPermissionsCreate, (nvtxDomainHandle_t domain, int32_t creationflags), (domain, creationflags))

#undef NVTX_EXT_FN_RETURN
#undef NVTX_EXT_FN_RETURN_INVALID
/* END: Non-void functions. */

/* void functions. */
#define NVTX_EXT_FN_RETURN
#define NVTX_EXT_FN_RETURN_INVALID(rtype)

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemHeapUnregister, (nvtxDomainHandle_t domain, nvtxMemHeapHandle_t heap), (domain, heap))

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemHeapReset, (nvtxDomainHandle_t domain, nvtxMemHeapHandle_t heap), (domain, heap))

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemRegionsRegister, (nvtxDomainHandle_t domain, nvtxMemRegionsRegisterBatch_t const* desc), (domain, desc))

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemRegionsResize, (nvtxDomainHandle_t domain, nvtxMemRegionsResizeBatch_t const* desc), (domain, desc))

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemRegionsUnregister, (nvtxDomainHandle_t domain, nvtxMemRegionsUnregisterBatch_t const* desc), (domain, desc))

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemRegionsName, (nvtxDomainHandle_t domain, nvtxMemRegionsNameBatch_t const* desc), (domain, desc))

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemPermissionsAssign, (nvtxDomainHandle_t domain, nvtxMemPermissionsAssignBatch_t const* desc), (domain, desc))

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemPermissionsDestroy, (nvtxDomainHandle_t domain, nvtxMemPermissionsHandle_t permissions), (domain, permissions))

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemPermissionsReset, (nvtxDomainHandle_t domain, nvtxMemPermissionsHandle_t permissions), (domain, permissions))

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemPermissionsBind, (nvtxDomainHandle_t domain, nvtxMemPermissionsHandle_t permissions, uint32_t bindScope, uint32_t bindFlags), (domain, permissions, bindScope, bindFlags))

NVTX_EXT_MEM_IMPL_FN_V1(void, nvtxMemPermissionsUnbind, (nvtxDomainHandle_t domain, uint32_t bindScope), (domain, bindScope))

#undef NVTX_EXT_FN_RETURN
#undef NVTX_EXT_FN_RETURN_INVALID
/* END: void functions. */

/* Keep NVTX_EXT_MEM_IMPL_FN_V1 defined for a future version of this extension. */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* NVTX_EXT_IMPL_MEM_V1 */
