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

#include "NvtxPayloadTextFormatter.h"

void NvtxPayloadTextVisitor::OnBeginPayloads(size_t count)
{
    containers_.clear();
    fieldHasValue_.clear();
    out_.clear();
    out_ += "count=";
    AppendNumber(count);
}

void NvtxPayloadTextVisitor::OnPayloadBegin(
    size_t payloadIndex, uint64_t schemaId, size_t payloadSize, const std::string& schemaName)
{
    out_ += " payload[";
    AppendNumber(payloadIndex);
    out_ += "]={schemaId=";
    AppendNumber(schemaId);
    out_ += ", size=";
    AppendNumber(payloadSize);
    if (!schemaName.empty())
    {
        out_ += ", schemaName=";
        out_ += schemaName;
    }
    out_ += ", fields=[";
    containers_.push_back({ContainerKind::PayloadFields, true});
}

void NvtxPayloadTextVisitor::OnPayloadEnd()
{
    if (!containers_.empty())
    {
        containers_.pop_back();
    }
    out_ += "]}";
}

void NvtxPayloadTextVisitor::OnFieldBegin(const std::string& name, const std::string& description)
{
    if (!containers_.empty() && (containers_.back().kind == ContainerKind::PayloadFields ||
                                 containers_.back().kind == ContainerKind::ObjectFields))
    {
        if (!containers_.back().first)
        {
            out_ += ", ";
        }
        containers_.back().first = false;
    }
    out_ += (name.empty() ? "<field>" : name);
    if (!description.empty())
    {
        out_ += " (";
        out_ += description;
        out_ += ')';
    }
    out_ += '=';
    fieldHasValue_.push_back(false);
}

void NvtxPayloadTextVisitor::OnFieldEnd()
{
    if (fieldHasValue_.empty())
        return;
    if (!fieldHasValue_.back())
    {
        out_ += "<unsupported>";
    }
    fieldHasValue_.pop_back();
}

void NvtxPayloadTextVisitor::OnArrayBegin(size_t)
{
    PrepareArrayElement();
    out_ += '[';
    containers_.push_back({ContainerKind::ArrayValues, true});
    MarkFieldValueEmitted();
}

void NvtxPayloadTextVisitor::OnArrayEnd()
{
    if (!containers_.empty())
    {
        containers_.pop_back();
    }
    out_ += ']';
}

void NvtxPayloadTextVisitor::OnObjectBegin()
{
    PrepareArrayElement();
    out_ += '{';
    containers_.push_back({ContainerKind::ObjectFields, true});
    MarkFieldValueEmitted();
}

void NvtxPayloadTextVisitor::OnObjectEnd()
{
    if (!containers_.empty())
    {
        containers_.pop_back();
    }
    out_ += '}';
}

void NvtxPayloadTextVisitor::OnSignedInteger(int64_t value)
{
    PrepareArrayElement();
    AppendNumber(value);
    MarkFieldValueEmitted();
}

void NvtxPayloadTextVisitor::OnUnsignedInteger(uint64_t value)
{
    PrepareArrayElement();
    AppendNumber(value);
    MarkFieldValueEmitted();
}

void NvtxPayloadTextVisitor::OnFloatingPoint(double value)
{
    PrepareArrayElement();
    AppendNumber(value);
    MarkFieldValueEmitted();
}

void NvtxPayloadTextVisitor::OnString(const std::string& value)
{
    PrepareArrayElement();
    out_ += value;
    MarkFieldValueEmitted();
}

void NvtxPayloadTextVisitor::OnRawBytes(const uint8_t* data, size_t size)
{
    PrepareArrayElement();
    AppendHexBytes(data, size);
    MarkFieldValueEmitted();
}

void NvtxPayloadTextVisitor::PrepareArrayElement()
{
    if (containers_.empty())
        return;
    auto& top = containers_.back();
    if (top.kind != ContainerKind::ArrayValues)
        return;
    if (!top.first)
        out_ += ", ";
    top.first = false;
}

void NvtxPayloadTextVisitor::MarkFieldValueEmitted()
{
    if (!fieldHasValue_.empty())
    {
        fieldHasValue_.back() = true;
    }
}

void NvtxPayloadTextVisitor::AppendHexBytes(const uint8_t* data, size_t size)
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

std::string NvtxPayloadTextVisitor::Finish()
{
    return std::move(out_);
}
