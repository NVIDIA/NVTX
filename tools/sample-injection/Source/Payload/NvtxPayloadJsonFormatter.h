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

#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

// Integer std::to_chars is widely available; floating-point to_chars is not
// (e.g. GCC 10 / libstdc++). Prefer to_chars for integers when present.
#if (defined(_MSC_VER) && _MSC_VER >= 1914) || \
    (defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L)
#include <charconv>
#ifndef NVTX_PAYLOAD_HAS_INT_TO_CHARS
#define NVTX_PAYLOAD_HAS_INT_TO_CHARS 1
#endif
#endif

#include "NvtxPayloadParser.h"

class NvtxPayloadJsonVisitor : public PayloadStreamVisitor
{
  public:
    void OnBeginPayloads(size_t count) override;
    void OnPayloadBegin(
        size_t payloadIndex,
        uint64_t schemaId,
        size_t payloadSize,
        const std::string& schemaName) override;
    void OnPayloadEnd() override;
    void OnFieldBegin(const std::string& name, const std::string& description) override;
    void OnFieldEnd() override;
    void OnArrayBegin(size_t length) override;
    void OnArrayEnd() override;
    void OnObjectBegin() override;
    void OnObjectEnd() override;
    void OnSignedInteger(int64_t value) override;
    void OnUnsignedInteger(uint64_t value) override;
    void OnFloatingPoint(double value) override;
    void OnString(const std::string& value) override;
    void OnRawBytes(const uint8_t* data, size_t size) override;

    std::string Finish();

  private:
    // Identifies which JSON container is currently open at a given nesting level.
    // Payload/Object containers hold "name: value" fields, while Array holds values.
    enum class ContainerKind
    {
        PayloadFields,
        ObjectFields,
        ArrayValues
    };

    // Per-container write state used to emit separators correctly.
    // `first` is true until the first field/element is emitted, then false so
    // subsequent writes can prepend a comma.
    struct ContainerState
    {
        ContainerKind kind;
        bool first;
    };

    void PrepareArrayElement();
    void MarkFieldValueEmitted();
    static std::string EscapeJson(const std::string& value);
    void AppendHexBytes(const uint8_t* data, size_t size);

    template <typename T>
    typename std::enable_if<std::is_floating_point<T>::value, void>::type AppendNumber(T value)
    {
        // Classic ("C") locale keeps '.' as the decimal separator regardless of LC_NUMERIC.
        // Avoid floating-point std::to_chars (missing on older libstdc++).
        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        oss << std::setprecision(17) << static_cast<double>(value);
        out_.append(oss.str());
    }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value, void>::type AppendNumber(T value)
    {
#if defined(NVTX_PAYLOAD_HAS_INT_TO_CHARS)
        char buf[24];
        const std::to_chars_result result = std::to_chars(buf, buf + sizeof(buf), value);
        if (result.ec == std::errc())
        {
            out_.append(buf, static_cast<size_t>(result.ptr - buf));
        }
#else
        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        // Unary plus promotes character-sized integers to printable integers.
        oss << +value;
        out_.append(oss.str());
#endif
    }

    // Stack of currently open JSON containers (payload fields, nested objects, arrays).
    std::vector<ContainerState> containers_;

    // Tracks whether the current field already produced a value. If false at
    // field end, we emit `null` to keep valid JSON output.
    std::vector<bool> fieldHasValue_;

    // Per-scope field counter used to generate unique keys for unnamed fields,
    // avoiding duplicate JSON keys.
    std::vector<size_t> fieldCounter_;

    std::string out_;
};
