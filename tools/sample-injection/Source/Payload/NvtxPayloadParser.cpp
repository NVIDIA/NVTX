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

#include "NvtxPayloadParser.h"
#include "NvtxPayloadUtils.h"
#include "UtfStringConversion.h"

#include <cassert>
#include <cinttypes>
#include <cstring>

namespace {

bool IsUtf16CStringType(uint64_t type)
{
    return type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF16;
}

bool IsUtf32CStringType(uint64_t type)
{
    return type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32;
}

// Payload pointer and size are validated once in ProcessPayloads before any
// field decoding begins; only per-field offset/length bounds checks are needed here.
const uint8_t* GetValidPayloadPtr(const nvtxPayloadData_t& payload, uint64_t offset, size_t length)
{
    if (payload.size < static_cast<size_t>(offset))
    {
        return nullptr;
    }

    // Check if the length is within the payload.
    if ((payload.size - static_cast<size_t>(offset)) < length)
    {
        return nullptr;
    }

    return static_cast<const uint8_t*>(payload.payload) + offset;
}

bool ReadBytes(const nvtxPayloadData_t& payload, uint64_t offset, void* outValue, size_t valueSize)
{
    const uint8_t* ptr = GetValidPayloadPtr(payload, offset, valueSize);
    if (!ptr)
    {
        return false;
    }
    std::memcpy(outValue, ptr, valueSize);
    return true;
}

bool ReadUnsigned(
    const nvtxPayloadData_t& payload, uint64_t offset, size_t valueSize, uint64_t& outValue)
{
    outValue = 0;
    if (valueSize == 0 || valueSize > sizeof(uint64_t))
    {
        return false;
    }
    return ReadBytes(payload, offset, &outValue, valueSize);
}

int64_t SignExtendToInt64(uint64_t rawValue, size_t valueSize)
{
    const size_t valueBits = valueSize * 8;
    if (valueBits == 0)
    {
        return 0;
    }

    if (valueBits >= 64)
    {
        return static_cast<int64_t>(rawValue);
    }

    const uint64_t signBit = uint64_t{1} << (valueBits - 1);
    if ((rawValue & signBit) != 0)
    {
        rawValue |= (~uint64_t{0}) << valueBits;
    }

    return static_cast<int64_t>(rawValue);
}

bool ReadSigned(
    const nvtxPayloadData_t& payload, uint64_t offset, size_t valueSize, int64_t& outValue)
{
    uint64_t rawValue = 0;
    if (!ReadUnsigned(payload, offset, valueSize, rawValue))
    {
        return false;
    }
    outValue = SignExtendToInt64(rawValue, valueSize);
    return true;
}

std::string ResolveEnumFlagValue(const PayloadEnum& payloadEnum, uint64_t value)
{
    std::string result;
    for (const auto& flag : payloadEnum.flagEntries)
    {
        if ((flag.value != 0) && ((value & flag.value) == flag.value))
        {
            if (!result.empty())
            {
                result += '|';
            }
            result += flag.name;
        }
    }
    return result;
}

constexpr size_t kSizeMax = static_cast<size_t>(-1);

/**
 * Returns the byte size of a null-terminated string (including the terminator)
 * for a given NVTX C-string type.  Used to resolve SIZE_MAX payload sizes.
 */
size_t GetNullTerminatedStringSize(const void* data, uint64_t type)
{
    if (!data)
    {
        return 0;
    }

    if (type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF16)
    {
        return (std::char_traits<char16_t>::length(static_cast<const char16_t*>(data)) + 1) *
               sizeof(char16_t);
    }
    if (type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32)
    {
        return (std::char_traits<char32_t>::length(static_cast<const char32_t*>(data)) + 1) *
               sizeof(char32_t);
    }

    assert(type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING || type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF8);

    return std::strlen(static_cast<const char*>(data)) + 1;
}

} // namespace

