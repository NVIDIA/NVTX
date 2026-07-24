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

/** Shared helpers for payload schema finalization and decoding. */

#pragma once

#include <cstdio>

#include "NvtxPayloadSchema.h"

#define NVTX_PAYLOAD_LOG_ERROR(fmt, ...)                                                           \
    do                                                                                             \
    {                                                                                              \
        std::fprintf(stderr, "[NVTX] " fmt "\n", ##__VA_ARGS__);                                   \
    } while (0)

/** Rounds `value` up to the next multiple of `alignment` (power of two). */
inline size_t AlignUp(size_t value, size_t alignment)
{
    if (alignment == 0)
    {
        return value;
    }
    const size_t mask = alignment - 1;
    // Round up to the next `alignment` boundary.
    return (value + mask) & ~mask;
}

/** Prefer over std::min/std::max to avoid Windows min/max macro conflicts. */
template <typename T>
inline T MinValue(T a, T b)
{
    return a < b ? a : b;
}

template <typename T>
inline T MaxValue(T a, T b)
{
    return a > b ? a : b;
}

/** Returns the type category for a predefined NVTX entry type. */
inline TypeCategoryFlags GetTypeCategory(uint64_t type)
{
    switch (type)
    {
    case NVTX_PAYLOAD_ENTRY_TYPE_UCHAR:
    case NVTX_PAYLOAD_ENTRY_TYPE_USHORT:
    case NVTX_PAYLOAD_ENTRY_TYPE_UINT:
    case NVTX_PAYLOAD_ENTRY_TYPE_ULONG:
    case NVTX_PAYLOAD_ENTRY_TYPE_ULONGLONG:
    case NVTX_PAYLOAD_ENTRY_TYPE_UINT8:
    case NVTX_PAYLOAD_ENTRY_TYPE_UINT16:
    case NVTX_PAYLOAD_ENTRY_TYPE_UINT32:
    case NVTX_PAYLOAD_ENTRY_TYPE_UINT64:
    case NVTX_PAYLOAD_ENTRY_TYPE_SIZE:
    case NVTX_PAYLOAD_ENTRY_TYPE_CATEGORY:
    case NVTX_PAYLOAD_ENTRY_TYPE_SCOPE_ID:
    case NVTX_PAYLOAD_ENTRY_TYPE_RANGE_ID:
    case NVTX_PAYLOAD_ENTRY_TYPE_PID_UINT32:
    case NVTX_PAYLOAD_ENTRY_TYPE_PID_UINT64:
    case NVTX_PAYLOAD_ENTRY_TYPE_TID_UINT32:
    case NVTX_PAYLOAD_ENTRY_TYPE_TID_UINT64:
    case NVTX_PAYLOAD_ENTRY_TYPE_UNION_SELECTOR:
        return kTypeCategoryUnsignedInteger;
    case NVTX_PAYLOAD_ENTRY_TYPE_ADDRESS:
    case NVTX_PAYLOAD_ENTRY_TYPE_BYTE:
    case NVTX_PAYLOAD_ENTRY_TYPE_COLOR_ARGB:
        return kTypeCategoryHexBytes;
    case NVTX_PAYLOAD_ENTRY_TYPE_CHAR:
    case NVTX_PAYLOAD_ENTRY_TYPE_CHAR8:
    case NVTX_PAYLOAD_ENTRY_TYPE_CHAR16:
    case NVTX_PAYLOAD_ENTRY_TYPE_CHAR32:
    case NVTX_PAYLOAD_ENTRY_TYPE_WCHAR:
    case NVTX_PAYLOAD_ENTRY_TYPE_SHORT:
    case NVTX_PAYLOAD_ENTRY_TYPE_INT:
    case NVTX_PAYLOAD_ENTRY_TYPE_LONG:
    case NVTX_PAYLOAD_ENTRY_TYPE_LONGLONG:
    case NVTX_PAYLOAD_ENTRY_TYPE_INT8:
    case NVTX_PAYLOAD_ENTRY_TYPE_INT16:
    case NVTX_PAYLOAD_ENTRY_TYPE_INT32:
    case NVTX_PAYLOAD_ENTRY_TYPE_INT64:
        return kTypeCategorySignedInteger;
    case NVTX_PAYLOAD_ENTRY_TYPE_FLOAT:
    case NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE:
    case NVTX_PAYLOAD_ENTRY_TYPE_LONGDOUBLE:
    case NVTX_PAYLOAD_ENTRY_TYPE_FLOAT16:
    case NVTX_PAYLOAD_ENTRY_TYPE_FLOAT32:
    case NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64:
    case NVTX_PAYLOAD_ENTRY_TYPE_FLOAT128:
    case NVTX_PAYLOAD_ENTRY_TYPE_BF16:
    case NVTX_PAYLOAD_ENTRY_TYPE_TF32:
        return kTypeCategoryFloatingPoint;
    case NVTX_PAYLOAD_ENTRY_TYPE_CSTRING:
    case NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF8:
    case NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF16:
    case NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32:
        return kTypeCategoryCString;
    case NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE:
        return kTypeCategoryRegisteredString;
    default:
        return kTypeCategoryNone;
    }
}

/** True for any NVTX C-string entry type (UTF-8, UTF-16, UTF-32). */
inline bool IsCStringType(uint64_t type)
{
    return type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING ||
           type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF8 ||
           type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF16 ||
           type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32;
}

/** Returns the byte width of one character for a C-string type, or 0. */
inline size_t GetStringCharacterSize(uint64_t type)
{
    if (type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF16)
    {
        return sizeof(char16_t);
    }

    if (type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32)
    {
        return sizeof(char32_t);
    }

    if (type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING || type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF8)
    {
        return sizeof(char);
    }

    return 0;
}

/** True when the entry is a fixed-size C-string stored inline (not a pointer). */
inline bool IsEmbeddedFixedStringEntry(const PayloadSchemaEntry& entry)
{
    if (!IsCStringType(entry.type))
    {
        return false;
    }

    if (entry.arrayOrUnionDetail == 0) // Check for fixed size.
    {
        return false;
    }

    if ((entry.flags & NVTX_PAYLOAD_ENTRY_FLAG_POINTER) != 0) // Embedded means no pointer.
    {
        return false;
    }

    return entry.arrayLayout == PayloadArrayLayout::None ||
           entry.arrayLayout == PayloadArrayLayout::FixedSize;
}

/** True when the entry stores character data inline (fixed, length-indexed, or zero-terminated).  */
inline bool IsEmbeddedCStringEntry(const PayloadSchemaEntry& entry)
{
    return IsEmbeddedFixedStringEntry(entry) ||
           ((entry.arrayLayout == PayloadArrayLayout::LengthIndex ||
             entry.arrayLayout == PayloadArrayLayout::ZeroTerminated) &&
            IsCStringType(entry.type));
}

/** Returns the fixed array element count, or 1 if not a fixed-size array. */
inline size_t GetArrayLength(const PayloadSchemaEntry& entry)
{
    if (NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_TYPE(entry.flags) != NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE)
    {
        return 1;
    }
    return static_cast<size_t>(entry.arrayOrUnionDetail);
}
