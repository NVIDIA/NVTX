/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/** Identifier of the semantic extension for correlations. */
#ifndef NVTX_SEMANTIC_ID_CORRELATION_V1
#define NVTX_SEMANTIC_ID_CORRELATION_V1 3

/* Use with the version field of `nvtxSemanticsHeader_t`. */
#define NVTX_CORRELATION_SEMANTIC_VERSION 1

/** Type of relationship between events. */
#define NVTX_CORRELATION_ROLE_NONE 0
/* Roles will be added as required. */

/** Correlation value indicating that an event should not correlate with other events. */
#define NVTX_CORRELATION_VALUE_NONE 0

/**
 * Mark this entry as correlation identifier and specify how this event
 * relates to one or more events in the same correlation domain.
 */
typedef struct nvtxSemanticsCorrelation_v1
{
    struct nvtxSemanticsHeader_v1 header;

    /** Globally unique ID (across NVTX domains). */
    unsigned char correlationDomainUuid[16];

    /**
     * Optional descriptive name of the correlation domain. If provided, a tool
     * will copy the string during schema registration.
     */
    const char* displayName;

    /** Specifies the role in a correlation with another event. */
    uint64_t role;
} nvtxSemanticsCorrelation_t;

#endif /* NVTX_SEMANTIC_ID_CORRELATION_V1 */