NvtxPayloadParser::FieldDecodeInfo NvtxPayloadParser::BuildFieldDecodeInfo(
    size_t packAlign,
    const PayloadSchemaEntry& entry,
    size_t entryIndex,
    const nvtxPayloadData_t& payload,
    uint64_t baseOffset,
    const std::unordered_map<uint64_t, PayloadSchema>& schemas,
    const ParseState& state) const
{
    // Embedded fixed C-strings store character data inline in the payload struct
    // (not behind a pointer) with a compile-time character count.
    const bool embeddedFixedCString = IsEmbeddedFixedStringEntry(entry);
    const size_t stringCharacterSize = GetStringCharacterSize(entry.type);

    FieldDecodeInfo decode{};
    decode.fieldBaseOffset = baseOffset + entry.offset;
    // For embedded strings the total size is character count * character width;
    // for all other types the element size is precomputed during finalization.
    decode.elementSize = embeddedFixedCString
                             ? static_cast<size_t>(entry.arrayOrUnionDetail) *
                                   ((stringCharacterSize > 0) ? stringCharacterSize : 1)
                             : entry.elementSize;
    decode.arrayLength = 1;
    decode.isArray = (!embeddedFixedCString && (entry.arrayLayout != PayloadArrayLayout::None));

    // Fast path: static-schema entries with compile-time layout do not need
    // dynamic cursor alignment, runtime length lookup, or per-field clamping.
    if (!state.dynamicSchema && entry.arrayLayout != PayloadArrayLayout::LengthIndex &&
        entry.arrayLayout != PayloadArrayLayout::ZeroTerminated)
    {
        if (entry.arrayLayout == PayloadArrayLayout::FixedSize)
        {
            decode.arrayLength = GetArrayLength(entry);
        }
        return decode;
    }

    // --- Slow path: dynamic schemas and runtime-sized arrays ---

    // Dynamic schemas don't store explicit offsets for every field; implicit
    // offsets are computed by advancing a cursor with proper alignment.
    // An offset of 0 on any entry after the first signals "use implicit offset".
    const bool hasUserProvidedOffset = !(entryIndex > 0 && entry.offset == 0);
    if (state.dynamicSchema && !hasUserProvidedOffset)
    {
        // Character width for embedded strings (fixed, length-indexed, or
        // zero-terminated), otherwise the type's natural alignment.
        size_t typeAlign = IsEmbeddedCStringEntry(entry)
                               ? stringCharacterSize
                               : schemaProcessor_.GetTypeAlign(entry.type);

        // Nested schema types use the schema's own finalized alignment if
        // available; fall back to min(elementSize, 8) as a sensible default.
        if (entry.type >= NVTX_PAYLOAD_SCHEMA_ID_STATIC_START && !embeddedFixedCString)
        {
            const auto nestedSchemaIt = schemas.find(entry.type);
            if (nestedSchemaIt != schemas.end() && nestedSchemaIt->second.alignment > 0)
            {
                typeAlign = nestedSchemaIt->second.alignment;
            }
            else if (typeAlign == 1)
            {
                typeAlign = MinValue(entry.elementSize, size_t{8});
            }
        }

        // Pack alignment caps the effective alignment (like #pragma pack).
        if (packAlign > 0)
        {
            typeAlign = (typeAlign == 0) ? packAlign : MinValue(typeAlign, packAlign);
        }
        const size_t localOffset = AlignUp(state.nextDynamicOffset, typeAlign);
        decode.fieldBaseOffset = baseOffset + localOffset;
    }

    // --- Resolve array length based on the entry's array layout ---

    if (entry.arrayLayout == PayloadArrayLayout::FixedSize)
    {
        decode.arrayLength = GetArrayLength(entry);
    }
    else if (entry.arrayLayout == PayloadArrayLayout::LengthIndex)
    {
        // The length comes from a previously-parsed integer field identified by
        // index (arrayOrUnionDetail). That field's value was captured in
        // state.lengthValues during EmitIntegerField.
        decode.arrayLength = 0;
        const int64_t idx = static_cast<int64_t>(entry.arrayOrUnionDetail);
        if (idx >= 0 && static_cast<size_t>(idx) < entryIndex)
        {
            const size_t lenIdx = static_cast<size_t>(idx);
            const auto lenIt = state.lengthValues.find(lenIdx);
            if (lenIt != state.lengthValues.end())
            {
                decode.arrayLength = static_cast<size_t>(lenIt->second);
            }
        }
    }
    else if (entry.arrayLayout == PayloadArrayLayout::ZeroTerminated)
    {
        // Scan forward from fieldBaseOffset looking for a zero-valued element.
        // For single-byte types (integers, chars) this is a plain zero byte;
        // for multi-byte types it means every byte of the element is zero.
        // Validate the full available range once, then scan with a raw pointer
        // to avoid per-element bounds re-validation.
        decode.arrayLength = 0;
        if (decode.elementSize > 0 && payload.size >= static_cast<size_t>(decode.fieldBaseOffset))
        {
            const size_t available = payload.size - static_cast<size_t>(decode.fieldBaseOffset);
            const size_t maxElements = available / decode.elementSize;
            const size_t scanBytes = maxElements * decode.elementSize;
            const uint8_t* base = GetValidPayloadPtr(payload, decode.fieldBaseOffset, scanBytes);
            if (base)
            {
                if (decode.elementSize == 1)
                {
                    // Single-byte types (e.g. uint8_t, int8_t, char): a zero byte
                    // is the terminator; memchr finds it via SIMD/word-sized ops.
                    const void* zeroPos = std::memchr(base, 0, scanBytes);
                    if (zeroPos)
                    {
                        decode.arrayLength =
                            static_cast<size_t>(static_cast<const uint8_t*>(zeroPos) - base);
                        decode.hasTerminator = true;
                    }
                    else
                    {
                        decode.arrayLength = maxElements;
                    }
                }
                else
                {
                    for (size_t i = 0; i < maxElements; ++i)
                    {
                        const uint8_t* elem = base + (i * decode.elementSize);
                        bool isZero = true;
                        for (size_t j = 0; j < decode.elementSize; ++j)
                        {
                            if (elem[j] != 0)
                            {
                                isZero = false;
                                break;
                            }
                        }
                        if (isZero)
                        {
                            decode.hasTerminator = true;
                            break;
                        }
                        ++decode.arrayLength;
                    }
                }
            }
        }
    }

    // Clamp array length to what the remaining payload can actually hold,
    // guarding against corrupt or truncated payloads.
    // Zero-terminated arrays are already bounded by the scan above.
    if (entry.arrayLayout != PayloadArrayLayout::ZeroTerminated)
    {
        if (decode.elementSize > 0 && payload.size >= static_cast<size_t>(decode.fieldBaseOffset))
        {
            const size_t available = payload.size - static_cast<size_t>(decode.fieldBaseOffset);
            const size_t maxElements = available / decode.elementSize;
            if (decode.arrayLength > maxElements)
            {
                decode.arrayLength = maxElements;
            }
        }
        else if (entry.type < NVTX_PAYLOAD_SCHEMA_ID_STATIC_START)
        {
            // Predefined types with no remaining payload space cannot be decoded.
            decode.arrayLength = 0;
        }
    }

    return decode;
}

