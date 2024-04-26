/*
* Copyright 2023  NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License v2.0 with LLVM Exceptions.
* See https://llvm.org/LICENSE.txt for license information.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#ifndef NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_EVENT_SCOPE_V1
#define NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_EVENT_SCOPE_V1 1

/**
 * \brief Add registered event scope to payload entry.
 *
 * This allows the event scope to be set for a specific value or counter in a
 * payload. The event scope must be known at schema registration time.
 */
typedef struct nvtxPayloadEntryEventScope_v1
{
    struct nvtxSemanticsHeader_v1 header;

    /* Event scope of a timestamp or counter. */
    uint64_t eventScope;
} nvtxPayloadEntryEventScope_t;

/**
 * \brief The entry defines an event scope that is not registered.
 *
 * For example, CUDA context and CUDA stream define an event scope, which might
 * be interpreted by an NVTX handler.
 */
typedef struct nvtxPayloadEntryEventScopeUser_v1
{
    struct nvtxSemanticsHeader_v1 header;

    /* If multiple values define an event scope, different hierarchy levels can
       be set. */
    int32_t hierarchyLevel;
} nvtxPayloadEntryEventScopeUser_t;

#endif /* NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_EVENT_SCOPE_V1 */