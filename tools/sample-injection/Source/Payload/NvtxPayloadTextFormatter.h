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

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "NvtxPayloadParser.h"

class NvtxPayloadTextVisitor : public PayloadStreamVisitor
{
  public:
    void OnBeginPayloads(size_t count) override;
    void OnPayloadBegin(
        size_t payloadIndex,
        uint64_t schemaId,
        size_t payloadSize,
        std::string_view schemaName) override;
    void OnPayloadEnd() override;
    void OnFieldBegin(std::string_view name, std::string_view description) override;
    void OnFieldEnd() override;
    void OnArrayBegin(size_t length) override;
    void OnArrayEnd() override;
    void OnObjectBegin() override;
    void OnObjectEnd() override;
    void OnSignedInteger(int64_t value) override;
    void OnUnsignedInteger(uint64_t value) override;
    void OnFloatingPoint(double value) override;
    void OnString(std::string_view value) override;
    void OnRawBytes(const uint8_t* data, size_t size) override;
    std::string Finish();

  private:
    enum class ContainerKind
    {
        PayloadFields,
        ObjectFields,
        ArrayValues
    };
    struct ContainerState
    {
        ContainerKind kind;
        bool first;
    };

    void PrepareArrayElement();
    void MarkFieldValueEmitted();
    void AppendHexBytes(const uint8_t* data, size_t size);

    template <typename T>
    void AppendNumber(T value)
    {
        char buf[24];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
        out_.append(buf, ptr);
    }

    std::vector<ContainerState> containers_;
    std::vector<bool> fieldHasValue_;
    std::string out_;
};
