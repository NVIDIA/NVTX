/*
* Copyright 2023  NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License v2.0 with LLVM Exceptions.
* See https://llvm.org/LICENSE.txt for license information.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#ifndef NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_CORRELATION_V1
#define NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_CORRELATION_V1 4

#include <stdint.h>

/**
 * \brief Type of relationship between events.
 */
#define NVTX_EVENT_RELATION_ANY            0
#define NVTX_EVENT_RELATION_HAPPENS_BEFORE 1
#define NVTX_EVENT_RELATION_HAPPENS_AFTER  2

/**
 *\brief Mark this entry as correlation identifier and define how this event
 * relates to one or more events in the same scope.
 */
typedef struct nvtxPayloadEntryCorrelation_v1 {
    struct nvtxPayloadEntrySemantic_v1 header;

    /* The event's execution scope. */
    uint64_t eventScopeId;

    /* The relation between this event and the referenced one. */
    uint64_t relation;
} nvtxPayloadEntryCorrelation_t;

/**
 *\brief Mark this entry as correlation identifier so that another event can
 * correlate to this event.
 */
typedef struct nvtxPayloadEntryCorrelationSelf_v1 {
    struct nvtxPayloadEntrySemantic_v1 header;

    /* The event's execution scope. */
    uint64_t eventScopeId;
} nvtxPayloadEntryCorrelationSelf_t;

#endif /* NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_CORRELATION_V1 */