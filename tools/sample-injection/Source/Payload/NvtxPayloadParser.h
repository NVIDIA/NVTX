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

#include <nvtx3/nvToolsExtPayload.h>

#include "NvtxPayloadSchema.h"

/**
 * Streaming callback interface for decoded payload values.
 *
 * The parser emits a depth-first event stream:
 * - payload envelope (`OnBeginPayloads`, `OnPayloadBegin`, `OnPayloadEnd`, `OnEndPayloads`)
 * - field envelope (`OnFieldBegin`, `OnFieldEnd`)
 * - structured containers (`OnArrayBegin/End`, `OnObjectBegin/End`)
 * - typed scalar values (`OnSignedInteger`, `OnUnsignedInteger`, `OnFloatingPoint`,
 *   `OnString`, `OnRawBytes`)
 *
 * Implementations are expected to treat callbacks as an ordered stream and keep
 * any formatting/aggregation state internally.
 */
class PayloadStreamVisitor
{
  public:
    virtual ~PayloadStreamVisitor() = default;

    /** Called once before iterating payload entries. */
    virtual void OnBeginPayloads(size_t count)
    {
        (void)count;
    }

    /** Called at start of one payload entry. `schemaName` is empty when unset. */
    virtual void OnPayloadBegin(
        size_t payloadIndex,
        uint64_t schemaId,
        size_t payloadSize,
        const std::string& schemaName) = 0;

    /** Called after all fields for the current payload are emitted. */
    virtual void OnPayloadEnd() = 0;

    /** Called before emitting a field's value stream. */
    virtual void OnFieldBegin(const std::string& name, const std::string& description) = 0;

    /** Called after a field value stream is complete. */
    virtual void OnFieldEnd() = 0;

    /** Called before emitting array elements. */
    virtual void OnArrayBegin(size_t length) = 0;

    /** Called after all array elements are emitted. */
    virtual void OnArrayEnd() = 0;

    /** Called before emitting nested object fields. */
    virtual void OnObjectBegin() = 0;

    /** Called after nested object fields are emitted. */
    virtual void OnObjectEnd() = 0;

    /** Emits a signed integer scalar value. */
    virtual void OnSignedInteger(int64_t value) = 0;

    /** Emits an unsigned integer scalar value. */
    virtual void OnUnsignedInteger(uint64_t value) = 0;

    /** Emits a floating-point scalar value. */
    virtual void OnFloatingPoint(double value) = 0;

    /** Emits a string scalar value. */
    virtual void OnString(const std::string& value) = 0;

    /** Emits raw bytes for unsupported or opaque values. */
    virtual void OnRawBytes(const uint8_t* data, size_t size) = 0;

    /** Called once after all payload entries have been processed. */
    virtual void OnEndPayloads() {}
};

/**
 * Decodes NVTX payload blobs according to registered schemas/enums and streams
 * typed events into a `PayloadStreamVisitor`.
 *
 * The class is lightweight and designed to be reused across calls.
 *
 * @par Unsupported entry flags
 * The following `NVTX_PAYLOAD_ENTRY_FLAG_*` flags are not yet handled:
 * - `POINTER` -- only honoured for C-string types. For all other types the
 *   stored value is read inline; a pointer indirection is not performed.
 * - `OFFSET_FROM_BASE` / `OFFSET_FROM_HERE` -- entries using these flags are
 *   parsed as if the value were inline data.
 * - `DEEP_COPY` -- irrelevant for read-only decode; noted for completeness.
 *
 * @par Unsupported semantic entry flags
 * The following flags are stored but do not influence decode or output:
 * `EVENT_MESSAGE`, `TIMESTAMP`, `RANGE_BEGIN`, `RANGE_END`, `MARK`,
 * `COUNTER`, `HIDE`.
 */
class NvtxPayloadParser
{
  public:
    explicit NvtxPayloadParser(const NvtxPayloadSchemaProcessor& schemaProcessor)
        : schemaProcessor_(schemaProcessor)
    {}

    /**
     * Streams decoded payload data to `visitor`.
     *
     * All schemas must be finalized before calling this method.
     *
     * @param payloadData Raw payload array from NVTX API callbacks.
     * @param count Number of payload elements in `payloadData`.
     * @param schemas Registered and finalized payload schemas keyed by schema ID.
     * @param enums Registered enum definitions keyed by schema/enum ID.
     * @param registeredStrings Registered NVTX string-handle lookup table.
     * @param visitor Destination for streaming decode events.
     */
    void ProcessPayloads(
        const nvtxPayloadData_t* payloadData,
        size_t count,
        const std::unordered_map<uint64_t, PayloadSchema>& schemas,
        const std::unordered_map<uint64_t, PayloadEnum>& enums,
        const std::unordered_map<nvtxStringHandle_t, std::string>& registeredStrings,
        PayloadStreamVisitor& visitor) const;

  private:
    /**
     * Per-field decode plan computed before value emission.
     *
     * Contains resolved base offset, element size, array shape, and whether a
     * zero-terminated array consumed a terminator element.
     */
    struct FieldDecodeInfo
    {
        /** Absolute byte offset of this field's first element within the raw payload buffer. */
        uint64_t fieldBaseOffset = 0;

        /** Size in bytes of one element; used as the iteration stride for arrays. */
        size_t elementSize = 0;

        /** Number of elements to decode (1 for scalars, N for arrays). */
        size_t arrayLength = 1;

        /** True when a zero-terminated array scan found the terminator element. */
        bool hasTerminator = false;

