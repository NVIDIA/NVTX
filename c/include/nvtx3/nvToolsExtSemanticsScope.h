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

/**
 * NVTX semantic headers require nvToolsExtPayload.h to be included beforehand.
 */

#ifndef NVTX_SEMANTIC_ID_SCOPE_V1
#define NVTX_SEMANTIC_ID_SCOPE_V1 1

/**
 * \brief Specify the NVTX scope for a payload entry.
 *
 * This allows the scope to be set for a specific value or counter in a payload.
 * The scope must be known at schema registration time.
 */
typedef struct nvtxSemanticsScope_v1
{
    struct nvtxSemanticsHeader_v1 header;

    /** Specifies the scope of a payload entry, e.g. a counter or timestamp. */
    uint64_t scopeId;
} nvtxSemanticsScope_t;

#endif /* NVTX_SEMANTIC_ID_SCOPE_V1 */