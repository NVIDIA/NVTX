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

#include "NvtxPayloadInjectionAdapter.h"

#include <atomic>
#include <cinttypes>
#include <cstdio>

#include "NvtxPayloadJsonFormatter.h"
#include "NvtxPayloadTextFormatter.h"

namespace {

std::atomic<uint64_t> g_nextDynamicSchemaId{NVTX_PAYLOAD_SCHEMA_ID_DYNAMIC_START};

uint64_t AllocateDynamicId()
{
    return g_nextDynamicSchemaId++;
}

bool IsValidStaticSchemaId(uint64_t schemaId)
{
    return (schemaId >= NVTX_PAYLOAD_SCHEMA_ID_STATIC_START) &&
           (schemaId < NVTX_PAYLOAD_SCHEMA_ID_DYNAMIC_START);
}

} // namespace

uint64_t NvtxPayloadInjection::RegisterSchema(
    NvtxPayloadRegistry& registry, const nvtxPayloadSchemaAttr_t* attr)
{
    if (!attr)
    {
        std::fprintf(stderr, "[NVTX] RegisterSchema failed: attr is null.\n");
        return 0;
    }

    const uint64_t fieldMask = attr->fieldMask;
    const uint64_t requiredMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
                                  NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
                                  NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES;
    if ((fieldMask & requiredMask) != requiredMask)
    {
        std::fprintf(
            stderr,
            "[NVTX] RegisterSchema failed: required fieldMask bits missing "
            "(required=0x%" PRIx64 " got=0x%" PRIx64 ").\n",
            requiredMask,
            fieldMask);
        return 0;
    }

    if (!attr->entries || attr->numEntries == 0)
    {
        std::fprintf(
            stderr, "[NVTX] RegisterSchema failed: entries is null or numEntries is zero.\n");
        return 0;
    }

    PayloadSchema schema{};
    schema.type = attr->type;

    if ((fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME) != 0)
    {
        if (attr->name == nullptr)
        {
            std::fprintf(
                stderr,
                "[NVTX] RegisterSchema failed: name is null while "
                "NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME is set.\n");
            return 0;
        }
        schema.name = attr->name;
    }

    if ((fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE) != 0)
    {
        if (attr->payloadStaticSize == 0)
        {
            std::fprintf(
                stderr,
                "[NVTX] RegisterSchema failed: static size 0 is not allowed when "
                "NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE is set.\n");
            return 0;
        }
        schema.staticPayloadSize = attr->payloadStaticSize;
    }

    if ((fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ALIGNMENT) != 0)
    {
        if (attr->packAlign == 0)
        {
            std::fprintf(
                stderr,
                "[NVTX] RegisterSchema failed: alignment 0 is not allowed when "
                "NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ALIGNMENT is set.\n");
            return 0;
        }
        schema.packAlign = attr->packAlign;
    }

    uint64_t schemaId = 0;
    if ((fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_SCHEMA_ID) != 0)
    {
        if (!IsValidStaticSchemaId(attr->schemaId))
        {
            std::fprintf(
                stderr,
                "[NVTX] RegisterSchema failed: static schema ID %" PRIu64 " is out of range "
                "[%" PRIu64 ", %" PRIu64 ").\n",
                attr->schemaId,
                static_cast<uint64_t>(NVTX_PAYLOAD_SCHEMA_ID_STATIC_START),
                static_cast<uint64_t>(NVTX_PAYLOAD_SCHEMA_ID_DYNAMIC_START));
            return 0;
        }
        schemaId = attr->schemaId;
    }
    else
    {
        schemaId = AllocateDynamicId();
    }

    schema.entries.reserve(attr->numEntries);
    for (size_t i = 0; i < attr->numEntries; ++i)
    {
        const nvtxPayloadSchemaEntry_t& source = attr->entries[i];
        PayloadSchemaEntry entry{};
        entry.flags = source.flags;
        entry.type = source.type;
        entry.arrayOrUnionDetail = source.arrayOrUnionDetail;
        entry.offset = source.offset;
        entry.name = source.name ? source.name : std::string{};
        entry.description = source.description ? source.description : std::string{};
        schema.entries.push_back(entry);
    }

    if (!registry.AddSchema(schemaId, std::move(schema)))
    {
        return 0;
    }

    return schemaId;
}

