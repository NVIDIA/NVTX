/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef NVTX_EXT_IMPL_COUNTERS_GUARD
#error Never include this file directly -- it is automatically included by nvToolsExtCounters.h (except when NVTX_NO_IMPL is defined).
#endif

#define NVTX_EXT_IMPL_GUARD
#include "nvtxExtImpl.h"
#undef NVTX_EXT_IMPL_GUARD

#ifndef NVTX_EXT_IMPL_COUNTERS_V1
#define NVTX_EXT_IMPL_COUNTERS_V1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Macros to create versioned symbols. */
#define NVTX_EXT_COUNTERS_VERSIONED_IDENTIFIER_L3(NAME, VERSION, COMPATID) \
    NAME##_v##VERSION##_bpl##COMPATID
#define NVTX_EXT_COUNTERS_VERSIONED_IDENTIFIER_L2(NAME, VERSION, COMPATID) \
    NVTX_EXT_COUNTERS_VERSIONED_IDENTIFIER_L3(NAME, VERSION, COMPATID)
#define NVTX_EXT_COUNTERS_VERSIONED_ID(NAME) \
    NVTX_EXT_COUNTERS_VERSIONED_IDENTIFIER_L2(NAME, NVTX_VERSION, NVTX_EXT_COUNTERS_COMPATID)

#ifdef NVTX_DISABLE

#include "nvtxExtHelperMacros.h"

#define NVTX_EXT_COUNTERS_IMPL_FN_V1(ret_val, fn_name, signature, arg_names) \
ret_val fn_name signature { \
    NVTX_EXT_HELPER_UNUSED_ARGS arg_names \
    return ((ret_val)(intptr_t)-1); \
}

#else /* NVTX_DISABLE */

/*
 * Function slots for the counters extension. First entry is the module state,
 * initialized to `0` (`NVTX_EXTENSION_FRESH`).
 */
#define NVTX_EXT_COUNTERS_SLOT_COUNT 63
NVTX_LINKONCE_DEFINE_GLOBAL intptr_t
NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersSlots)[NVTX_EXT_COUNTERS_SLOT_COUNT + 1]
    = {0};

/* Avoid warnings about missing prototype. */
NVTX_LINKONCE_FWDDECL_FUNCTION void NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersInitOnce)(void);
NVTX_LINKONCE_DEFINE_FUNCTION void NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersInitOnce)()
{
    intptr_t* fnSlots = NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersSlots) + 1;
    nvtxExtModuleSegment_t segment = {
        0, /* unused (only one segment) */
        NVTX_EXT_COUNTERS_SLOT_COUNT,
        fnSlots
    };

    nvtxExtModuleInfo_t module = {
        NVTX_VERSION, sizeof(nvtxExtModuleInfo_t),
        NVTX_EXT_COUNTERS_MODULEID, NVTX_EXT_COUNTERS_COMPATID,
        1, &segment, /* number of segments, segments */
        NULL, /* no export function needed */
        /* bake type sizes and alignment information into program binary */
        NULL
    };

    NVTX_INFO( "%s\n", __FUNCTION__  );

    NVTX_VERSIONED_IDENTIFIER(nvtxExtInitOnce)(&module,
        NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersSlots));
}

#define NVTX_EXT_COUNTERS_IMPL_FN_V1(ret_type, fn_name, signature, arg_names) \
typedef ret_type (*fn_name##_impl_fntype)signature; \
NVTX_DECLSPEC ret_type NVTX_API fn_name signature { \
    intptr_t slot = NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersSlots)[NVTX3EXT_CBID_##fn_name + 1]; \
    if (slot != NVTX_EXTENSION_DISABLED) { \
        if (slot != NVTX_EXTENSION_FRESH) { \
            return (*(fn_name##_impl_fntype)slot) arg_names; \
        } else { \
            NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersInitOnce)(); \
            /* Re-read function slot after extension initialization. */ \
            slot = NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersSlots)[NVTX3EXT_CBID_##fn_name + 1]; \
            if (slot != NVTX_EXTENSION_DISABLED && slot != NVTX_EXTENSION_FRESH) { \
                return (*(fn_name##_impl_fntype)slot) arg_names; \
            } \
        } \
    } \
    NVTX_EXT_FN_RETURN_INVALID(ret_type) \
}

#endif /*NVTX_DISABLE*/

/* Non-void functions. */
#define NVTX_EXT_FN_RETURN_INVALID(rtype) return ((rtype)(intptr_t)-1);

NVTX_EXT_COUNTERS_IMPL_FN_V1(uint64_t, nvtxCountersRegister,
    (nvtxDomainHandle_t domain, const nvtxCountersAttr_t* attr),
    (domain, attr))

#undef NVTX_EXT_FN_RETURN_INVALID
/* END: Non-void functions. */

/* void functions. */
#define NVTX_EXT_FN_RETURN_INVALID(rtype)
#define return

NVTX_EXT_COUNTERS_IMPL_FN_V1(void, nvtxCountersSampleInt64,
    (nvtxDomainHandle_t domain, uint64_t countersId, int64_t value),
    (domain, countersId, value))

NVTX_EXT_COUNTERS_IMPL_FN_V1(void, nvtxCountersSampleFloat64,
    (nvtxDomainHandle_t domain, uint64_t countersId, double value),
    (domain, countersId, value))

NVTX_EXT_COUNTERS_IMPL_FN_V1(void, nvtxCountersSample,
    (nvtxDomainHandle_t domain, uint64_t countersId, const void* values, size_t size),
    (domain, countersId, values, size))

NVTX_EXT_COUNTERS_IMPL_FN_V1(void, nvtxCountersSampleNoValue,
    (nvtxDomainHandle_t domain, uint64_t countersId, uint8_t reason),
    (domain, countersId, reason))

#undef return
#undef NVTX_EXT_FN_RETURN_INVALID
/* END: void functions. */

/* Keep NVTX_EXT_COUNTERS_IMPL_FN_V1 defined for a future version of this extension. */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* NVTX_EXT_IMPL_COUNTERS_V1 */