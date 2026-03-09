/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#pragma once

#include <stdint.h>
#include <string>

#include <nvtx3/nvToolsExtPayload.h>

#include "NvtxPayloadRegistry.h"

enum class PayloadFormat
{
    Text,
    Json
};

/**
 * NVTX-API-facing helpers that validate incoming schema/enum registration
 * attributes, manage dynamic ID generation, and delegate storage to a
 * caller-supplied NvtxPayloadRegistry.
 *
 * Injection tools call these functions to process NVTX callback data. Code
 * that already has pre-assigned schema IDs (e.g. trace-file readers) can
 * use NvtxPayloadRegistry directly.
 */
namespace NvtxPayloadInjection {

/**
 * Validates an NVTX schema registration request, assigns an ID (static or
 * dynamic), converts the API struct to a PayloadSchema, and stores it in
 * the registry. Returns the assigned schema ID, or 0 on failure.
 */
uint64_t RegisterSchema(NvtxPayloadRegistry& registry, const nvtxPayloadSchemaAttr_t* attr);

/**
 * Validates an NVTX enum registration request, assigns an ID, converts
 * the API struct to a PayloadEnum, and stores it in the registry.
 * Returns the assigned enum ID, or 0 on failure.
 */
uint64_t RegisterEnum(NvtxPayloadRegistry& registry, const nvtxPayloadEnumAttr_t* attr);

/**
 * Decodes payloads and returns the result as a formatted string.
 * Convenience wrapper that creates the appropriate visitor for the given format.
 */
std::string DescribePayloads(
    const NvtxPayloadRegistry& registry,
    const nvtxPayloadData_t* payloadData,
    size_t count,
    PayloadFormat format = PayloadFormat::Text);

/**
 * Extracts payload data embedded in event attributes (via NVTX_PAYLOAD_TYPE_EXT)
 * and returns it formatted as a string. Returns empty string if no payload is present.
 */
std::string DescribeEmbeddedPayload(
    const NvtxPayloadRegistry& registry,
    const nvtxEventAttributes_t* eventAttrib,
    PayloadFormat format = PayloadFormat::Text);

} // namespace NvtxPayloadInjection
