/*
* Copyright 2023  NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License v2.0 with LLVM Exceptions.
* See https://llvm.org/LICENSE.txt for license information.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#ifndef NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_COUNTERS_V1
#define NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_COUNTERS_V1 4

#include <stdint.h>

/**
 * \brief Flags to augment the counter value.
 */
#define NVTX_COUNTER_FLAG_NONE  0

/* Convert the fixed point value to a normalized floating point.
Unsigned [0f : 1f] or signed [-1f : 1f] */
#define NVTX_COUNTER_FLAG_NORM         (1 << 1)

/* Tools should apply scale and limits when graphing, ideally in a "soft" way
to see when limits are exceeded. */
#define NVTX_COUNTER_FLAG_LIMIT_MIN    (1 << 2)
#define NVTX_COUNTER_FLAG_LIMIT_MAX    (1 << 3)
#define NVTX_COUNTER_FLAG_LIMITS \
    (NVTX_COUNTER_FLAG_LIMIT_MIN | NVTX_COUNTER_FLAG_LIMIT_MAX)

/* A stepwise graph is expected, if not set. A counter value represents the
value after the sample point. */
#define NVTX_COUNTER_INFO_FLAG_GRAPH_LINEAR (1 << 4)

/* Datatype for limits union. */
#define NVTX_COUNTER_LIMIT_I64 0
#define NVTX_COUNTER_LIMIT_U64 1
#define NVTX_COUNTER_LIMIT_F64 2

/**
 *\brief Specify additional properties of a counter entry.
 */
typedef struct nvtxPayloadEntryCounter_v1 {
    struct nvtxSemanticsHeader_v1 header;

    /* Apply normalization, scale limits, etc. (see `NVTX_COUNTER_FLAG_*`) */
    uint64_t flags;

    /* Unit of the counter value. */
    const char* unit;

    /* Soft graph limit. */
    union limits_t {
        int64_t 	i64[2];
        uint64_t	u64[2];
        double  	f64[2];
    } limits;

    /*
     * Valid values are NVTX_PAYLOAD_UNKNOWN, NVTX_PAYLOAD_TYPE_INT64,
     * NVTX_PAYLOAD_TYPE_UNSIGNED_INT64 and NVTX_PAYLOAD_TYPE_DOUBLE.
     */
    int32_t limitsDataType;
} nvtxPayloadEntryCounter_t;

#endif /* NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_COUNTERS_V1 */