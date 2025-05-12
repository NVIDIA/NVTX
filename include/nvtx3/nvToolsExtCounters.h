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

#if defined(NVTX_AS_SYSTEM_HEADER)
#if defined(__clang__)
#pragma clang system_header
#elif defined(__GNUC__) || defined(__NVCOMPILER)
#pragma GCC system_header
#elif defined(_MSC_VER)
#pragma system_header
#endif
#endif

#include "nvToolsExtPayload.h"

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

#ifndef NVTX_COUNTER_IDS_V1
#define NVTX_COUNTER_IDS_V1

/** The counter ID is not specified. */
#define NVTX_COUNTER_ID_NONE          0

/** Static (user-provided, feed-forward) counter (group) IDs. */
#define NVTX_COUNTER_ID_STATIC_START  (1 << 24)

/** Dynamically (tool) generated counter (group) IDs */
#define NVTX_COUNTER_ID_DYNAMIC_START (NVTX_STATIC_CAST(uint64_t, 1) << 32)

#endif /* NVTX_COUNTER_IDS_V1 */

/** Reasons for the missing sample value. */
#ifndef NVTX_COUNTER_SAMPLES_V1
#define NVTX_COUNTER_SAMPLES_V1

#define NVTX_COUNTER_SAMPLE_ZERO        0
#define NVTX_COUNTER_SAMPLE_UNCHANGED   1
#define NVTX_COUNTER_SAMPLE_UNAVAILABLE 2 /* Failed to get a counter sample. */

#endif /* NVTX_COUNTER_SAMPLES_V1 */

/**
 * Counter batch timestamp array flags.
 * Values must not overlap with `NVTX_BATCH_FLAG_*`.
 * By default, one timestamp per sample is assumed.
 */
#ifndef NVTX_COUNTER_BATCH_FLAGS_V1
#define NVTX_COUNTER_BATCH_FLAGS_V1

#define NVTX_COUNTER_BATCH_FLAG_BEGINTIME_INTERVAL_PAIR (1 << 32)
#define NVTX_COUNTER_BATCH_FLAG_ENDTIME_INTERVAL_PAIR   (2 << 32)

#endif /* NVTX_COUNTER_BATCH_FLAGS_V1 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef NVTX_COUNTER_TYPEDEFS_V1
#define NVTX_COUNTER_TYPEDEFS_V1

/**
 * \brief Attributes of a counter or counter group.
 */
typedef struct nvtxCounterAttr_v1
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
     * the same NVTX domain as `nvtxCounterRegister`. An invalid scope ID will
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
     * A static counter ID must be unique within the domain,
     * >= NVTX_COUNTER_ID_STATIC_START, and < NVTX_COUNTER_ID_DYNAMIC_START.
     * Use NVTX_COUNTER_ID_NONE to let the tool create a (dynamic) counter ID.
     */
    uint64_t counterId;
} nvtxCounterAttr_t;

/**
 * \brief Helper struct to submit a batch of counters.
 *
 * The size of one sample is specified via the `payloadStaticSize` field of the
 * counter's data layout schema or the size of the predefined payload entry type
 * and must include padding. There should be no remainder when dividing
 * `countersSize` by `nvtxPayloadSchemaAttr_t::payloadStaticSize`.
 */
typedef struct nvtxCounterBatch_v1
{
    /**
     * Identifier of a counter group (data layout, scope, etc.). All counter
     * samples in the batch have the same layout and size.
     */
    uint64_t counterId;

    /** Batch of counter (group) samples. */
    const void* counters;

    /** Size of the counter batch (in bytes). */
    size_t countersSize;

    /**
     * Timestamp ordering, timestamp style, etc.
     * See `NVTX_BATCH_FLAG_*` and `NVTX_COUNTER_BATCH_FLAG_*`.
     */
    uint64_t flags;

    /**
     * Array of timestamps or a timestamp/interval pair. This field can be
     * `NULL`, if timestamps are included in the counter samples as part of the
     * counter group layout. By default, one timestamp per sample is assumed.
     * The timestamp source is specified via time semantics passed during the
     * counter group registration.
     * This overrides the timestamps embedded in counter samples.
     */
    const int64_t* timestamps;

    /** Size of the timestamps array or timestamp/interval pair (in bytes). */
    size_t timestampsSize;
} nvtxCounterBatch_t;

#endif /* NVTX_COUNTER_TYPEDEFS_V1 */

#ifndef NVTX_COUNTER_API_FUNCTIONS_V1
#define NVTX_COUNTER_API_FUNCTIONS_V1

