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

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include <nvtx3/nvToolsExtPayload.h>

/** Normalized array-shape classification derived from NVTX entry flags. */
enum class PayloadArrayLayout
{
    None,          /** Scalar field (non-array). */
    FixedSize,     /** Fixed-size array (`arrayOrUnionDetail` stores element count). */
    LengthIndex,   /** Runtime-sized array (`arrayOrUnionDetail` points to length source index). */
    ZeroTerminated /** Runtime-sized array terminated by a zero element. */
};

/**
 * Coarse type classification/category for predefined NVTX payload entry types.
 * Cached per entry during schema finalization to optimize parsing performance.
 */
enum TypeCategoryFlags : uint8_t
{
    kTypeCategoryNone = 0,
    kTypeCategorySignedInteger = 1u << 0,
    kTypeCategoryUnsignedInteger = 1u << 1,
    kTypeCategoryFloatingPoint = 1u << 2,
    kTypeCategoryCString = 1u << 3,
    kTypeCategoryHexBytes = 1u << 4,
    kTypeCategoryRegisteredString = 1u << 5,

    kTypeCategoryInteger = kTypeCategorySignedInteger | kTypeCategoryUnsignedInteger
};

/** Resolved kind of a schema entry's type after finalization. */
enum class PayloadEntryKind
{
    Predefined,   /** Built-in NVTX type (integer, float, string, etc.). */
    NestedSchema, /** References a registered payload schema. */
    Enum,         /** References a registered payload enum. */
    Unknown       /** Custom ID that could not be resolved during finalization. */
};

/** Internal normalized representation of one registered schema entry. */
struct PayloadSchemaEntry
{
    /** NVTX entry flags. */
    uint64_t flags;

    /** Predefined NVTX type ID or nested schema/enum ID. */
    uint64_t type;

    /** Optional entry label/name from schema registration. */
    std::string name;

    /** Optional human-readable entry description from schema registration. */
    std::string description;

    /** Array length/index or union selector. */
    uint64_t arrayOrUnionDetail;

    /** Byte offset from the start of the containing payload object. */
    uint64_t offset;

    /**
     * Total size of the whole entry when statically known, or 0 for dynamic/unknown entries.
     * For example, for a fixed-size array of 3 elements, the static size is 3 * elementSize.
     */
    size_t staticSizeBytes = 0;

    /** Size in bytes of one element (array stride/base type or character size for strings). */
    size_t elementSize = 0;

    /** True when this entry is referenced as an ARRAY_LENGTH_INDEX source. */
    bool isArrayLength = false;

    /** Array layout, if entry is an array. */
    PayloadArrayLayout arrayLayout = PayloadArrayLayout::None;

    /** Resolved kind of this entry's type (set during finalization). */
    PayloadEntryKind kind = PayloadEntryKind::Predefined;

    /** Coarse type category (set during schema finalization). */
    TypeCategoryFlags typeCategory = kTypeCategoryNone;
};

/** Normalized representation of one registered payload schema. */
struct PayloadSchema
{
    /** Schema name from registration. */
    std::string name;

    uint64_t type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC;

    /** Optional packing/alignment cap for implicit offset calculation. */
    size_t packAlign;

    /**
     * Static size of the associated payload. Set by the user during registration
     * or during schema finalization for static schemas. 0 for dynamic schemas.
     */
    size_t staticPayloadSize;
    /** Finalized schema alignment in bytes (max effective member alignment, pack-capped). */
    size_t alignment = 1;

    /** Ordered schema entries. */
    std::vector<PayloadSchemaEntry> entries;

    void ClearArrayLengthFlags()
    {
        for (auto& entry : entries)
        {
            entry.isArrayLength = false;
        }
    }
};

/** One named enum value/flag entry. */
struct PayloadEnumEntry
{
    std::string name;
    uint64_t value = 0;
    bool isFlag = false;
};

/** Registered enum metadata used for name/flag resolution during decode. */
struct PayloadEnum
{
    /** Enum name from registration. */
    std::string name;

    /** Size of enum in bytes. */
    size_t sizeOfEnum = 0;

    /** Exact enum value -> display name lookup. */
    std::unordered_map<uint64_t, std::string> valueNames;

    /** Flag entries used to resolve bitset combinations. */
    std::vector<PayloadEnumEntry> flagEntries;
};

class NvtxPayloadSchemaProcessor
{
  public:
    explicit NvtxPayloadSchemaProcessor(const nvtxPayloadEntryTypeInfo_t* typeInfo = nullptr)
        : typeInfo_(typeInfo)
    {}

    /** Returns byte size for a predefined type ID, or 0 if unknown. */
    size_t GetTypeSize(uint64_t type) const;

    /** Returns byte alignment for a predefined type ID, falling back to size-based defaults. */
    size_t GetTypeAlign(uint64_t type) const;

    /**
     * Eagerly finalizes one schema's field layout and size metadata.
     *
     * All nested schema dependencies must already be registered and finalized.
     * Returns `false` for invalid layouts (for example unresolved dependencies,
     * bad length indices, or unsupported static dynamic-sized placement).
     */
    bool FinalizeSchema(
        uint64_t schemaId,
        std::unordered_map<uint64_t, PayloadSchema>& schemas,
        const std::unordered_map<uint64_t, PayloadEnum>& enums) const;

  private:
    /** Returns total byte size of one entry when statically known; 0 for dynamic/unknown entries.
     */
    size_t GetEntrySize(
        const PayloadSchemaEntry& entry,
        const std::unordered_map<uint64_t, PayloadSchema>& schemas,
        const std::unordered_map<uint64_t, PayloadEnum>& enums) const;

    /** Resolves size of predefined, nested schema, or enum types. */
    size_t GetEntryElementSize(
        uint64_t type,
        const std::unordered_map<uint64_t, PayloadSchema>& schemas,
        const std::unordered_map<uint64_t, PayloadEnum>& enums) const;

    /** Optional NVTX-provided type info table for predefined types. */
    const nvtxPayloadEntryTypeInfo_t* typeInfo_ = nullptr;
};