void NvtxPayloadParser::FinishFieldEmission(
    uint64_t baseOffset,
    const FieldDecodeInfo& decode,
    size_t consumedBytes,
    const ParseContext& context,
    ParseState& state) const
{
    const size_t localFieldOffset = static_cast<size_t>(decode.fieldBaseOffset - baseOffset);
    state.nextDynamicOffset = MaxValue(state.nextDynamicOffset, localFieldOffset + consumedBytes);
    if (decode.isArray)
    {
        context.visitor.OnArrayEnd();
    }
    context.visitor.OnFieldEnd();
}

size_t NvtxPayloadParser::EmitNestedSchemaField(
    const PayloadSchema& nestedSchema,
    const nvtxPayloadData_t& payload,
    const FieldDecodeInfo& decode,
    const ParseContext& context) const
{
    assert(
        decode.elementSize > 0 && "Parent schema finalization guarantees non-zero element size.");

    size_t nestedConsumed = 0;
    for (size_t i = 0; i < decode.arrayLength; ++i)
    {
        const uint64_t nestedBase = decode.fieldBaseOffset + (i * decode.elementSize);
        ParseState nestedState{};
        nestedState.dynamicSchema = (nestedSchema.type == NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC);
        nestedState.nextDynamicOffset = 0;
        context.visitor.OnObjectBegin();
        VisitSchemaFields(nestedSchema, payload, nestedBase, context, nestedState);
        context.visitor.OnObjectEnd();
        nestedConsumed = nestedState.nextDynamicOffset;
    }

    // Static schemas have a fixed element size; arrays use it as stride.
    // Dynamic schemas (always scalar - arrays are rejected at finalization)
    // need the actual consumed bytes from the nested parse.
    if (nestedSchema.type == NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC)
    {
        return nestedConsumed;
    }
    return decode.ConsumedBytes();
}

