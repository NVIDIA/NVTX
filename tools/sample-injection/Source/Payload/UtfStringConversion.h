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

#include <cstring>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace detail {

inline void AppendUtf8CodePoint(std::string& out, uint32_t cp)
{
    if (cp <= 0x7F)
    {
        out.push_back(static_cast<char>(cp));
    }
    else if (cp <= 0x7FF)
    {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else if (cp <= 0xFFFF)
    {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

} // namespace detail

inline std::string Utf16ToUtf8(const char16_t* data, size_t count, bool stopAtTerminator)
{
    std::string out;
    if (!data)
    {
        return out;
    }
    out.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        const uint16_t cu1 = static_cast<uint16_t>(data[i]);
        if (stopAtTerminator && cu1 == 0)
        {
            break;
        }

        uint32_t codePoint = 0xFFFD;
        if (cu1 >= 0xD800 && cu1 <= 0xDBFF)
        {
            if ((i + 1) < count)
            {
                const uint16_t cu2 = static_cast<uint16_t>(data[i + 1]);
                if (cu2 >= 0xDC00 && cu2 <= 0xDFFF)
                {
                    codePoint = 0x10000 + ((static_cast<uint32_t>(cu1 - 0xD800) << 10) |
                                           static_cast<uint32_t>(cu2 - 0xDC00));
                    ++i;
                }
            }
        }
        else if (!(cu1 >= 0xDC00 && cu1 <= 0xDFFF))
        {
            codePoint = cu1;
        }

        detail::AppendUtf8CodePoint(out, codePoint);
    }
    return out;
}

inline std::string Utf32ToUtf8(const char32_t* data, size_t count, bool stopAtTerminator)
{
    std::string out;
    if (!data)
    {
        return out;
    }
    out.reserve(count * 2);
    for (size_t i = 0; i < count; ++i)
    {
        uint32_t codePoint = static_cast<uint32_t>(data[i]);
        if (stopAtTerminator && codePoint == 0)
        {
            break;
        }
        if (codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
        {
            codePoint = 0xFFFD;
        }
        detail::AppendUtf8CodePoint(out, codePoint);
    }
    return out;
}

inline std::string Utf16BytesToUtf8(const uint8_t* data, size_t byteCount, bool stopAtTerminator)
{
    const size_t unitCount = byteCount / sizeof(char16_t);
    std::vector<char16_t> units(unitCount);
    if (unitCount > 0)
    {
        std::memcpy(units.data(), data, unitCount * sizeof(char16_t));
    }
    return Utf16ToUtf8(units.data(), units.size(), stopAtTerminator);
}

inline std::string Utf32BytesToUtf8(const uint8_t* data, size_t byteCount, bool stopAtTerminator)
{
    const size_t unitCount = byteCount / sizeof(char32_t);
    std::vector<char32_t> units(unitCount);
    if (unitCount > 0)
    {
        std::memcpy(units.data(), data, unitCount * sizeof(char32_t));
    }
    return Utf32ToUtf8(units.data(), units.size(), stopAtTerminator);
}
