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

#include "NvtxPayloadJsonFormatter.h"

std::string NvtxPayloadJsonVisitor::EscapeJson(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (char c : value)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

void NvtxPayloadJsonVisitor::OnBeginPayloads(size_t count)
{
    containers_.clear();
    fieldHasValue_.clear();
    fieldCounter_.clear();
    out_.clear();
    out_ += "{\"count\":";
    AppendNumber(count);
    out_ += ",\"payloads\":[";
}

void NvtxPayloadJsonVisitor::OnPayloadBegin(
    size_t payloadIndex, uint64_t schemaId, size_t payloadSize, const std::string& schemaName)
{
    if (payloadIndex > 0)
    {
        out_ += ',';
    }
    out_ += "{\"schemaId\":";
    AppendNumber(schemaId);
    out_ += ",\"size\":";
    AppendNumber(payloadSize);
    out_ += ",\"schemaName\":";
    if (!schemaName.empty())
    {
        out_ += '"';
        out_ += EscapeJson(schemaName);
        out_ += '"';
    }
    else
    {
        out_ += "null";
    }
    out_ += ",\"fields\":{";

    containers_.push_back({ContainerKind::PayloadFields, true});

    fieldCounter_.push_back(0);
}

void NvtxPayloadJsonVisitor::OnPayloadEnd()
{
    if (!containers_.empty())
    {
        containers_.pop_back();
    }

    if (!fieldCounter_.empty())
    {
        fieldCounter_.pop_back();
    }

    out_ += "}}";
}

void NvtxPayloadJsonVisitor::OnFieldBegin(const std::string& name, const std::string&)
{
    if (!containers_.empty() && (containers_.back().kind == ContainerKind::PayloadFields ||
                                 containers_.back().kind == ContainerKind::ObjectFields))
    {
        if (!containers_.back().first)
            out_ += ',';
        containers_.back().first = false;
    }

    std::string uniqueName;
    const std::string* key = &name;
    if (name.empty() && !fieldCounter_.empty())
    {
        uniqueName = "<field_" + std::to_string(fieldCounter_.back()++) + ">";
        key = &uniqueName;
    }

    out_ += '"';
    out_ += EscapeJson(*key);
    out_ += "\":";
    fieldHasValue_.push_back(false);
}

void NvtxPayloadJsonVisitor::OnFieldEnd()
{
    if (fieldHasValue_.empty())
        return;
    if (!fieldHasValue_.back())
    {
        out_ += "null";
    }
    fieldHasValue_.pop_back();
}

void NvtxPayloadJsonVisitor::OnArrayBegin(size_t)
{
    PrepareArrayElement();
    out_ += '[';
    containers_.push_back({ContainerKind::ArrayValues, true});
    MarkFieldValueEmitted();
}

void NvtxPayloadJsonVisitor::OnArrayEnd()
{
    if (!containers_.empty())
    {
        containers_.pop_back();
    }
    out_ += ']';
}

void NvtxPayloadJsonVisitor::OnObjectBegin()
{
    PrepareArrayElement();
    out_ += '{';
    containers_.push_back({ContainerKind::ObjectFields, true});
    fieldCounter_.push_back(0);
    MarkFieldValueEmitted();
}

void NvtxPayloadJsonVisitor::OnObjectEnd()
{
    if (!containers_.empty())
    {
        containers_.pop_back();
    }

    if (!fieldCounter_.empty())
    {
        fieldCounter_.pop_back();
    }

    out_ += '}';
}

void NvtxPayloadJsonVisitor::OnSignedInteger(int64_t value)
{
    PrepareArrayElement();
    AppendNumber(value);
    MarkFieldValueEmitted();
}

void NvtxPayloadJsonVisitor::OnUnsignedInteger(uint64_t value)
{
    PrepareArrayElement();
    AppendNumber(value);
    MarkFieldValueEmitted();
}

void NvtxPayloadJsonVisitor::OnFloatingPoint(double value)
{
    PrepareArrayElement();
    AppendNumber(value);
    MarkFieldValueEmitted();
}

void NvtxPayloadJsonVisitor::OnString(const std::string& value)
{
    PrepareArrayElement();
    out_ += '"';
    out_ += EscapeJson(value);
    out_ += '"';
    MarkFieldValueEmitted();
}

void NvtxPayloadJsonVisitor::OnRawBytes(const uint8_t* data, size_t size)
{
    PrepareArrayElement();
    out_ += '"';
    AppendHexBytes(data, size);
    out_ += '"';
    MarkFieldValueEmitted();
}

void NvtxPayloadJsonVisitor::PrepareArrayElement()
{
    if (containers_.empty())
        return;
    auto& top = containers_.back();
    if (top.kind != ContainerKind::ArrayValues)
        return;
    if (!top.first)
        out_ += ',';
    top.first = false;
}

void NvtxPayloadJsonVisitor::MarkFieldValueEmitted()
{
    if (!fieldHasValue_.empty())
    {
        fieldHasValue_.back() = true;
    }
}

void NvtxPayloadJsonVisitor::AppendHexBytes(const uint8_t* data, size_t size)
{
    // Nibble-to-hex lookup table: index 0..15 maps to '0'..'f'.
    static constexpr char kHex[] = "0123456789abcdef";
    out_ += "0x";
    for (size_t i = 0; i < size; ++i)
    {
        out_ += kHex[data[i] >> 4];   // High nibble (bits 7..4).
        out_ += kHex[data[i] & 0x0F]; // Low nibble  (bits 3..0).
    }
}

std::string NvtxPayloadJsonVisitor::Finish()
{
    out_ += "]}";
    return std::move(out_);
}