size_t NvtxPayloadParser::EmitEnumField(
    const PayloadEnum& payloadEnum,
    const nvtxPayloadData_t& payload,
    const FieldDecodeInfo& decode,
    const ParseContext& context) const
{
    const size_t enumElementSize = payloadEnum.sizeOfEnum;
    if (enumElementSize == 0)
    {
        return 0;
    }

    for (size_t i = 0; i < decode.arrayLength; ++i)
    {
        const uint64_t offset = decode.fieldBaseOffset + (i * enumElementSize);
        uint64_t rawValue = 0;
        if (!ReadUnsigned(payload, offset, enumElementSize, rawValue))
        {
            continue;
        }

        // Exact enum value hit avoids temporary string allocation.
        const auto exactIt = payloadEnum.valueNames.find(rawValue);
        if (exactIt != payloadEnum.valueNames.end())
        {
            context.visitor.OnString(exactIt->second);
            continue;
        }

        if (!payloadEnum.flagEntries.empty() && rawValue != 0)
        {
            const std::string resolvedFlags = ResolveEnumFlagValue(payloadEnum, rawValue);
            if (!resolvedFlags.empty())
            {
                context.visitor.OnString(resolvedFlags);
                continue;
            }
        }

        context.visitor.OnUnsignedInteger(rawValue);
    }

    return decode.ConsumedBytes();
}

size_t NvtxPayloadParser::EmitIntegerField(
    const PayloadSchemaEntry& entry,
    size_t entryIndex,
    const nvtxPayloadData_t& payload,
    const FieldDecodeInfo& decode,
    const ParseContext& context,
    ParseState& state) const
{
    const bool isSigned = (entry.typeCategory & kTypeCategorySignedInteger) != 0;

    // A scalar entry marked isArrayLength supplies the array size for an ARRAY_LENGTH_INDEX entry.
    const bool isArrayLengthSource = entry.isArrayLength && !decode.isArray;
    uint64_t capturedLength = 0;
    bool capturedLengthValue = false;

    for (size_t i = 0; i < decode.arrayLength; ++i)
    {
        const uint64_t offset = decode.fieldBaseOffset + (i * decode.elementSize);
        if (isSigned)
        {
            int64_t v = 0;
            if (!ReadSigned(payload, offset, decode.elementSize, v))
                continue;
            context.visitor.OnSignedInteger(v);
            if (isArrayLengthSource && !capturedLengthValue && v >= 0)
            {
                capturedLengthValue = true;
                capturedLength = static_cast<uint64_t>(v);
            }
        }
        else
        {
            uint64_t v = 0;
            if (!ReadUnsigned(payload, offset, decode.elementSize, v))
                continue;
            context.visitor.OnUnsignedInteger(v);
            if (isArrayLengthSource && !capturedLengthValue)
            {
                capturedLengthValue = true;
                capturedLength = v;
            }
        }
    }

    // Store the captured value so later LengthIndex entries can resolve it.
    if (capturedLengthValue)
    {
        state.lengthValues[entryIndex] = capturedLength;
    }

    return decode.ConsumedBytes();
}

size_t NvtxPayloadParser::EmitFloatingPointField(
    const nvtxPayloadData_t& payload,
    const FieldDecodeInfo& decode,
    const ParseContext& context) const
{
    for (size_t i = 0; i < decode.arrayLength; ++i)
    {
        const uint64_t offset = decode.fieldBaseOffset + (i * decode.elementSize);
        if (decode.elementSize == 4)
        {
            float v = 0.0f;
            if (!ReadBytes(payload, offset, &v, sizeof(v)))
                continue;
            context.visitor.OnFloatingPoint(static_cast<double>(v));
        }
        else if (decode.elementSize == 8)
        {
            double v = 0.0;
            if (!ReadBytes(payload, offset, &v, sizeof(v)))
                continue;
            context.visitor.OnFloatingPoint(v);
        }
        else if (decode.elementSize == sizeof(long double))
        {
            long double v = 0.0;
            if (!ReadBytes(payload, offset, &v, sizeof(v)))
                continue;
            context.visitor.OnFloatingPoint(static_cast<double>(v));
        }
        else if (decode.elementSize > 0)
        {
            const uint8_t* ptr = GetValidPayloadPtr(payload, offset, decode.elementSize);
            if (!ptr)
                continue;
            context.visitor.OnRawBytes(ptr, decode.elementSize);
        }
    }
    return decode.ConsumedBytes();
}

