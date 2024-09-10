/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "nvToolsExtPayload.h"
#include "nvToolsExtSemanticsCounters.h"

#ifndef NVTOOLSEXT_COUNTERS_H
#define NVTOOLSEXT_COUNTERS_H

/**
 * \brief The compatibility ID is used for versioning of this extension.
 */
#ifndef NVTX_EXT_COUNTERS_COMPATID
#define NVTX_EXT_COUNTERS_COMPATID 0x0102
#endif

/**
 * \brief The module ID identifies the payload extension. It has to be unique
 * among the extension modules.
 */
#ifndef NVTX_EXT_COUNTERS_MODULEID
#define NVTX_EXT_COUNTERS_MODULEID 4
#endif

/** The counters ID is not specified. */
#define NVTX_COUNTERS_ID_NONE          0

/** Static (user-provided, feed-forward) counter (group) IDs. */
#define NVTX_COUNTERS_ID_STATIC_START  (1 << 24)

/** Dynamically (tool) generated counter (group) IDs */
#define NVTX_COUNTERS_ID_DYNAMIC_START ((uint64_t)1 << 32)

/** Reasons for the missing sample value. */
#define NVTX_COUNTERS_SAMPLE_ZERO        0
#define NVTX_COUNTERS_SAMPLE_UNCHANGED   1
#define NVTX_COUNTERS_SAMPLE_UNAVAILABLE 2 /* Failed to get a counter sample. */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef NVTX_COUNTERS_TYPEDEFS_V1
#define NVTX_COUNTERS_TYPEDEFS_V1

/**
 * \brief Attributes of a counter or counter group.
 */
typedef struct nvtxCountersAttr_v1
{
    size_t structSize;

    /**
     * A schema ID referring to the data layout of the counter group or a
     * predefined NVTX payloads number type.
     */
    uint64_t schemaId;

    /** Name of the counter (group). */
    const char* name;

    /**
     * Optional detailed description of the counter (group). A description for
     * individual counters can be set in the schema registration.
     */
    const char* description;

    /**
     * Identifier of the counters' scope. A valid scope ID is either a
     * predefined scope or the value returned by `nvtxScopeRegister` called for
     * the same NVTX domain as `nvtxCountersRegister`. An invalid scope ID will
     * be handled like `NVTX_SCOPE_NONE`.
     */
    uint64_t scopeId;

    /**
     * Optional semantics for a counter (group). The specified semantics apply
     * to all counters in a group. If the semantics should only refer to a
     * single counter in a group, the semantics field of the payload entry has
     * to be used. Accepted semantics are `nvtxSemanticsCounter_t` and
     * `nvtxSemanticsTime_t`.
     */
    const nvtxSemanticsHeader_t* semantics;

    /**
     * A static counters ID must be unique within the domain,
   	 * >= NVTX_COUNTERS_ID_STATIC_START, and < NVTX_COUNTERS_ID_DYNAMIC_START.
     * Use NVTX_COUNTERS_ID_NONE to let the tool create a (dynamic) counters ID.
     */
    uint64_t countersId;
} nvtxCountersAttr_t;

#endif /* NVTX_COUNTERS_TYPEDEFS_V1 */

#ifndef NVTX_COUNTERS_API_FUNCTIONS_V1
#define NVTX_COUNTERS_API_FUNCTIONS_V1

/**
 * \brief Register a counter (group).
 *
 * @param hDomain NVTX domain handle.
 * @param attr Pointer to the attributes of the counter (group).
 *
 * @return Identifier of a counter (group). The counters ID is unique within
 *         the NVTX domain.
 */
NVTX_DECLSPEC uint64_t NVTX_API nvtxCountersRegister(
    nvtxDomainHandle_t hDomain,
    const nvtxCountersAttr_t* attr);

/**
 * \brief Sample one integer counter by value immediately (the NVTX tool determines the timestamp).
 *
 * @param hDomain handle of the NVTX domain.
 * @param countersId identifier of the NVTX counter (group).
 * @param value 64-bit integer counter value.
 */
NVTX_DECLSPEC void NVTX_API nvtxCountersSampleInt64(
    nvtxDomainHandle_t hDomain,
    uint64_t countersId,
    int64_t value);

/**
 * \brief Sample one floating point counter by value immediately (the NVTX tool determines the timestamp).
 *
 * @param hDomain handle of the NVTX domain.
 * @param countersId identifier of the NVTX counter (group).
 * @param value 64-bit floating-point counter value.
 */
NVTX_DECLSPEC void NVTX_API nvtxCountersSampleFloat64(
    nvtxDomainHandle_t hDomain,
    uint64_t countersId,
    double value);

/**
 * \brief Sample a counter group by reference immediately (the NVTX tool determines the timestamp).
 *
 * @param hDomain handle of the NVTX domain.
 * @param countersId identifier of the NVTX counter (group).
 * @param counters pointer to one or more counter values.
 * @param size size of the counter value(s) in bytes.
 */
NVTX_DECLSPEC void NVTX_API nvtxCountersSample(
    nvtxDomainHandle_t hDomain,
    uint64_t countersId,
    const void* values,
    size_t size);

/**
 * \brief Sample without value.
 *
 * @param hDomain handle of the NVTX domain.
 * @param countersId identifier of the NVTX counter (group).
 * @param reason reason for the missing sample value.
 */
NVTX_DECLSPEC void NVTX_API nvtxCountersSampleNoValue(
    nvtxDomainHandle_t hDomain,
    uint64_t countersId,
    uint8_t reason);

#endif /* NVTX_COUNTERS_API_FUNCTIONS_V1 */

#ifndef NVTX_COUNTERS_CALLBACK_ID_V1
#define NVTX_COUNTERS_CALLBACK_ID_V1

#define NVTX3EXT_CBID_nvtxCountersRegister           0
#define NVTX3EXT_CBID_nvtxCountersSampleInt64        1
#define NVTX3EXT_CBID_nvtxCountersSampleFloat64      2
#define NVTX3EXT_CBID_nvtxCountersSample             3
#define NVTX3EXT_CBID_nvtxCountersSampleNoValue      4

#endif /* NVTX_COUNTERS_CALLBACK_ID_V1 */

#ifdef __GNUC__
#pragma GCC visibility push(internal)
#endif

#define NVTX_EXT_TYPES_GUARD /* Ensure other headers cannot be included directly. */
#include "nvtxDetail/nvtxExtTypes.h"
#undef NVTX_EXT_TYPES_GUARD

#ifndef NVTX_NO_IMPL
#define NVTX_EXT_IMPL_COUNTERS_GUARD /* Ensure other headers cannot be included directly. */
#include "nvtxDetail/nvtxExtImplCounters_v1.h"
#undef NVTX_EXT_IMPL_COUNTERS_GUARD
#endif /*NVTX_NO_IMPL*/

#ifdef __GNUC__
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NVTOOLSEXT_COUNTERS_H */