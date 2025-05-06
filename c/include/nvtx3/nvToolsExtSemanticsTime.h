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

/** Identifier of the semantic extension for timestamps. */
#ifndef NVTX_SEMANTIC_ID_TIME_V1
#define NVTX_SEMANTIC_ID_TIME_V1 2

/* Use with the version field of `nvtxSemanticsHeader_t`. */
#define NVTX_TIME_SEMANTIC_VERSION 1

/** Semantic extension specifying timestamp properties. */
typedef struct nvtxSemanticsTime_v1
{
    struct nvtxSemanticsHeader_v1 header;

    /** Time domain ID or predefined `NVTX_TIMESTAMP_TYPE_*`. */
    uint64_t timeDomainId;
} nvtxSemanticsTime_t;

#endif /* NVTX_SEMANTIC_ID_TIME_V1 */