size_t NvtxPayloadParser::EmitCStringField(
    const PayloadSchemaEntry& entry,
    const nvtxPayloadData_t& payload,
    const FieldDecodeInfo& decode,
    const ParseContext& context) const
{
    if (IsEmbeddedFixedStringEntry(entry))
    {
        if (decode.elementSize == 0)
        {
            return 0;
        }
        const uint8_t* ptr =
            GetValidPayloadPtr(payload, decode.fieldBaseOffset, decode.elementSize);
        if (ptr)
        {
            if (IsUtf16CStringType(entry.type))
            {
                context.visitor.OnString(Utf16BytesToUtf8(ptr, decode.elementSize, true));
            }
            else if (IsUtf32CStringType(entry.type))
            {
                context.visitor.OnString(Utf32BytesToUtf8(ptr, decode.elementSize, true));
            }
            else
            {
                const void* zeroPos = std::memchr(ptr, '\0', decode.elementSize);
                const size_t length =
                    zeroPos != nullptr
                        ? static_cast<size_t>(static_cast<const uint8_t*>(zeroPos) - ptr)
                        : decode.elementSize;
                context.visitor.OnString(
                    std::string(reinterpret_cast<const char*>(ptr), length));
            }
        }
        return decode.elementSize;
    }

    if (entry.arrayLayout == PayloadArrayLayout::LengthIndex && decode.elementSize > 0 &&
        decode.arrayLength > 0)
    {
        const size_t byteCount = decode.arrayLength * decode.elementSize;
        const uint8_t* ptr = GetValidPayloadPtr(payload, decode.fieldBaseOffset, byteCount);
        if (ptr)
        {
            if (IsUtf16CStringType(entry.type))
            {
                context.visitor.OnString(Utf16BytesToUtf8(ptr, byteCount, true));
            }
            else if (IsUtf32CStringType(entry.type))
            {
                context.visitor.OnString(Utf32BytesToUtf8(ptr, byteCount, true));
            }
            else
            {
                const void* zeroPos = std::memchr(ptr, '\0', byteCount);
                const size_t length =
                    zeroPos != nullptr
                        ? static_cast<size_t>(static_cast<const uint8_t*>(zeroPos) - ptr)
                        : byteCount;
                context.visitor.OnString(
                    std::string(reinterpret_cast<const char*>(ptr), length));
            }
        }
        return byteCount;
    }

    if (entry.arrayLayout == PayloadArrayLayout::ZeroTerminated && decode.elementSize > 0 &&
        (decode.arrayLength > 0 || decode.hasTerminator))
    {
        const size_t byteCount = decode.ConsumedBytesWithTerminator();
        if (byteCount == 0)
        {
            return 0;
        }
        const uint8_t* ptr = GetValidPayloadPtr(payload, decode.fieldBaseOffset, byteCount);
        if (ptr)
        {
            if (IsUtf16CStringType(entry.type))
            {
                context.visitor.OnString(Utf16BytesToUtf8(ptr, byteCount, true));
            }
            else if (IsUtf32CStringType(entry.type))
            {
                context.visitor.OnString(Utf32BytesToUtf8(ptr, byteCount, true));
            }
            else
            {
                const void* zeroPos = std::memchr(ptr, '\0', byteCount);
                const size_t length =
                    zeroPos != nullptr
                        ? static_cast<size_t>(static_cast<const uint8_t*>(zeroPos) - ptr)
                        : byteCount;
                context.visitor.OnString(
                    std::string(reinterpret_cast<const char*>(ptr), length));
            }
        }
        return byteCount;
    }

    for (size_t i = 0; i < decode.arrayLength; ++i)
    {
        const uint64_t offset = decode.fieldBaseOffset + (i * decode.elementSize);
        const void* v = nullptr;
        if (!ReadBytes(payload, offset, &v, sizeof(v)))
        {
            continue;
        }
        if (!v)
        {
            context.visitor.OnString("<null>");
            continue;
        }

        if (IsUtf16CStringType(entry.type))
        {
            auto* p = static_cast<const char16_t*>(v);
            context.visitor.OnString(Utf16ToUtf8(p, std::char_traits<char16_t>::length(p), false));
        }
        else if (IsUtf32CStringType(entry.type))
        {
            auto* p = static_cast<const char32_t*>(v);
            context.visitor.OnString(Utf32ToUtf8(p, std::char_traits<char32_t>::length(p), false));
        }
        else
        {
            context.visitor.OnString(std::string(static_cast<const char*>(v)));
        }
    }

    return (entry.arrayLayout == PayloadArrayLayout::ZeroTerminated)
               ? decode.ConsumedBytesWithTerminator()
               : decode.ConsumedBytes();
}