/**
 * \brief Register a counter (group).
 *
 * @param hDomain NVTX domain handle.
 * @param attr Pointer to the attributes of the counter (group).
 *
 * @return Identifier of a counter (group). The counter ID is unique within
 *         the NVTX domain.
 */
NVTX_DECLSPEC uint64_t NVTX_API nvtxCounterRegister(
    nvtxDomainHandle_t hDomain,
    const nvtxCounterAttr_t* attr);

/**
 * Sample one integer counter by value immediately
 * (the NVTX tool determines the timestamp).
 *
 * @param hDomain handle of the NVTX domain.
 * @param counterId identifier of the NVTX counter (group).
 * @param value 64-bit integer counter value.
 */
NVTX_DECLSPEC void NVTX_API nvtxCounterSampleInt64(
    nvtxDomainHandle_t hDomain,
    uint64_t counterId,
    int64_t value);

/**
 * Sample one floating point counter by value immediately
 * (the NVTX tool determines the timestamp).
 *
 * @param hDomain handle of the NVTX domain.
 * @param counterId identifier of the NVTX counter (group).
 * @param value 64-bit floating-point counter value.
 */
NVTX_DECLSPEC void NVTX_API nvtxCounterSampleFloat64(
    nvtxDomainHandle_t hDomain,
    uint64_t counterId,
    double value);

/**
 * Sample a counter (group) by reference immediately
 * (the NVTX tool determines the timestamp).
 *
 * @param hDomain handle of the NVTX domain.
 * @param counterId identifier of the NVTX counter (group).
 * @param value pointer to one or more counter values.
 * @param size size of the counter value(s) in bytes.
 */
NVTX_DECLSPEC void NVTX_API nvtxCounterSample(
    nvtxDomainHandle_t hDomain,
    uint64_t counterId,
    const void* value,
    size_t size);

/**
 * \brief Sample without value.
 *
 * @param hDomain handle of the NVTX domain.
 * @param counterId identifier of the NVTX counter (group).
 * @param reason reason for the missing sample value.
 */
NVTX_DECLSPEC void NVTX_API nvtxCounterSampleNoValue(
    nvtxDomainHandle_t hDomain,
    uint64_t counterId,
    uint8_t reason);

/**
 * \brief Submit a batch of counters in the given domain.
 *
 * The size of a data sampling point is defined by the `payloadStaticSize` field
 * of the payload schema. An NVTX tool can assume that the counter samples are
 * stored as an array with each entry being `payloadStaticSize` bytes.
 *
 * @param hDomain handle of the NVTX domain
 * @param counterData Pointer to the counter data to be submitted.
 */
NVTX_DECLSPEC void NVTX_API nvtxCounterBatchSubmit(
    nvtxDomainHandle_t hDomain,
    const nvtxCounterBatch_t* counterData);

#endif /* NVTX_COUNTER_API_FUNCTIONS_V1 */

#ifndef NVTX_COUNTER_CALLBACK_ID_V1
#define NVTX_COUNTER_CALLBACK_ID_V1

#define NVTX3EXT_CBID_nvtxCounterRegister           0
#define NVTX3EXT_CBID_nvtxCounterSampleInt64        1
#define NVTX3EXT_CBID_nvtxCounterSampleFloat64      2
#define NVTX3EXT_CBID_nvtxCounterSample             3
#define NVTX3EXT_CBID_nvtxCounterSampleNoValue      4
#define NVTX3EXT_CBID_nvtxCounterBatchSubmit        5

#endif /* NVTX_COUNTER_CALLBACK_ID_V1 */

/* Macros to create versioned symbols. */
#ifndef NVTX_EXT_COUNTERS_VERSIONED_IDENTIFIERS_V1
#define NVTX_EXT_COUNTERS_VERSIONED_IDENTIFIERS_V1
#define NVTX_EXT_COUNTERS_VERSIONED_IDENTIFIER_L3(NAME, VERSION, COMPATID) \
    NAME##_v##VERSION##_cnt##COMPATID
#define NVTX_EXT_COUNTERS_VERSIONED_IDENTIFIER_L2(NAME, VERSION, COMPATID) \
    NVTX_EXT_COUNTERS_VERSIONED_IDENTIFIER_L3(NAME, VERSION, COMPATID)
#define NVTX_EXT_COUNTERS_VERSIONED_ID(NAME) \
    NVTX_EXT_COUNTERS_VERSIONED_IDENTIFIER_L2(NAME, NVTX_VERSION, NVTX_EXT_COUNTERS_COMPATID)
#endif /* NVTX_EXT_COUNTERS_VERSIONED_IDENTIFIERS_V1 */

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
