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

#include "NvtxPayloadSchema.h"
#include "NvtxPayloadUtils.h"

#include <cassert>
#include <cinttypes>

namespace {

/**
 * Maps NVTX array entry flags to `PayloadArrayLayout`.
 *
 * @note `NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_PAYLOAD_INDEX` (array length
 *       stored in a separate payload of the same event) is not handled and
 *       falls through to `None`.
 */
PayloadArrayLayout GetArrayLayout(uint64_t flags)
{
    const uint64_t arrayType = NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_TYPE(flags);
    if (arrayType == NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE)
    {
        return PayloadArrayLayout::FixedSize;
    }
    if (arrayType == NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX)
    {
        return PayloadArrayLayout::LengthIndex;
    }
    if (arrayType == NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED)
    {
        return PayloadArrayLayout::ZeroTerminated;
    }
    return PayloadArrayLayout::None;
}

const char* PrintSchemaName(const PayloadSchema& schema)
{
    return schema.name.empty() ? "<unnamed>" : schema.name.c_str();
}

const char* PrintEntryName(const PayloadSchemaEntry& entry)
{
    return entry.name.empty() ? "<unnamed>" : entry.name.c_str();
}

} // namespace

size_t NvtxPayloadSchemaProcessor::GetTypeSize(uint64_t type) const
{
    if (typeInfo_ && type < NVTX_PAYLOAD_ENTRY_TYPE_INFO_ARRAY_SIZE)
    {
        const size_t size = typeInfo_[type].size;
        if (size > 0)
            return size;
    }

    switch (type)
    {
    // Fixed-width integer types.
    case NVTX_PAYLOAD_ENTRY_TYPE_INT8:
    case NVTX_PAYLOAD_ENTRY_TYPE_UINT8:
    case NVTX_PAYLOAD_ENTRY_TYPE_BYTE:
        return 1;
    case NVTX_PAYLOAD_ENTRY_TYPE_INT16:
    case NVTX_PAYLOAD_ENTRY_TYPE_UINT16:
        return 2;
    case NVTX_PAYLOAD_ENTRY_TYPE_INT32:
    case NVTX_PAYLOAD_ENTRY_TYPE_UINT32:
        return 4;
    case NVTX_PAYLOAD_ENTRY_TYPE_INT64:
    case NVTX_PAYLOAD_ENTRY_TYPE_UINT64:
        return 8;
    case NVTX_PAYLOAD_ENTRY_TYPE_INT128:
    case NVTX_PAYLOAD_ENTRY_TYPE_UINT128:
        return 16;

    // Fixed-width floating-point types.
    case NVTX_PAYLOAD_ENTRY_TYPE_FLOAT16:
    case NVTX_PAYLOAD_ENTRY_TYPE_BF16:
        return 2;
    case NVTX_PAYLOAD_ENTRY_TYPE_FLOAT32:
    case NVTX_PAYLOAD_ENTRY_TYPE_TF32:
        return 4;
    case NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64:
        return 8;
    case NVTX_PAYLOAD_ENTRY_TYPE_FLOAT128:
        return 16;

    // Fixed-width character types.
    case NVTX_PAYLOAD_ENTRY_TYPE_CHAR8:
        return 1;
    case NVTX_PAYLOAD_ENTRY_TYPE_CHAR16:
        return 2;
    case NVTX_PAYLOAD_ENTRY_TYPE_CHAR32:
        return 4;

    // Semantic types with sizes documented in the NVTX header.
    case NVTX_PAYLOAD_ENTRY_TYPE_COLOR_ARGB:
    case NVTX_PAYLOAD_ENTRY_TYPE_CATEGORY:
    case NVTX_PAYLOAD_ENTRY_TYPE_PID_UINT32:
    case NVTX_PAYLOAD_ENTRY_TYPE_TID_UINT32:
        return 4;
    case NVTX_PAYLOAD_ENTRY_TYPE_SCOPE_ID:
    case NVTX_PAYLOAD_ENTRY_TYPE_RANGE_ID:
    case NVTX_PAYLOAD_ENTRY_TYPE_PID_UINT64:
    case NVTX_PAYLOAD_ENTRY_TYPE_TID_UINT64:
        return 8;

    // String pointer and handle types. Embedded (non-pointer) strings use per-character
    // sizes instead; callers handle that distinction via entry flags before reaching here.
    case NVTX_PAYLOAD_ENTRY_TYPE_CSTRING:
    case NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF8:
    case NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF16:
    case NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32:
        return sizeof(const char*);
    case NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE:
        return sizeof(nvtxStringHandle_t);

    // Platform-dependent C types: use sizeof() for the tool's platform
    // as a best-effort fallback when the producer's typeInfo is unavailable.
    case NVTX_PAYLOAD_ENTRY_TYPE_CHAR:
        return sizeof(char);
    case NVTX_PAYLOAD_ENTRY_TYPE_UCHAR:
        return sizeof(unsigned char);
    case NVTX_PAYLOAD_ENTRY_TYPE_SHORT:
        return sizeof(short);
    case NVTX_PAYLOAD_ENTRY_TYPE_USHORT:
        return sizeof(unsigned short);
    case NVTX_PAYLOAD_ENTRY_TYPE_INT:
        return sizeof(int);
    case NVTX_PAYLOAD_ENTRY_TYPE_UINT:
        return sizeof(unsigned int);
    case NVTX_PAYLOAD_ENTRY_TYPE_LONG:
        return sizeof(long);
    case NVTX_PAYLOAD_ENTRY_TYPE_ULONG:
        return sizeof(unsigned long);
    case NVTX_PAYLOAD_ENTRY_TYPE_LONGLONG:
        return sizeof(long long);
    case NVTX_PAYLOAD_ENTRY_TYPE_ULONGLONG:
        return sizeof(unsigned long long);
    case NVTX_PAYLOAD_ENTRY_TYPE_FLOAT:
        return sizeof(float);
    case NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE:
        return sizeof(double);
    case NVTX_PAYLOAD_ENTRY_TYPE_LONGDOUBLE:
        return sizeof(long double);
    case NVTX_PAYLOAD_ENTRY_TYPE_SIZE:
        return sizeof(size_t);
    case NVTX_PAYLOAD_ENTRY_TYPE_ADDRESS:
        return sizeof(void*);
    case NVTX_PAYLOAD_ENTRY_TYPE_WCHAR:
        return sizeof(wchar_t);

    default:
        return 0;
    }
}

