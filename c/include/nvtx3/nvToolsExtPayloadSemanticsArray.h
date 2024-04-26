/*
* Copyright 2023  NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License v2.0 with LLVM Exceptions.
* See https://llvm.org/LICENSE.txt for license information.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#ifndef NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_ARRAY_V1
#define NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_ARRAY_V1 3

/**
 * \brief Provide details on the layout of the array described by this
 *  entry.
 */
typedef struct nvtxPayloadEntryArrayLayout_v1
{
    struct nvtxPayloadEntrySemantic_v1 header;

    /* Index of the entry that specifies the array stride. */
    size_t strideEntryIdx;

    /* The payload index or `-1` for *this* payload. */
    int32_t stridePayloadIdx;

    /* row-major == 0 (default), column-major == 1 */
    uint8_t storageLayout;
} nvtxPayloadEntryArrayLayout_t;

/**
 * \brief Provide details on the location of the array length.
 *
 * This enables the length of an array to specified in another payload and
 * payload entry of the same event.
 */
typedef struct nvtxPayloadEntryArrayLength_v1
{
    struct nvtxPayloadEntrySemantic_v1 header;

    /* Index of the payload in the array of payload data (`nvtxPayloadData_t*`).
       `-1` for *this* payload. */
    int32_t payloadIdx;

    /* Index of the entry in a payload. */
    int32_t entryIdx;
} nvtxPayloadEntryArrayLength_t;

/**
 * \brief Specify the ordering of the array defined by this entry.
 * \todo
 */
typedef struct nvtxPayloadEntryArrayOrdering_v1
{
    struct nvtxPayloadEntrySemantic_v1 header;

    int16_t orderingType;
    int16_t orderInterleaving;
    int32_t orderingSkid;
    int64_t orderingSkidAmount;
} nvtxPayloadEntryArrayOrdering_t;

/**
 * \brief The entry value specifies the ordering.
 * \todo
 */
typedef struct nvtxPayloadEntryIsOrdering_v1
{
    struct nvtxPayloadEntrySemantic_v1 header;

    /* array of batch indices + count */
} nvtxPayloadEntryIsOrdering_t;

#endif /* NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_ARRAY_V1 */