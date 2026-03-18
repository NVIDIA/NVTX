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

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <unordered_map>

#include <nvtx3/nvToolsExtPayload.h>

#include "NvtxPayloadParser.h"
#include "NvtxPayloadSchema.h"

/**
 * Owns registered payload schemas and enums, and provides decode operations
 * on payload data.
 *
 * Registered NVTX string handles are referenced (not owned) via a const
 * reference supplied at construction time, so the caller retains ownership
 * and can use the same map for non-payload purposes.
 *
 * This class is the primary entry point for consuming NVTX payloads as a
 * standalone module. Callers populate the registry via AddSchema/AddEnum
 * and then use FormatPayloads to decode binary payload blobs into a
 * caller-supplied PayloadStreamVisitor.
 */
class NvtxPayloadRegistry
{
  public:
    explicit NvtxPayloadRegistry(
        const std::unordered_map<nvtxStringHandle_t, std::string>& registeredStrings,
        const nvtxPayloadEntryTypeInfo_t* typeInfo = nullptr)
        : schemaProcessor_(typeInfo)
        , registeredStrings_(registeredStrings)
    {}

    /**
     * Stores and finalizes a schema under the given ID.
     *
     * All nested schema dependencies must already be registered and finalized.
     * Returns false if finalization fails or the ID is already in use.
     */
    bool AddSchema(uint64_t schemaId, PayloadSchema schema);

    /** Stores an enum definition under the given ID. Returns false if the ID is already in use. */
    bool AddEnum(uint64_t enumId, PayloadEnum payloadEnum);

    /** Decodes payloads and streams typed events into the caller-supplied visitor. */
    void VisitPayloads(
        const nvtxPayloadData_t* payloadData, size_t count, PayloadStreamVisitor& visitor) const;

  private:
    bool HasId(uint64_t id) const;

    NvtxPayloadSchemaProcessor schemaProcessor_;
    NvtxPayloadParser parser_{schemaProcessor_};

    std::unordered_map<uint64_t, PayloadSchema> schemas_;
    std::unordered_map<uint64_t, PayloadEnum> enums_;
    const std::unordered_map<nvtxStringHandle_t, std::string>& registeredStrings_;
};