size_t NvtxPayloadSchemaProcessor::GetTypeAlign(uint64_t type) const
{
    if (typeInfo_ && type < NVTX_PAYLOAD_ENTRY_TYPE_INFO_ARRAY_SIZE)
    {
        const size_t align = typeInfo_[type].align;
        if (align > 0)
        {
            return align;
        }
    }

    const size_t size = GetTypeSize(type);
    if (size == 0)
    {
        return 1;
    }
    return size > 8 ? 8 : size;
}

size_t NvtxPayloadSchemaProcessor::GetEntryElementSize(
    uint64_t type,
    const std::unordered_map<uint64_t, PayloadSchema>& schemas,
    const std::unordered_map<uint64_t, PayloadEnum>& enums) const
{
    // Predefined types.
    if (type < NVTX_PAYLOAD_SCHEMA_ID_STATIC_START)
    {
        return GetTypeSize(type);
    }

    // Nested schema or enum.
    const auto schemaIt = schemas.find(type);
    if (schemaIt == schemas.end())
    {
        // Nested schema not found, try enum.
        const auto enumIt = enums.find(type);
        return (enumIt != enums.end()) ? enumIt->second.sizeOfEnum : 0;
    }

    return schemaIt->second.staticPayloadSize;
}

size_t NvtxPayloadSchemaProcessor::GetEntrySize(
    const PayloadSchemaEntry& entry,
    const std::unordered_map<uint64_t, PayloadSchema>& schemas,
    const std::unordered_map<uint64_t, PayloadEnum>& enums) const
{
    if (IsEmbeddedFixedStringEntry(entry))
    {
        const size_t charSize = GetStringCharacterSize(entry.type);
        return static_cast<size_t>(entry.arrayOrUnionDetail) * charSize;
    }

    const PayloadArrayLayout arrayLayout = GetArrayLayout(entry.flags);
    if (arrayLayout == PayloadArrayLayout::LengthIndex ||
        arrayLayout == PayloadArrayLayout::ZeroTerminated)
    {
        return 0;
    }

    const size_t elementSize = GetEntryElementSize(entry.type, schemas, enums);
    const size_t arrayLength = ::GetArrayLength(entry);
    if (elementSize == 0 || arrayLength == 0)
        return 0;
    return elementSize * arrayLength;
}