        /** True when the field represents an array (fixed-size, length-index, or zero-terminated).
         */
        bool isArray = false;

        /** Returns total bytes occupied by decoded elements (element count * stride). */
        size_t ConsumedBytes() const
        {
            return (isArray) ? (arrayLength * elementSize) : elementSize;
        }

        /** Total bytes including the trailing zero element if one was found.
         *  Only valid for array fields (zero-terminated layout). */
        size_t ConsumedBytesWithTerminator() const
        {
            return (arrayLength + static_cast<size_t>(hasTerminator)) * elementSize;
        }
    };

    /** Shared per-stream context: immutable decode tables plus mutable output visitor sink. */
    struct ParseContext
    {
        const std::unordered_map<uint64_t, PayloadSchema>& schemas;
        const std::unordered_map<uint64_t, PayloadEnum>& enums;
        const std::unordered_map<nvtxStringHandle_t, std::string>& registeredStrings;
        PayloadStreamVisitor& visitor;
    };

    /** Mutable parse state for one schema object while streaming fields. */
    struct ParseState
    {
        /** Dynamic schemas compute offsets while parsing payload bytes. */
        bool dynamicSchema = false;

        /** Fatal parse guard: when set, stop parsing remaining fields in this schema object. */
        bool abortParsing = false;

        /** Next implicit offset (schema-local) used when an entry has no explicit offset. */
        size_t nextDynamicOffset = 0;

        /** Captured non-negative integer values for entries referenced as ARRAY_LENGTH_INDEX. */
        std::unordered_map<size_t, uint64_t> lengthValues;
    };

    /** Computes decode metadata (absolute offset, element size, array length) for one schema field.
     *  @param packAlign    Schema pack alignment (like \c #pragma \c pack); 0 means natural
     * alignment.
     *  @param entry        Field descriptor (type, offset within schema, array layout, etc.).
     *  @param entryIndex   Zero-based index of @p entry within its schema (for length-index
     * lookup).
     *  @param payload      Raw payload buffer; only used for zero-terminated scanning and
     *                      array-length clamping against available bytes.
     *  @param baseOffset   Byte offset of the containing schema within the payload (non-zero for
     *                      nested schemas).
     *  @param schemas      Registered schema map (for nested-schema alignment lookup in dynamic
     * layouts).
     *  @param state        Mutable parse state tracking the dynamic cursor and captured lengths. */
    FieldDecodeInfo BuildFieldDecodeInfo(
        size_t packAlign,
        const PayloadSchemaEntry& entry,
        size_t entryIndex,
        const nvtxPayloadData_t& payload,
        uint64_t baseOffset,
        const std::unordered_map<uint64_t, PayloadSchema>& schemas,
        const ParseState& state) const;

    /** Finalizes one field emission: advances dynamic cursor and closes array/field events. */
    void FinishFieldEmission(
        uint64_t baseOffset,
        const FieldDecodeInfo& decode,
        size_t consumedBytes,
        const ParseContext& context,
        ParseState& state) const;

    /** Emits a nested schema object/array field recursively. */
    size_t EmitNestedSchemaField(
        const PayloadSchema& nestedSchema,
        const nvtxPayloadData_t& payload,
        const FieldDecodeInfo& decode,
        const ParseContext& context) const;

    /** Emits values for a field typed as a registered enum schema. */
    size_t EmitEnumField(
        const PayloadEnum& payloadEnum,
        const nvtxPayloadData_t& payload,
        const FieldDecodeInfo& decode,
        const ParseContext& context) const;

    /** Emits signed/unsigned integer fields and updates ARRAY_LENGTH_INDEX caches. */
    size_t EmitIntegerField(
        const PayloadSchemaEntry& entry,
        size_t entryIndex,
        const nvtxPayloadData_t& payload,
        const FieldDecodeInfo& decode,
        const ParseContext& context,
        ParseState& state) const;

    /** Emits floating-point field values (or raw bytes for unsupported float widths). */
    size_t EmitFloatingPointField(
        const nvtxPayloadData_t& payload,
        const FieldDecodeInfo& decode,
        const ParseContext& context) const;

    /** Emits C-string family field values. */
    size_t EmitCStringField(
        const PayloadSchemaEntry& entry,
        const nvtxPayloadData_t& payload,
        const FieldDecodeInfo& decode,
        const ParseContext& context) const;

    /** Emits `nvtxStringHandle_t` fields using the registered string lookup map. */
    size_t EmitRegisteredStringField(
        const PayloadSchemaEntry& entry,
        const nvtxPayloadData_t& payload,
        const FieldDecodeInfo& decode,
        const ParseContext& context) const;

    /** Emits payloads whose top-level `schemaId` is a predefined NVTX type. */
    void
    EmitPredefinedTypePayload(const nvtxPayloadData_t& payload, const ParseContext& context) const;

    /** Iterates fields of one schema object in declaration order and emits each field. */
    void VisitSchemaFields(
        const PayloadSchema& schema,
        const nvtxPayloadData_t& payload,
        uint64_t baseOffset,
        const ParseContext& context,
        ParseState& state) const;

    /** Top-level field dispatcher that routes to type-specific emit helpers. */
    void EmitField(
        const PayloadSchema& schema,
        size_t entryIndex,
        const nvtxPayloadData_t& payload,
        uint64_t baseOffset,
        const ParseContext& context,
        ParseState& state) const;

    const NvtxPayloadSchemaProcessor& schemaProcessor_;
};
