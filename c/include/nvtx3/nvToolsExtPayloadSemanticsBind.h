/*
* Copyright 2023  NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License v2.0 with LLVM Exceptions.
* See https://llvm.org/LICENSE.txt for license information.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#ifndef NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_BIND_V1
#define NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_BIND_V1 5

#include <stdint.h>

#define NVTX_PAYLOAD_SEMANTICS_INDEX_INVALID SIZE_MAX

/**
 *\brief Specify a binding for this payload schema entry.
 */
typedef struct nvtxPayloadEntryBind_v1 {
    struct nvtxPayloadEntrySemantic_v1 header;

    /* Entry binds to the provided schema entry (index). */
    size_t schemaEntryIdx;

    /* Entry binds to the provided payload entry (index). */
    size_t payloadDataIdx;
} nvtxPayloadEntryBind_t;

#endif /* NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_BIND_V1 */