bool NvtxPayloadSchemaProcessor::FinalizeSchema(
    uint64_t schemaId,
    std::unordered_map<uint64_t, PayloadSchema>& schemas,
    const std::unordered_map<uint64_t, PayloadEnum>& enums) const
{
    const auto schemaIt = schemas.find(schemaId);
    if (schemaIt == schemas.end())
    {
        NVTX_PAYLOAD_LOG_ERROR("Unknown schema id=%" PRIu64 ".", schemaId);
        return false;
    }
    PayloadSchema& schema = schemaIt->second;

    if (schema.type == NVTX_PAYLOAD_SCHEMA_TYPE_UNION ||
        schema.type == NVTX_PAYLOAD_SCHEMA_TYPE_UNION_WITH_INTERNAL_SELECTOR)
    {
        NVTX_PAYLOAD_LOG_ERROR(
            "Schema '%s' (id=%" PRIu64 "): union schema types are not supported.",
            PrintSchemaName(schema),
            schemaId);
        return false;
    }

    if (!typeInfo_)
    {
        static bool warned = false;
        if (!warned)
        {
            warned = true;
            NVTX_PAYLOAD_LOG_ERROR(
                "nvtxPayloadEntryTypeInfo_t array not set. "
                "Using tool-platform type sizes as fallback; "
                "sizes may be incorrect for payloads produced on a different platform.");
        }
    }

    schema.ClearArrayLengthFlags();
    bool valid = true; // Is the schema valid?

    // Finalize schema entries.
    size_t nextOffset = 0;  // Next implicit offset used when an entry has no explicit offset.
    size_t schemaAlign = 1; // Max effective member alignment; 1 is the safe minimum.
    for (size_t i = 0; i < schema.entries.size(); ++i)
    {
        auto& entry = schema.entries[i];
        entry.arrayLayout = GetArrayLayout(entry.flags);
        entry.typeCategory = GetTypeCategory(entry.type);

        // Mark entries that serve as array length.
        if (entry.arrayLayout == PayloadArrayLayout::LengthIndex)
        {
            const int64_t idx = static_cast<int64_t>(entry.arrayOrUnionDetail);
            if (idx >= 0 && static_cast<size_t>(idx) < i)
            {
                schema.entries[static_cast<size_t>(idx)].isArrayLength = true;
            }
            else
            {
                NVTX_PAYLOAD_LOG_ERROR(
                    "Illegal schema '%s' (id=%" PRIu64 "), entry[%zu] '%s' "
                    "references invalid entry index %" PRIi64 ".",
                    PrintSchemaName(schema),
                    schemaId,
                    i,
                    PrintEntryName(entry),
                    idx);
                valid = false;
            }
        }

        // Resolve element size for nested schema types (must already be finalized).
        if (IsEmbeddedFixedStringEntry(entry))
        {
            entry.elementSize = GetStringCharacterSize(entry.type);
        }
        else if (
            (entry.arrayLayout == PayloadArrayLayout::LengthIndex ||
             entry.arrayLayout == PayloadArrayLayout::ZeroTerminated) &&
            IsCStringType(entry.type))
        {
            // Variable-length embedded string: length from another entry or zero-terminated;
            // element = one character.
            entry.elementSize = GetStringCharacterSize(entry.type);
        }
        else
        {
            entry.elementSize = GetEntryElementSize(entry.type, schemas, enums);
        }

        // Resolve entry kind for custom types (nested schema or enum).
        if (entry.type >= NVTX_PAYLOAD_SCHEMA_ID_STATIC_START)
        {
            if (schemas.count(entry.type))
                entry.kind = PayloadEntryKind::NestedSchema;
            else if (enums.count(entry.type))
                entry.kind = PayloadEntryKind::Enum;
            else
                entry.kind = PayloadEntryKind::Unknown;
        }

        // Arrays require a fixed element stride; dynamic schemas have variable size.
        if (entry.kind == PayloadEntryKind::NestedSchema &&
            entry.arrayLayout != PayloadArrayLayout::None)
        {
            const auto& nestedSchema = schemas.find(entry.type)->second;
            if (nestedSchema.type == NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC)
            {
                NVTX_PAYLOAD_LOG_ERROR(
                    "Illegal schema '%s' (id=%" PRIu64 "), entry[%zu] '%s': "
                    "array element type references dynamic schema '%s'. "
                    "Array elements must have a fixed size.",
                    PrintSchemaName(schema),
                    schemaId,
                    i,
                    PrintEntryName(entry),
                    PrintSchemaName(nestedSchema));
                valid = false;
            }
        }

        // Compute compile-time static size for this entry (0 for runtime-sized entries).
        const bool isDynamicSize = (entry.arrayLayout == PayloadArrayLayout::LengthIndex) ||
                                   (entry.arrayLayout == PayloadArrayLayout::ZeroTerminated);
        if (isDynamicSize)
        {
            entry.staticSizeBytes = 0;
        }
        else if (entry.arrayLayout == PayloadArrayLayout::None && IsEmbeddedFixedStringEntry(entry))
        {
            entry.staticSizeBytes =
                static_cast<size_t>(entry.arrayOrUnionDetail) * entry.elementSize;
        }
        else if (entry.arrayLayout == PayloadArrayLayout::FixedSize) // Fixed-size array.
        {
            const size_t length = GetArrayLength(entry);
            const bool staticKnown = (entry.elementSize > 0) && (length > 0);
            entry.staticSizeBytes = staticKnown ? (entry.elementSize * length) : 0;
        }
        else // Scalar entry.
        {
            entry.staticSizeBytes = entry.elementSize;
        }

        // Compute finalized static byte size for this entry when possible.
        const size_t entrySize = GetEntrySize(entry, schemas, enums);
        if (!isDynamicSize && entrySize == 0)
        {
            NVTX_PAYLOAD_LOG_ERROR(
                "Illegal schema '%s' (id=%" PRIu64 "), entry[%zu] '%s': "
                "unable to determine static size for type=%" PRIu64 ".",
                PrintSchemaName(schema),
                schemaId,
                i,
                PrintEntryName(entry),
                entry.type);
            valid = false;
            continue;
        }

        // Compute effective member alignment (also used for final schema alignment).
        size_t typeAlign = 1;
        if (entry.type >= NVTX_PAYLOAD_SCHEMA_ID_STATIC_START)
        {
            // Custom type (nested schema/enum) is already finalized.
            const auto nestedIt = schemas.find(entry.type);
            if (nestedIt != schemas.end() && nestedIt->second.alignment > 0)
            {
                typeAlign = nestedIt->second.alignment;
            }
            else
            {
                const auto enumIt = enums.find(entry.type);
                const size_t enumSize = (enumIt != enums.end()) ? enumIt->second.sizeOfEnum : 0;
                if (enumSize > 0)
                {
                    typeAlign = MinValue(enumSize, size_t{8});
                }
                else
                {
                    NVTX_PAYLOAD_LOG_ERROR(
                        "Illegal schema '%s' (id=%" PRIu64 "), entry[%zu] '%s': "
                        "unable to resolve alignment for custom type=%" PRIu64 ".",
                        PrintSchemaName(schema),
                        schemaId,
                        i,
                        PrintEntryName(entry),
                        entry.type);
                    valid = false;
                    // Keep finalization progressing to surface additional errors in one pass.
                    typeAlign = MinValue(entrySize, size_t{8});
                }
            }
        }
        else // Predefined type.
        {
            typeAlign = IsEmbeddedCStringEntry(entry) ? GetStringCharacterSize(entry.type)
                                                      : GetTypeAlign(entry.type);
        }

        // Apply packing/alignment cap, if it is set.
        if (schema.packAlign > 0)
        {
            typeAlign = (typeAlign == 0) ? schema.packAlign : MinValue(typeAlign, schema.packAlign);
        }

        // Invalid schemas can leave alignment unresolved in fallback paths.
        assert(typeAlign != 0 && "Entry alignment must be non-zero.");

        schemaAlign = MaxValue(schemaAlign, typeAlign);

        // Use explicit offsets if provided. Implicit offsets are resolved from `nextOffset`.
        const bool hasUserProvidedOffset = !(i > 0 && entry.offset == 0);
        if (isDynamicSize && !hasUserProvidedOffset)
        {
            // Static schemas cannot contain runtime-sized fields with implicit offsets.
            if (schema.type == NVTX_PAYLOAD_SCHEMA_TYPE_STATIC)
            {
                NVTX_PAYLOAD_LOG_ERROR(
                    "Illegal schema '%s' (id=%" PRIu64 "), entry[%zu] '%s': "
                    "static schema uses runtime-sized field with implicit offset.",
                    PrintSchemaName(schema),
                    schemaId,
                    i,
                    PrintEntryName(entry));
                valid = false;
            }
            continue;
        }

        if (hasUserProvidedOffset)
        {
            // Keep implicit placement monotonic even when explicit offsets are out of order.
            // Cast entry.offset so size_t and uint64_t do not fight in MaxValue on 32-bit.
            nextOffset = MaxValue(nextOffset, static_cast<size_t>(entry.offset) + entrySize);
            continue;
        }

        // Use the already computed effective member alignment for implicit placement.
        nextOffset = AlignUp(nextOffset, typeAlign);
        entry.offset = nextOffset;
        nextOffset = MaxValue(nextOffset, static_cast<size_t>(entry.offset) + entrySize);
    }

    // Ensure schema static size covers all finalized entries.
    // Only log an error when the user explicitly provided a size that's too small,
    // or when a static schema has no size at all. Dynamic schemas without an
    // explicit size are silently auto-adjusted to cover the fixed portion.
    const bool userSpecifiedSize = schema.staticPayloadSize > 0;
    const bool explicitSizeTooSmall = userSpecifiedSize && schema.staticPayloadSize < nextOffset;
    const bool staticSchemaMissingSize =
        (schema.type == NVTX_PAYLOAD_SCHEMA_TYPE_STATIC && !userSpecifiedSize);
    if (explicitSizeTooSmall || staticSchemaMissingSize)
    {
        if (staticSchemaMissingSize)
        {
            NVTX_PAYLOAD_LOG_ERROR(
                "Illegal schema '%s' (id=%" PRIu64 "): "
                "static schema has no size specified (required %zu bytes).",
                PrintSchemaName(schema),
                schemaId,
                nextOffset);
        }
        else
        {
            NVTX_PAYLOAD_LOG_ERROR(
                "Illegal schema '%s' (id=%" PRIu64 "): "
                "static size %zu is less than required %zu.",
                PrintSchemaName(schema),
                schemaId,
                schema.staticPayloadSize,
                nextOffset);
        }
    }

    // Auto-adjust to cover the known fixed entries.
    if (schema.staticPayloadSize < nextOffset)
    {
        schema.staticPayloadSize = nextOffset;
    }

    // Set the final schema alignment.
    schema.alignment = schemaAlign;

    return valid;
}