size_t NvtxPayloadParser::EmitRegisteredStringField(
    const PayloadSchemaEntry& entry,
    const nvtxPayloadData_t& payload,
    const FieldDecodeInfo& decode,
    const ParseContext& context) const
{
    for (size_t i = 0; i < decode.arrayLength; ++i)
    {
        const uint64_t offset = decode.fieldBaseOffset + (i * decode.elementSize);
        nvtxStringHandle_t v{};
        if (!ReadBytes(payload, offset, &v, decode.elementSize))
            continue;
        const auto it = context.registeredStrings.find(v);
        if (it != context.registeredStrings.end())
        {
            context.visitor.OnString(it->second);
        }
        else
        {
            context.visitor.OnString("<unresolved-handle>");
        }
    }

    return (entry.arrayLayout == PayloadArrayLayout::ZeroTerminated)
               ? decode.ConsumedBytesWithTerminator()
               : decode.ConsumedBytes();
}

void NvtxPayloadParser::EmitPredefinedTypePayload(
    const nvtxPayloadData_t& payload, const ParseContext& context) const
{
    // Payload pointer, size, and schema ID are already validated in ProcessPayloads.
    const uint64_t type = payload.schemaId;

    PayloadSchema syntheticSchema{};
    syntheticSchema.type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC;
    syntheticSchema.packAlign = 0;

    syntheticSchema.entries.push_back(PayloadSchemaEntry{});
    PayloadSchemaEntry& entry = syntheticSchema.entries.back();
    entry.type = type;
    entry.typeCategory = GetTypeCategory(type);
    entry.arrayLayout = PayloadArrayLayout::None;

    if (entry.typeCategory & kTypeCategoryCString)
    {
        // C-string payloads contain inline string bytes (not a pointer).
        // Model as an embedded fixed string so EmitCStringField handles it.
        const size_t charSize = GetStringCharacterSize(type);
        entry.arrayOrUnionDetail = payload.size / ((charSize > 0) ? charSize : 1);
    }
    else
    {
        entry.elementSize = schemaProcessor_.GetTypeSize(type);
        if (entry.elementSize > 0 && payload.size > entry.elementSize &&
            (payload.size % entry.elementSize) == 0)
        {
            entry.flags = NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE;
            entry.arrayLayout = PayloadArrayLayout::FixedSize;
            entry.arrayOrUnionDetail = payload.size / entry.elementSize;
        }
    }

    ParseState state{};
    EmitField(syntheticSchema, 0, payload, 0, context, state);
}

