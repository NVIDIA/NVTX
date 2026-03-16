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

#include "NvtxPayloadRegistry.h"

#include <cinttypes>
#include <cstdio>

bool NvtxPayloadRegistry::AddSchema(uint64_t schemaId, PayloadSchema schema)
{
    if (HasId(schemaId))
    {
        std::fprintf(
            stderr, "[NVTX] AddSchema failed: ID %" PRIu64 " is already in use.\n", schemaId);
        return false;
    }

    schemas_[schemaId] = std::move(schema);

    if (!schemaProcessor_.FinalizeSchema(schemaId, schemas_, enums_))
    {
        std::fprintf(
            stderr,
            "[NVTX] AddSchema failed: finalization of '%s' "
            "(id=%" PRIu64 ") failed.\n",
            schemas_[schemaId].name.c_str(),
            schemaId);
        schemas_.erase(schemaId);
        return false;
    }

    return true;
}

bool NvtxPayloadRegistry::AddEnum(uint64_t enumId, PayloadEnum payloadEnum)
{
    if (HasId(enumId))
    {
        std::fprintf(stderr, "[NVTX] AddEnum failed: ID %" PRIu64 " is already in use.\n", enumId);
        return false;
    }

    enums_[enumId] = std::move(payloadEnum);
    return true;
}

bool NvtxPayloadRegistry::HasId(uint64_t id) const
{
    return schemas_.find(id) != schemas_.end() || enums_.find(id) != enums_.end();
}

void NvtxPayloadRegistry::VisitPayloads(
    const nvtxPayloadData_t* payloadData, size_t count, PayloadStreamVisitor& visitor) const
{
    parser_.ProcessPayloads(payloadData, count, schemas_, enums_, registeredStrings_, visitor);
}