uint64_t
NvtxPayloadInjection::RegisterEnum(NvtxPayloadRegistry& registry, const nvtxPayloadEnumAttr_t* attr)
{
    if (!attr)
    {
        std::fprintf(stderr, "[NVTX] RegisterEnum failed: attr is null.\n");
        return 0;
    }

    const uint64_t fieldMask = attr->fieldMask;
    const uint64_t requiredMask = NVTX_PAYLOAD_ENUM_ATTR_FIELD_ENTRIES |
                                  NVTX_PAYLOAD_ENUM_ATTR_FIELD_NUM_ENTRIES |
                                  NVTX_PAYLOAD_ENUM_ATTR_FIELD_SIZE;
    if ((fieldMask & requiredMask) != requiredMask)
    {
        std::fprintf(
            stderr,
            "[NVTX] RegisterEnum failed: required fieldMask bits missing "
            "(required=0x%" PRIx64 " got=0x%" PRIx64 ").\n",
            requiredMask,
            fieldMask);
        return 0;
    }
    if (!attr->entries || attr->numEntries == 0)
    {
        std::fprintf(
            stderr, "[NVTX] RegisterEnum failed: entries is null or numEntries is zero.\n");
        return 0;
    }
    if (attr->sizeOfEnum == 0 || attr->sizeOfEnum > sizeof(uint64_t))
    {
        std::fprintf(
            stderr,
            "[NVTX] RegisterEnum failed: sizeOfEnum=%zu is unsupported.\n",
            attr->sizeOfEnum);
        return 0;
    }

    uint64_t enumId = 0;
    if ((fieldMask & NVTX_PAYLOAD_ENUM_ATTR_FIELD_SCHEMA_ID) != 0)
    {
        if (!IsValidStaticSchemaId(attr->schemaId))
        {
            std::fprintf(
                stderr,
                "[NVTX] RegisterEnum failed: static schema ID %" PRIu64 " is out of range "
                "[%" PRIu64 ", %" PRIu64 ").\n",
                attr->schemaId,
                static_cast<uint64_t>(NVTX_PAYLOAD_SCHEMA_ID_STATIC_START),
                static_cast<uint64_t>(NVTX_PAYLOAD_SCHEMA_ID_DYNAMIC_START));
            return 0;
        }
        enumId = attr->schemaId;
    }
    else
    {
        enumId = AllocateDynamicId();
    }

    PayloadEnum payloadEnum{};
    if ((fieldMask & NVTX_PAYLOAD_ENUM_ATTR_FIELD_NAME) != 0)
    {
        if (attr->name == nullptr)
        {
            std::fprintf(
                stderr,
                "[NVTX] RegisterEnum failed: name is null while "
                "NVTX_PAYLOAD_ENUM_ATTR_FIELD_NAME is set.\n");
            return 0;
        }
        payloadEnum.name = attr->name;
    }
    payloadEnum.sizeOfEnum = attr->sizeOfEnum;
    payloadEnum.flagEntries.reserve(attr->numEntries);

    for (size_t i = 0; i < attr->numEntries; ++i)
    {
        const nvtxPayloadEnum_t& source = attr->entries[i];
        PayloadEnumEntry entry{};
        entry.name = source.name ? source.name : std::string{};
        entry.value = source.value;
        entry.isFlag = (source.isFlag != 0);
        payloadEnum.valueNames[entry.value] = entry.name;
        if (entry.isFlag)
        {
            payloadEnum.flagEntries.push_back(entry);
        }
    }

    if (!registry.AddEnum(enumId, std::move(payloadEnum)))
    {
        return 0;
    }

    return enumId;
}

std::string NvtxPayloadInjection::DescribePayloads(
    const NvtxPayloadRegistry& registry,
    const nvtxPayloadData_t* payloadData,
    size_t count,
    PayloadFormat format)
{
    switch (format)
    {
    case PayloadFormat::Json:
    {
        NvtxPayloadJsonVisitor visitor;
        registry.VisitPayloads(payloadData, count, visitor);
        return visitor.Finish();
    }
    case PayloadFormat::Text:
    default:
    {
        NvtxPayloadTextVisitor visitor;
        registry.VisitPayloads(payloadData, count, visitor);
        return visitor.Finish();
    }
    }
}

std::string NvtxPayloadInjection::DescribeEmbeddedPayload(
    const NvtxPayloadRegistry& registry,
    const nvtxEventAttributes_t* eventAttrib,
    PayloadFormat format)
{
    if (!eventAttrib || eventAttrib->payloadType != NVTX_PAYLOAD_TYPE_EXT)
    {
        return {};
    }

    auto count = static_cast<size_t>(eventAttrib->reserved0);
    if (count == 0)
    {
        return {};
    }

    auto* payloadData = reinterpret_cast<const nvtxPayloadData_t*>(
        static_cast<uintptr_t>(eventAttrib->payload.ullValue));
    if (!payloadData)
    {
        return {};
    }

    return DescribePayloads(registry, payloadData, count, format);
}