void NvtxPayloadParser::EmitField(
    const PayloadSchema& schema,
    size_t entryIndex,
    const nvtxPayloadData_t& payload,
    uint64_t baseOffset,
    const ParseContext& context,
    ParseState& state) const
{
    if (state.abortParsing)
    {
        return;
    }

    const PayloadSchemaEntry& entry = schema.entries[entryIndex];
    context.visitor.OnFieldBegin(entry.name, entry.description);

    const FieldDecodeInfo decode = BuildFieldDecodeInfo(
        schema.packAlign, entry, entryIndex, payload, baseOffset, context.schemas, state);
    if (decode.isArray)
    {
        context.visitor.OnArrayBegin(decode.arrayLength);
    }
    size_t consumedBytes = 0;

    const auto typeCategory = entry.typeCategory;
    if (typeCategory & kTypeCategoryInteger)
    {
        consumedBytes = EmitIntegerField(entry, entryIndex, payload, decode, context, state);
        FinishFieldEmission(baseOffset, decode, consumedBytes, context, state);
        return;
    }

    if (typeCategory & kTypeCategoryFloatingPoint)
    {
        consumedBytes = EmitFloatingPointField(payload, decode, context);
        FinishFieldEmission(baseOffset, decode, consumedBytes, context, state);
        return;
    }

    if (typeCategory & kTypeCategoryCString)
    {
        consumedBytes = EmitCStringField(entry, payload, decode, context);
        FinishFieldEmission(baseOffset, decode, consumedBytes, context, state);
        return;
    }
    if (typeCategory & kTypeCategoryHexBytes)
    {
        for (size_t i = 0; i < decode.arrayLength; ++i)
        {
            const uint64_t offset = decode.fieldBaseOffset + (i * decode.elementSize);
            const uint8_t* ptr = GetValidPayloadPtr(payload, offset, decode.elementSize);
            if (ptr)
            {
                context.visitor.OnRawBytes(ptr, decode.elementSize);
            }
        }
        consumedBytes = decode.ConsumedBytes();
        FinishFieldEmission(baseOffset, decode, consumedBytes, context, state);
        return;
    }
    if (typeCategory & kTypeCategoryRegisteredString)
    {
        consumedBytes = EmitRegisteredStringField(entry, payload, decode, context);
        FinishFieldEmission(baseOffset, decode, consumedBytes, context, state);
        return;
    }

    // Dispatch on resolved kind (set during schema finalization).
    if (entry.kind == PayloadEntryKind::NestedSchema)
    {
        const auto nestedSchemaIt = context.schemas.find(entry.type);
        if (nestedSchemaIt != context.schemas.end())
        {
            consumedBytes = EmitNestedSchemaField(nestedSchemaIt->second, payload, decode, context);
            FinishFieldEmission(baseOffset, decode, consumedBytes, context, state);
            return;
        }
    }

    if (entry.kind == PayloadEntryKind::Enum)
    {
        const auto enumIt = context.enums.find(entry.type);
        if (enumIt != context.enums.end() && enumIt->second.sizeOfEnum > 0)
        {
            consumedBytes = EmitEnumField(enumIt->second, payload, decode, context);
            FinishFieldEmission(baseOffset, decode, consumedBytes, context, state);
            return;
        }
    }

    if (decode.elementSize > 0)
    {
        const uint8_t* ptr =
            GetValidPayloadPtr(payload, decode.fieldBaseOffset, decode.elementSize);
        if (ptr)
        {
            context.visitor.OnRawBytes(ptr, decode.elementSize);
        }
    }
    consumedBytes = decode.ConsumedBytes();
    FinishFieldEmission(baseOffset, decode, consumedBytes, context, state);
}

void NvtxPayloadParser::VisitSchemaFields(
    const PayloadSchema& schema,
    const nvtxPayloadData_t& payload,
    uint64_t baseOffset,
    const ParseContext& context,
    ParseState& state) const
{
    // Parse fields in declaration order; this is required for length-index arrays
    // and for dynamic-layout cursor progression.
    for (size_t i = 0; i < schema.entries.size(); ++i)
    {
        if (state.abortParsing)
        {
            break;
        }

        EmitField(schema, i, payload, baseOffset, context, state);
    }
}

