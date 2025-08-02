/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef NVTX_EXT_IMPL_PAYLOAD_GUARD
#error Never include this file directly -- it is automatically included by nvToolsExtPayload.h (except when NVTX_NO_IMPL is defined).
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

#ifndef NVTX_EXT_IMPL_PAYLOAD_V1
#define NVTX_EXT_IMPL_PAYLOAD_V1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef NVTX_DISABLE

#include "nvtxExtHelperMacros.h"

#define NVTX_EXT_PAYLOAD_IMPL_FN_V1(ret_type, fn_name, signature, arg_names) \
NVTX_DECLSPEC ret_type NVTX_API fn_name signature { \
    NVTX_SET_NAME_MANGLING_OPTIONS \
    NVTX_EXT_HELPER_UNUSED_ARGS arg_names \
    NVTX_EXT_FN_RETURN_INVALID(ret_type) \
}

#define NVTX_EXT_PAYLOAD_IMPL_FN_NOARGS_V1(ret_type, fn_name) \
NVTX_DECLSPEC ret_type NVTX_API fn_name (void) { \
    NVTX_SET_NAME_MANGLING_OPTIONS \
    NVTX_EXT_FN_RETURN_INVALID(ret_type) \
}

#else /* NVTX_DISABLE */

#include "nvtxExtPayloadTypeInfo.h"

/*
 * Function slots for the payload extension. First entry is the module state,
 * initialized to `0` (`NVTX_EXTENSION_FRESH`).
 */
#define NVTX_EXT_PAYLOAD_SLOT_COUNT 63

NVTX_LINKONCE_DEFINE_GLOBAL intptr_t
NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxExtPayloadSlots)[NVTX_EXT_PAYLOAD_SLOT_COUNT + 1]
    = {0};

/* `NVTX_LINKONCE_FWDDECL_FUNCTION` is used to avoid warnings about missing prototypes. */

/* This helper returns always `1` as `uint8_t`. */
NVTX_LINKONCE_FWDDECL_FUNCTION uint8_t NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxReturnOne)(void);
NVTX_LINKONCE_DEFINE_FUNCTION uint8_t NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxReturnOne)(void)
{
    return NVTX_STATIC_CAST(uint8_t, 1);
}

/*
 * If a tool is attached, but does not handle `nvtxDomainIsEnabled`, the latter
 * will always return `1` (enabled).
 */
NVTX_LINKONCE_FWDDECL_FUNCTION void NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxInitIsDomainEnabledFn)(void);
NVTX_LINKONCE_DEFINE_FUNCTION void NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxInitIsDomainEnabledFn)(void)
{
    intptr_t* pSlot = &NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxExtPayloadSlots)[NVTX3EXT_CBID_nvtxDomainIsEnabled + 1];

    /* The initialization disables all slots that have not been set by the tool. */
    if (*pSlot == NVTX_EXTENSION_DISABLED)
    {
        intptr_t* moduleState = NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxExtPayloadSlots);
        int isInitFnSet =
            NVTX_VERSIONED_IDENTIFIER(nvtxExtGlobals1).injectionFnPtr != NVTX_NULLPTR;

        /* Make `nvtxDomainIsEnabled` return `1`, if the tool does not provide an extension
           initialization function or if the tool does not handle `nvtxDomainIsEnabled`. */
        if (*moduleState == NVTX_EXTENSION_DISABLED ||
            (isInitFnSet && *moduleState != NVTX_EXTENSION_INIT_FN_FAILED))
        {
            *pSlot = NVTX_REINTERPRET_CAST(intptr_t, NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxReturnOne));
        }
    }
}

