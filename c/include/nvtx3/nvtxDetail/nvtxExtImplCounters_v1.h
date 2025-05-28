/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef NVTX_EXT_IMPL_COUNTERS_GUARD
#error Never include this file directly -- it is automatically included by nvToolsExtCounters.h (except when NVTX_NO_IMPL is defined).
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

#ifndef NVTX_EXT_IMPL_COUNTERS_V1
#define NVTX_EXT_IMPL_COUNTERS_V1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef NVTX_DISABLE

#include "nvtxExtHelperMacros.h"

#define NVTX_EXT_COUNTERS_IMPL_FN_V1(ret_type, fn_name, signature, arg_names) \
NVTX_DECLSPEC ret_type NVTX_API fn_name signature { \
    NVTX_SET_NAME_MANGLING_OPTIONS \
    NVTX_EXT_HELPER_UNUSED_ARGS arg_names \
    NVTX_EXT_FN_RETURN_INVALID(ret_type) \
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
NVTX_LINKONCE_DEFINE_FUNCTION void NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersInitOnce)(void)
{
    intptr_t* fnSlots = NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersSlots) + 1;
    nvtxExtModuleSegment_t segment = {
        0, /* unused (only one segment) */
        NVTX_EXT_COUNTERS_SLOT_COUNT,
        NVTX_NULLPTR /* function slots */
    };

    nvtxExtModuleInfo_t module_info = {
        NVTX_VERSION, sizeof(nvtxExtModuleInfo_t),
        NVTX_EXT_COUNTERS_MODULEID, NVTX_EXT_COUNTERS_COMPATID,
        1, NVTX_NULLPTR, /* number of segments, segments */
        NVTX_NULLPTR, /* no export function needed */
        NVTX_NULLPTR /* no extension private info */
    };

    segment.functionSlots = fnSlots;
    module_info.segments = &segment;

    NVTX_INFO( "%s\n", __FUNCTION__  );

    NVTX_VERSIONED_IDENTIFIER(nvtxExtInitOnce)(&module_info,
        NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersSlots));
}

#define NVTX_EXT_COUNTERS_IMPL_FN_V1(ret_type, fn_name, signature, arg_names) \
typedef ret_type (*fn_name##_impl_fntype)signature; \
NVTX_DECLSPEC ret_type NVTX_API fn_name signature { \
    NVTX_SET_NAME_MANGLING_OPTIONS \
    intptr_t* pSlot = &NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersSlots)[NVTX3EXT_CBID_##fn_name + 1]; \
    intptr_t slot = *pSlot; \
    if (slot != NVTX_EXTENSION_DISABLED) { \
        if (slot != NVTX_EXTENSION_FRESH) { \
            NVTX_EXT_FN_RETURN (*NVTX_REINTERPRET_CAST(fn_name##_impl_fntype, slot)) arg_names; \
        } else { \
            NVTX_EXT_COUNTERS_VERSIONED_ID(nvtxExtCountersInitOnce)(); \
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
#define NVTX_EXT_FN_RETURN_INVALID(rtype) return NVTX_STATIC_CAST(rtype, 0);

NVTX_EXT_COUNTERS_IMPL_FN_V1(uint64_t, nvtxCounterRegister,
    (nvtxDomainHandle_t domain, const nvtxCounterAttr_t* attr),
    (domain, attr))

#undef NVTX_EXT_FN_RETURN
#undef NVTX_EXT_FN_RETURN_INVALID
/* END: Non-void functions. */

/* void functions. */
#define NVTX_EXT_FN_RETURN
#define NVTX_EXT_FN_RETURN_INVALID(rtype)

NVTX_EXT_COUNTERS_IMPL_FN_V1(void, nvtxCounterSampleInt64,
    (nvtxDomainHandle_t domain, uint64_t counterId, int64_t value),
    (domain, counterId, value))

NVTX_EXT_COUNTERS_IMPL_FN_V1(void, nvtxCounterSampleFloat64,
    (nvtxDomainHandle_t domain, uint64_t counterId, double value),
    (domain, counterId, value))

NVTX_EXT_COUNTERS_IMPL_FN_V1(void, nvtxCounterSample,
    (nvtxDomainHandle_t domain, uint64_t counterId, const void* values, size_t size),
    (domain, counterId, values, size))

NVTX_EXT_COUNTERS_IMPL_FN_V1(void, nvtxCounterSampleNoValue,
    (nvtxDomainHandle_t domain, uint64_t counterId, uint8_t reason),
    (domain, counterId, reason))

NVTX_EXT_COUNTERS_IMPL_FN_V1(void, nvtxCounterBatchSubmit,
    (nvtxDomainHandle_t domain, const nvtxCounterBatch_t* counterData),
    (domain, counterData))

#undef NVTX_EXT_FN_RETURN
#undef NVTX_EXT_FN_RETURN_INVALID
/* END: void functions. */

/* Keep NVTX_EXT_COUNTERS_IMPL_FN_V1 defined for a future version of this extension. */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* NVTX_EXT_IMPL_COUNTERS_V1 */