void NvtxPayloadParser::ProcessPayloads(
    const nvtxPayloadData_t* payloadData,
    size_t count,
    const std::unordered_map<uint64_t, PayloadSchema>& schemas,
    const std::unordered_map<uint64_t, PayloadEnum>& enums,
    const std::unordered_map<nvtxStringHandle_t, std::string>& registeredStrings,
    PayloadStreamVisitor& visitor) const
{
    ParseContext context{schemas, enums, registeredStrings, visitor};

    context.visitor.OnBeginPayloads(count);
    if (!payloadData || count == 0)
    {
        context.visitor.OnEndPayloads();
        return;
    }

    for (size_t i = 0; i < count; ++i)
    {
        // Mutable copy: SIZE_MAX resolution may adjust the size field.
        nvtxPayloadData_t payload = payloadData[i];

        // Skip payloads that are fundamentally unusable: no buffer, no data,
        // or an explicitly invalid schema ID.
        if (!payload.payload || payload.size == 0 ||
            payload.schemaId == NVTX_PAYLOAD_ENTRY_TYPE_INVALID)
        {
            NVTX_PAYLOAD_LOG_ERROR(
                "Skipping payload[%zu]: %s.",
                i,
                !payload.payload    ? "payload pointer is null"
                : payload.size == 0 ? "size is zero"
                                    : "schema ID is invalid");
            continue;
        }

        // Resolve SIZE_MAX: the sender defers size determination to the handler.
        // A tool is only required to support SIZE_MAX for null-terminated types.
        if (payload.size == kSizeMax)
        {
            if (payload.schemaId < NVTX_PAYLOAD_SCHEMA_ID_STATIC_START &&
                IsCStringType(payload.schemaId))
            {
                payload.size = GetNullTerminatedStringSize(payload.payload, payload.schemaId);
            }
            else
            {
                NVTX_PAYLOAD_LOG_ERROR(
                    "Skipping payload[%zu]: SIZE_MAX is only supported for "
                    "null-terminated string types (schemaId=%" PRIu64 ").",
                    i,
                    payload.schemaId);
                continue;
            }
        }

        // Referenced payloads are auxiliary blobs pointed to by other payloads
        // in the same event; a tool should not expose them directly.
        if (payload.schemaId == NVTX_TYPE_PAYLOAD_SCHEMA_REFERENCED)
        {
            continue;
        }

        // Raw payloads are opaque blobs; emit them as raw bytes.
        if (payload.schemaId == NVTX_TYPE_PAYLOAD_SCHEMA_RAW)
        {
            context.visitor.OnPayloadBegin(
                i, payload.schemaId, payload.size, "<raw>");
            context.visitor.OnFieldBegin({}, {});
            context.visitor.OnRawBytes(static_cast<const uint8_t*>(payload.payload), payload.size);
            context.visitor.OnFieldEnd();
            context.visitor.OnPayloadEnd();
            continue;
        }

        // Predefined types are decoded directly without a registered schema.
        if (payload.schemaId < NVTX_PAYLOAD_SCHEMA_ID_STATIC_START)
        {
            context.visitor.OnPayloadBegin(
                i, payload.schemaId, payload.size, "<predefined>");
            EmitPredefinedTypePayload(payload, context);
            context.visitor.OnPayloadEnd();
            continue;
        }

        // Custom schema ID must be registered.
        const auto schemaIt = schemas.find(payload.schemaId);
        if (schemaIt == schemas.end())
        {
            NVTX_PAYLOAD_LOG_ERROR(
                "Skipping payload[%zu]: schema ID %" PRIu64 " is not registered.",
                i,
                payload.schemaId);
            continue;
        }

        const PayloadSchema& schema = schemaIt->second;
        context.visitor.OnPayloadBegin(i, payload.schemaId, payload.size, schema.name);

        ParseState state{};
        state.dynamicSchema = (schema.type == NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC);
        state.nextDynamicOffset = 0;

        // Static schemas: warn if payload is smaller than schema static size, but still parse
        // as much as we have (ReadBytes / GetValidPayloadPtr will skip past-end reads).
        if (schema.type == NVTX_PAYLOAD_SCHEMA_TYPE_STATIC &&
            payload.size < schema.staticPayloadSize)
        {
            NVTX_PAYLOAD_LOG_ERROR(
                "Payload[%zu] size %zu is smaller than schema '%s' static size "
                "%zu; parsing available bytes only.",
                i,
                payload.size,
                schema.name.empty() ? "<unnamed>" : schema.name.c_str(),
                schema.staticPayloadSize);
        }

        VisitSchemaFields(schema, payload, 0, context, state);

        context.visitor.OnPayloadEnd();
    }
    context.visitor.OnEndPayloads();
}