NVTX_LINKONCE_FWDDECL_FUNCTION void NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxExtPayloadInitOnce)(void);
NVTX_LINKONCE_DEFINE_FUNCTION void NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxExtPayloadInitOnce)(void)
{
    intptr_t* fnSlots = NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxExtPayloadSlots) + 1;
    nvtxExtModuleSegment_t segment = {
        0, /* unused (only one segment) */
        NVTX_EXT_PAYLOAD_SLOT_COUNT,
        NVTX_NULLPTR /* function slots */
    };

    nvtxExtModuleInfo_t module_info = {
        NVTX_VERSION, sizeof(nvtxExtModuleInfo_t),
        NVTX_EXT_PAYLOAD_MODULEID, NVTX_EXT_PAYLOAD_COMPATID,
        1, NVTX_NULLPTR, /* number of segments, segments */
        NVTX_NULLPTR, /* no export function needed */
        /* bake type sizes and alignment information into program binary */
        &(NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxExtPayloadTypeInfo))
    };

    segment.functionSlots = fnSlots;
    module_info.segments = &segment;

    NVTX_INFO( "%s\n", __FUNCTION__  );

    NVTX_VERSIONED_IDENTIFIER(nvtxExtInitOnce)(&module_info,
        NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxExtPayloadSlots));

    NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxInitIsDomainEnabledFn)();
}

