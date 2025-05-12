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

/** Identifier of the semantic extension for counters. */
#ifndef NVTX_SEMANTIC_ID_COUNTERS_V1
#define NVTX_SEMANTIC_ID_COUNTERS_V1 5

/* Use with the version field of `nvtxSemanticsHeader_t`. */
#define NVTX_COUNTER_SEMANTIC_VERSION 2

/***  Flags to augment the counter value. ***/
#define NVTX_COUNTER_FLAGS_NONE  0

/**
 * Convert the fixed point value to a normalized floating point.
 * Use the sign/unsign from the underlying type this flag is applied to.
 * Unsigned [0f : 1f] or signed [-1f : 1f]
 */
#define NVTX_COUNTER_FLAG_NORMALIZE  (1 << 1)

/**
 * Tools should apply scale and limits when graphing, ideally in a "soft" way to
 * to see when limits are exceeded.
 */
#define NVTX_COUNTER_FLAG_LIMIT_MIN  (1 << 2)
#define NVTX_COUNTER_FLAG_LIMIT_MAX  (1 << 3)
#define NVTX_COUNTER_FLAG_LIMITS \
    (NVTX_COUNTER_FLAG_LIMIT_MIN | NVTX_COUNTER_FLAG_LIMIT_MAX)

/**
 * Counter value types
 */
#define NVTX_COUNTER_FLAG_VALUETYPE_ABSOLUTE          (1 << 4)
/* Delta to previous sample, tool-defined if no previous sample is available. */
#define NVTX_COUNTER_FLAG_VALUETYPE_DELTA             (2 << 4)
#define NVTX_COUNTER_FLAG_VALUETYPE_DELTA_SINCE_START (3 << 4)

/**
 * Counter interpolation / effective range of counters.
 */
/* No interpolation between samples. */
#define NVTX_COUNTER_FLAG_INTERPOLATION_POINT         (1 << 8)
/* Piecewise constant interpolation between the current and the last sample. */
#define NVTX_COUNTER_FLAG_INTERPOLATION_SINCE_LAST    (2 << 8)
/* Piecewise constant interpolation between the current and the next sample. */
#define NVTX_COUNTER_FLAG_INTERPOLATION_UNTIL_NEXT    (3 << 8)
/* Piecewise linear interpolation between samples. */
#define NVTX_COUNTER_FLAG_INTERPOLATION_LINEAR        (4 << 8)

/**
 * Datatype for limits union (value of `limitType`).
 */
#define NVTX_COUNTER_LIMIT_UNDEFINED 0
#define NVTX_COUNTER_LIMIT_I64       1
#define NVTX_COUNTER_LIMIT_U64       2
#define NVTX_COUNTER_LIMIT_F64       3


/**
 * Union of datatypes that can be used as counter value limits.
 */
typedef union
{
    int64_t i64;
    uint64_t u64;
    double f64;
} nvtxCounterLimit_t;

/**
 * \brief Specify additional properties of a counter or counter group.
 */
typedef struct nvtxSemanticsCounter_v1
{
    /** Header of the semantic extension (with identifier, version, etc.). */
    struct nvtxSemanticsHeader_v1 header;

    /**
     * Flag if normalization, scale limits, etc. should be applied to counter
     * values.
     */
    uint64_t flags;

    /** Unit of the counter value (case insensitive) */
    const char* unit;

    /** Should be 1 if not used. */
    uint64_t unitScaleNumerator;

    /** Should be 1 if not used. */
    uint64_t unitScaleDenominator;

    /**
     * Specifies the used union member for `min` and `max`.
     * Use the defines `NVTX_COUNTER_LIMIT_*`.
     */
    int64_t limitType;

    /** Value limits. */
    nvtxCounterLimit_t min;
    nvtxCounterLimit_t max;
} nvtxSemanticsCounter_t;

#endif /* NVTX_SEMANTIC_ID_COUNTERS_V1 */