#define NVTX_EXT_PAYLOAD_IMPL_FN_V1(ret_type, fn_name, signature, arg_names) \
typedef ret_type (*fn_name##_impl_fntype)signature; \
NVTX_DECLSPEC ret_type NVTX_API fn_name signature { \
    NVTX_SET_NAME_MANGLING_OPTIONS \
    intptr_t* pSlot = &NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxExtPayloadSlots)[NVTX3EXT_CBID_##fn_name + 1]; \
    intptr_t slot = *pSlot; \
    if (slot != NVTX_EXTENSION_DISABLED) { \
        if (slot != NVTX_EXTENSION_FRESH) { \
            NVTX_EXT_FN_RETURN (*NVTX_REINTERPRET_CAST(fn_name##_impl_fntype, slot)) arg_names; \
        } else { \
            NVTX_EXT_PAYLOAD_VERSIONED_ID(nvtxExtPayloadInitOnce)(); \
            /* Re-read function slot after extension initialization. */ \
            slot = *pSlot; \
            if (slot != NVTX_EXTENSION_DISABLED && slot != NVTX_EXTENSION_FRESH) { \
                NVTX_EXT_FN_RETURN (*NVTX_REINTERPRET_CAST(fn_name##_impl_fntype, slot)) arg_names; \
            } \
        } \
    } \
    NVTX_EXT_FN_RETURN_INVALID(ret_type) /* No tool attached. */ \
}

#define NVTX_EXT_PAYLOAD_IMPL_FN_NOARGS_V1(ret_type, fn_name) \
    NVTX_EXT_PAYLOAD_IMPL_FN_V1(ret_type, fn_name, (void), ())

#endif /* NVTX_DISABLE */

/* Push/pop functions return `NVTX_NO_PUSH_POP_TRACKING` if no tool is attached. */
#define NVTX_EXT_FN_RETURN return
#define NVTX_EXT_FN_RETURN_INVALID(rtype) return NVTX_NO_PUSH_POP_TRACKING;

NVTX_EXT_PAYLOAD_IMPL_FN_V1(int, nvtxRangePushPayload,
    (nvtxDomainHandle_t domain, const nvtxPayloadData_t* payloadData, size_t count),
    (domain, payloadData, count))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(int, nvtxRangePopPayload,
    (nvtxDomainHandle_t domain, const nvtxPayloadData_t* payloadData, size_t count),
    (domain, payloadData, count))

#undef NVTX_EXT_FN_RETURN
#undef NVTX_EXT_FN_RETURN_INVALID

/* Non-void functions. */
#define NVTX_EXT_FN_RETURN return
#define NVTX_EXT_FN_RETURN_INVALID(rtype) return NVTX_STATIC_CAST(rtype, 0);

NVTX_EXT_PAYLOAD_IMPL_FN_V1(uint64_t, nvtxPayloadSchemaRegister,
    (nvtxDomainHandle_t domain, const nvtxPayloadSchemaAttr_t* attr),
    (domain, attr))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(uint64_t, nvtxPayloadEnumRegister,
    (nvtxDomainHandle_t domain, const nvtxPayloadEnumAttr_t* attr),
    (domain, attr))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(nvtxRangeId_t, nvtxRangeStartPayload,
    (nvtxDomainHandle_t domain, const nvtxPayloadData_t* payloadData, size_t count),
    (domain, payloadData, count))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(uint8_t, nvtxDomainIsEnabled, (nvtxDomainHandle_t domain), (domain))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(uint64_t, nvtxScopeRegister, (nvtxDomainHandle_t domain,
    const nvtxScopeAttr_t* attr), (domain, attr))

NVTX_EXT_PAYLOAD_IMPL_FN_NOARGS_V1(int64_t, nvtxTimestampGet)

NVTX_EXT_PAYLOAD_IMPL_FN_V1(uint64_t, nvtxTimeDomainRegister,
    (nvtxDomainHandle_t domain, const nvtxTimeDomainAttr_t* attr),
    (domain, attr))

#undef NVTX_EXT_FN_RETURN
#undef NVTX_EXT_FN_RETURN_INVALID
/* END: Non-void functions. */

/* void functions. */
#define NVTX_EXT_FN_RETURN
#define NVTX_EXT_FN_RETURN_INVALID(rtype)

NVTX_EXT_PAYLOAD_IMPL_FN_V1(void, nvtxMarkPayload, (nvtxDomainHandle_t domain,
    const nvtxPayloadData_t* payloadData, size_t count), (domain, payloadData, count))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(void, nvtxRangeEndPayload, (nvtxDomainHandle_t domain,
    nvtxRangeId_t id, const nvtxPayloadData_t* payloadData, size_t count),
    (domain, id, payloadData, count))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(void, nvtxTimerSource,
    (nvtxDomainHandle_t domain, uint64_t timeDomainId, uint64_t flags, int64_t (*timestampProviderFn)(void)),
    (domain, timeDomainId, flags, timestampProviderFn))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(void, nvtxTimerSourceWithData,
    (nvtxDomainHandle_t domain, uint64_t timeDomainId, uint64_t flags, int64_t (*timestampProviderFn)(void* data), void* data),
    (domain, timeDomainId, flags, timestampProviderFn, data))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(void, nvtxTimeSyncPoint,
    (nvtxDomainHandle_t domain, uint64_t timeDomainId1, uint64_t timeDomainId2,
        int64_t timestamp1, int64_t timestamp2),
    (domain, timeDomainId1, timeDomainId2, timestamp1, timestamp2))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(void, nvtxTimeSyncPointTable,
    (nvtxDomainHandle_t domain, uint64_t timeDomainIdSrc, uint64_t timeDomainIdDst,
    const nvtxSyncPoint_t* syncPoints, size_t count),
    (domain, timeDomainIdSrc, timeDomainIdDst, syncPoints, count))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(void, nvtxTimestampConversionFactor,
    (nvtxDomainHandle_t domain, uint64_t timeDomainIdSrc, uint64_t timeDomainIdDst,
        double slope, int64_t timestampSrc, int64_t timestampDst),
    (domain, timeDomainIdSrc, timeDomainIdDst, slope, timestampSrc, timestampDst))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(void, nvtxEventSubmit,
    (nvtxDomainHandle_t domain, const nvtxPayloadData_t* payloadData, size_t numPayloads),
    (domain, payloadData, numPayloads))

NVTX_EXT_PAYLOAD_IMPL_FN_V1(void, nvtxEventBatchSubmit, (nvtxDomainHandle_t domain,
    const nvtxEventBatch_t* eventBatch), (domain, eventBatch))

#undef NVTX_EXT_FN_RETURN
#undef NVTX_EXT_FN_RETURN_INVALID
/* END: void functions. */

/* Keep NVTX_EXT_PAYLOAD_IMPL_FN_V1 defined for a future version of this extension. */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* NVTX_EXT_IMPL_PAYLOAD_V1 */
