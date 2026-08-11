/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#if defined(NVTX_AS_SYSTEM_HEADER)
#if defined(__clang__)
#pragma clang system_header
#elif defined(__GNUC__) || defined(__NVCOMPILER)
#pragma GCC system_header
#elif defined(_MSC_VER)
#pragma system_header
#endif
#endif

/** \file nvToolsExtPayload.h
 * \brief NVTX payload extension API: schema types, entry flags, and registration.
 *
 * Extended payloads allow arbitrary structured data to be attached to NVTX mark
 * and range events. A registered schema describes how tools decode payload bytes.
 *
 * Workflow:
 * - Define payload layout and register the schema with
 *   @ref nvtxPayloadSchemaRegister.
 * - Build one or more @ref nvtxPayloadData_t entries for event data.
 * - Attach payload data to NVTX events via event attributes (for example
 *   using the helper macro @ref nvtxPayloadRangePush) or dedicated APIs such
 *   as @ref nvtxMarkPayload and @ref nvtxRangePushPayload.
 *
 * For detailed concepts, full workflow, and example usage, see
 * \ref NVTX_EXTENDED_PAYLOADS.
 */

#include "nvToolsExt.h"

/* Optionally include helper macros. */
/* #include "nvToolsExtPayloadHelper.h" */

/**
 * If needed, semantic extension headers can be included after this header.
 */

/**
 * \brief The compatibility ID is used for versioning of this extension.
 */
#ifndef NVTX_EXT_PAYLOAD_COMPATID
#define NVTX_EXT_PAYLOAD_COMPATID 0x0104
#endif

/**
 * \brief Unique module ID identifying the payload extension.
 */
#ifndef NVTX_EXT_PAYLOAD_MODULEID
#define NVTX_EXT_PAYLOAD_MODULEID 2
#endif

/**
 * \brief Additional value for the enum @ref nvtxPayloadType_t.
 */
#ifndef NVTX_PAYLOAD_TYPE_EXT
#define NVTX_PAYLOAD_TYPE_EXT (NVTX_STATIC_CAST(int32_t, 0xDFBD0009))
#endif

/**
 * Payload schema entry flags. Used for @ref nvtxPayloadSchemaEntry_t::flags.
 */
#ifndef NVTX_PAYLOAD_ENTRY_FLAGS_V1
#define NVTX_PAYLOAD_ENTRY_FLAGS_V1

#define NVTX_PAYLOAD_ENTRY_FLAG_UNUSED 0

/**
 * Absolute pointer into a payload (entry) of the same event.
 */
#define NVTX_PAYLOAD_ENTRY_FLAG_POINTER (1 << 1)

/**
 * Offset from base address of the payload.
 */
#define NVTX_PAYLOAD_ENTRY_FLAG_OFFSET_FROM_BASE (1 << 2)

/**
 * Offset from the end of this payload entry.
 */
#define NVTX_PAYLOAD_ENTRY_FLAG_OFFSET_FROM_HERE (1 << 3)

/**
 * The value is an array with fixed length set by `arrayOrUnionDetail`.
 */
#define NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE (1 << 4)

/**
 * A zero-terminated array. The terminator is an element whose bytes are all zero.
 */
#define NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED (2 << 4)

/**
 * \brief A single or multi-dimensional array of variable length.
 *
 * The field `arrayOrUnionDetail` contains the index of the schema entry that
 * holds the length(s). If the length entry is a scalar, then this entry is a 1D
 * array. If the length entry is a fixed-size array, then the number of
 * dimensions is defined with the registration of the schema. If the length
 * entry is a zero-terminated array, then the array of the dimensions can be
 * determined at runtime.
 * For multidimensional arrays, values are stored in row-major order, with rows
 * being stored consecutively in contiguous memory. The size of the entry (in
 * bytes) is the product of the dimensions multiplied by the size of the array
 * element.
 *
 * The referenced length entry must appear **before** this entry in the schema's
 * entries array and must be of an integer type. For signed integer length
 * entries, negative values are treated as zero (resulting in a zero-length array).
 */
#define NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX (3 << 4)

/**
 * \brief A single or multi-dimensional array of variable length, where the
 * dimensions are stored in a different payload (index) of the same event.
 *
 * `arrayOrUnionDetail` contains the zero-based **payload index** (into the
 * `nvtxPayloadData_t` array of the event) of a separate payload whose single
 * entry holds the array length(s). The referenced payload is decoded as an
 * integer (1D) or integer array (multi-dimensional, row-major).
 *
 * This enables an existing array to be passed as payload data, while the array
 * dimensions are defined in a separate payload with only one payload entry.
 */
#define NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_PAYLOAD_INDEX (4 << 4)

/**
 * \brief The value or data that is pointed to by this payload entry value shall
 * be copied by the NVTX handler.
 *
 * A tool that does not support deep copy may retain only the address value; in
 * that case, the referenced data is unavailable for interpretation.
 * See @ref NVTX_PAYLOAD_SCHEMA_FLAG_DEEP_COPY for more details.
 */
#define NVTX_PAYLOAD_ENTRY_FLAG_DEEP_COPY (1 << 8)

/**
 * Notifies the NVTX handler to hide this entry in case of visualization.
 */
#define NVTX_PAYLOAD_ENTRY_FLAG_HIDE (1 << 9)

/**
 * The entry specifies the event message. Any string type can be used.
 *
 * If multiple messages are specified for a logical event, the effective message
 * is selected according to @ref NVTX_PAYLOAD_EVENT_ATTRIBUTE_PRECEDENCE.
 */
#define NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE (1 << 10)

/**
 * \brief The entry contains a timestamp.
 *
 * The time source might be provided via the entry semantics field. In most
 * cases, the timestamp (entry) type is @ref NVTX_PAYLOAD_ENTRY_TYPE_INT64.
 */
#define NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP (2 << 10)

/**
 * \brief Flags that assign an event-type role to an entry.
 *
 * These flags let a tool identify which entries carry special event semantics
 * (e.g. timestamps for range begin/end, counter values). They work in
 * conjunction with the schema-level flags `NVTX_PAYLOAD_SCHEMA_FLAG_*`:
 *
 * - `NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSHPOP` or `_RANGE_STARTEND`: the schema
 *   represents a range. Use the entry flags `RANGE_BEGIN` and `RANGE_END` on
 *   timestamp entries to mark start and end time.
 * - `NVTX_PAYLOAD_SCHEMA_FLAG_MARK`: the schema represents an instantaneous
 *   marker. Use `NVTX_PAYLOAD_ENTRY_FLAG_MARK` on a timestamp entry.
 * - `NVTX_PAYLOAD_SCHEMA_FLAG_COUNTER_GROUP`: the schema represents a group
 *   of counters. Use `NVTX_PAYLOAD_ENTRY_FLAG_COUNTER` on each value entry
 *   that is a counter. Counter semantics (normalization, limits, interpolation)
 *   can be further described via the entry's
 *   @ref nvtxPayloadSchemaEntry_t::semantics field. For counter registration
 *   and sampling, use `nvtx3/nvToolsExtCounters.h`.
 *
 * For ranges and marks, use `NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE` on a
 * string entry to provide the event's display name.
 */
#define NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN (1 << 12)
/** \brief Marks a timestamp entry as the end of a range. */
#define NVTX_PAYLOAD_ENTRY_FLAG_RANGE_END (2 << 12)
/** \brief Marks a timestamp entry as an instantaneous marker. */
#define NVTX_PAYLOAD_ENTRY_FLAG_MARK (3 << 12)
/** \brief Marks a payload entry as a counter value. */
#define NVTX_PAYLOAD_ENTRY_FLAG_COUNTER (4 << 12)

/**
 * @note The 'array' flags assume that the array is embedded. Otherwise,
 * @ref NVTX_PAYLOAD_ENTRY_FLAG_POINTER must also be specified. Some
 * combinations may be invalid based on the `NVTX_PAYLOAD_SCHEMA_TYPE_*` this
 * entry is enclosed. For instance, variable length embedded arrays are valid
 * within @ref NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC but invalid with
 * @ref NVTX_PAYLOAD_SCHEMA_TYPE_STATIC. See `NVTX_PAYLOAD_SCHEMA_TYPE_*` for
 * additional details.
 */

/* Helper macro to check if an entry represents an array. */
#define NVTX_PAYLOAD_ENTRY_FLAG_IS_ARRAY                                                           \
    (NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE | NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED |    \
     NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX |                                                  \
     NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_PAYLOAD_INDEX)

#define NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_TYPE(F) ((F) & NVTX_PAYLOAD_ENTRY_FLAG_IS_ARRAY)

#endif /* NVTX_PAYLOAD_ENTRY_FLAGS_V1 */
/** ---------------------------------------------------------------------------
 * END: Payload schema entry flags.
 * ------------------------------------------------------------------------- */

/**
 * \anchor NVTX_PAYLOAD_EVENT_ATTRIBUTE_PRECEDENCE
 * \par Event attribute precedence
 *
 * If the same event attribute is specified more than once for a logical event,
 * the latest-specified value is the effective value. Tools may preserve
 * superseded values, but applications should not rely on them being available.
 *
 * Ordering is: regular @ref nvtxEventAttributes_v2 "nvtxEventAttributes_t"
 * attributes first, then @ref nvtxPayloadData_t entries in array order, then
 * schema entries in order.
 *
 * For ranges, end/pop attributes are ordered later than start/push attributes.
 * Tools that act before range completion can only use attributes known at that
 * time. Runtime filtering on extended-payload attributes is optional for tools;
 * tools may skip it to avoid decoding overhead.
 *
 * \anchor NVTX_PAYLOAD_EVENT_MESSAGE_REQUIREMENT
 * Payload APIs that emit a mark, begin a range, or submit a deferred event
 * supply the event message with @ref NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE. If
 * the message is missing, a tool may ignore the event. Range pop/end payloads
 * may omit a message unless they intentionally override the range message.
 * Keep range messages stable for filtering; use color or payload fields for
 * state changes.
 */

/**
 * \brief Types of entries in a payload schema.
 *
 * @note Some predefined types have platform-dependent sizes. See
 * @ref nvtxPayloadEntryTypeInfo_t for the portability mechanism.
 */
#ifndef NVTX_PAYLOAD_ENTRY_TYPES_V1
#define NVTX_PAYLOAD_ENTRY_TYPES_V1

#define NVTX_PAYLOAD_ENTRY_TYPE_INVALID 0

/**
 * Basic integer types.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_CHAR 1
#define NVTX_PAYLOAD_ENTRY_TYPE_UCHAR 2
#define NVTX_PAYLOAD_ENTRY_TYPE_SHORT 3
#define NVTX_PAYLOAD_ENTRY_TYPE_USHORT 4
#define NVTX_PAYLOAD_ENTRY_TYPE_INT 5
#define NVTX_PAYLOAD_ENTRY_TYPE_UINT 6
#define NVTX_PAYLOAD_ENTRY_TYPE_LONG 7
#define NVTX_PAYLOAD_ENTRY_TYPE_ULONG 8
#define NVTX_PAYLOAD_ENTRY_TYPE_LONGLONG 9
#define NVTX_PAYLOAD_ENTRY_TYPE_ULONGLONG 10

/**
 * Integer types with explicit size.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_INT8 11
#define NVTX_PAYLOAD_ENTRY_TYPE_UINT8 12
#define NVTX_PAYLOAD_ENTRY_TYPE_INT16 13
#define NVTX_PAYLOAD_ENTRY_TYPE_UINT16 14
#define NVTX_PAYLOAD_ENTRY_TYPE_INT32 15
#define NVTX_PAYLOAD_ENTRY_TYPE_UINT32 16
/** \brief 64-bit signed integer payload entry type. */
#define NVTX_PAYLOAD_ENTRY_TYPE_INT64 17
#define NVTX_PAYLOAD_ENTRY_TYPE_UINT64 18

/**
 * Floating point types
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_FLOAT 19
#define NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE 20
#define NVTX_PAYLOAD_ENTRY_TYPE_LONGDOUBLE 21

/**
 * Size type (`size_t` in C).
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_SIZE 22

/**
 * Any address, e.g. `void*`. If the pointee type matters, use
 * @ref NVTX_PAYLOAD_ENTRY_FLAG_POINTER with the pointee type instead.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_ADDRESS 23

/**
 * Special character types.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_WCHAR 24 /* wide character (since C90) */
#define NVTX_PAYLOAD_ENTRY_TYPE_CHAR8 25 /* since C2x and C++20 */
#define NVTX_PAYLOAD_ENTRY_TYPE_CHAR16 26
#define NVTX_PAYLOAD_ENTRY_TYPE_CHAR32 27

/**
 * There is type size and alignment information for all previous types.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_INFO_ARRAY_SIZE (NVTX_PAYLOAD_ENTRY_TYPE_CHAR32 + 1)

/**
 * Store raw 8-bit binary data. As with `char`, 1-byte alignment is assumed.
 * Typically, a tool will display this as hex or binary.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_BYTE 32

/**
 * These types do not have standardized equivalents. It is assumed that the
 * number at the end corresponds to the bits used to store the value and that
 * the alignment corresponds to standardized types of the same size.
 * A tool may not support these types.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_INT128 33
#define NVTX_PAYLOAD_ENTRY_TYPE_UINT128 34

/**
 * IEEE 754 floating-point types with explicit size. The number at the end
 * corresponds to the storage width in bits. The alignment is assumed to match
 * standardized types of the same size.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_FLOAT16 42
#define NVTX_PAYLOAD_ENTRY_TYPE_FLOAT32 43
#define NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64 44
#define NVTX_PAYLOAD_ENTRY_TYPE_FLOAT128 45

#define NVTX_PAYLOAD_ENTRY_TYPE_BF16 50 /* bfloat16 (16-bit) */
#define NVTX_PAYLOAD_ENTRY_TYPE_TF32 52 /* TensorFloat-32 (stored in 32 bits) */

/**
 * Data types are as defined by NVTXv3 core.
 *
 * Entries of these types are interpreted as event attributes.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_RANGE_ID 67 /* nvtxRangeId_t / uint64_t */
#define NVTX_PAYLOAD_ENTRY_TYPE_CATEGORY 68 /* uint32_t */
#define NVTX_PAYLOAD_ENTRY_TYPE_COLOR_ARGB 69 /* uint32_t */

/**
 * The scope of events or counters (see `nvtxScopeRegister`).
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_SCOPE_ID 70 /* uint64_t */

/**
 * Process ID as scope.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_PID_UINT32 71
#define NVTX_PAYLOAD_ENTRY_TYPE_PID_UINT64 72

/**
 * Thread ID as scope.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_TID_UINT32 73
#define NVTX_PAYLOAD_ENTRY_TYPE_TID_UINT64 74

/**
 * \brief String types.
 *
 * String entries hold inline character data or a pointer. With no array flags,
 * `arrayOrUnionDetail` is a fixed length in string code units; setting
 * @ref NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE is redundant (still a single
 * fixed-length string, not an array of strings). With
 * @ref NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX, `arrayOrUnionDetail` is the
 * index of a length-source entry whose value is the length in string code units,
 * where 0 denotes an empty string. Zero-terminated strings use
 * @ref NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED. A string code unit is 1 byte
 * for `CSTRING`/`CSTRING_UTF8`, 2 bytes for `CSTRING_UTF16`, and 4 bytes for
 * `CSTRING_UTF32`. Despite the `CSTRING` name, strings with an explicit length
 * need not be null-terminated.
 *
 * A fixed-length string always occupies (for inline data) or is read (for
 * pointer and deep-copy forms) as exactly the declared number of code units.
 * Its value is the code units up to, but not including, the first null
 * terminator; if no null terminator occurs within the declared length, the
 * value is all of the declared code units. Code units after the first null
 * terminator are ignored.
 *
 * Pointer strings normally reference data in another payload of the same event.
 * With @ref NVTX_PAYLOAD_ENTRY_FLAG_DEEP_COPY, they may reference arbitrary
 * memory that the tool should copy.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_CSTRING 75 /* `char*`, system LOCALE */
#define NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF8 76
#define NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF16 77
#define NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32 78

/**
 * The entry value is of type
 * @ref REGISTERED_STRING_HANDLE_STRUCTURE "nvtxStringHandle_t" returned by
 * @ref nvtxDomainRegisterStringA or @ref nvtxDomainRegisterStringW.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE 80

/**
 * This type marks the union selector member (entry index) in schemas used by
 * a union with internal selector.
 * See @ref NVTX_PAYLOAD_SCHEMA_TYPE_UNION_WITH_INTERNAL_SELECTOR.
 */
#define NVTX_PAYLOAD_ENTRY_TYPE_UNION_SELECTOR 100

/**
 * \brief Predefined value for payload data that is referenced in another payload.
 *
 * This value can be used in @ref nvtxPayloadData_t::schemaId to indicate that the
 * payload is a blob of memory which other payload entries may point into.
 * A tool will not expose this payload directly.
 *
 * This value cannot be used as a schema entry type.
 */
#define NVTX_TYPE_PAYLOAD_SCHEMA_REFERENCED 1022

/**
 * \brief Predefined value for raw payload data.
 *
 * This value can be used in @ref nvtxPayloadData_t::schemaId to indicate
 * that the payload is a blob, which can be shown with an arbitrary data viewer.
 * This value cannot be used as a schema entry type.
 */
#define NVTX_TYPE_PAYLOAD_SCHEMA_RAW 1023

/* Custom (static) schema IDs. */
/** \brief First valid user-defined static schema ID. */
#define NVTX_PAYLOAD_SCHEMA_ID_STATIC_START (1 << 24)

/* Dynamic schema IDs (generated by the tool) start here. */
#define NVTX_PAYLOAD_SCHEMA_ID_DYNAMIC_START (NVTX_STATIC_CAST(uint64_t, 1) << 32)

#endif /* NVTX_PAYLOAD_ENTRY_TYPES_V1 */
/** ---------------------------------------------------------------------------
 * END: Payload schema entry types.
 * ------------------------------------------------------------------------- */

#ifndef NVTX_PAYLOAD_SCHEMA_TYPES_V1
#define NVTX_PAYLOAD_SCHEMA_TYPES_V1

/**
 * \brief The payload schema type.
 *
 * A schema can be either of the following types. It is set with
 * @ref nvtxPayloadSchemaAttr_t::type.
 *
 * **Static schemas** (`NVTX_PAYLOAD_SCHEMA_TYPE_STATIC`) describe C-like
 * structs with a fixed binary size. All entry offsets and sizes must be
 * deterministic at compile time. Variable-length fields are not allowed.
 *
 * **Dynamic schemas** (`NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC`) allow
 * variable-length fields. A tool parses fields sequentially, advancing a
 * running cursor with proper alignment after each field. Entries with an
 * explicit non-zero @ref nvtxPayloadSchemaEntry_t::offset are placed at that
 * offset; otherwise the offset is computed from the cursor. Entries that rely
 * on implicit offsets must be declared in memory order. Arrays of nested
 * dynamic schemas are not supported (each nested dynamic-schema entry must be
 * a scalar). `payloadStaticSize` may be omitted for dynamic schemas.
 *
 * **Union schemas** (`NVTX_PAYLOAD_SCHEMA_TYPE_UNION` and
 * `NVTX_PAYLOAD_SCHEMA_TYPE_UNION_WITH_INTERNAL_SELECTOR`) describe C-like
 * unions. The selected member is determined by an external or internal
 * selector entry of integral type.
 */
#define NVTX_PAYLOAD_SCHEMA_TYPE_INVALID 0
/** \brief Fixed-size C-like struct schema type. */
#define NVTX_PAYLOAD_SCHEMA_TYPE_STATIC 1
/** \brief Variable-length payload schema type. */
#define NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC 2
/** \brief C-like union schema with an external selector entry. */
#define NVTX_PAYLOAD_SCHEMA_TYPE_UNION 3
/** \brief C-like union schema with an internal selector entry. */
#define NVTX_PAYLOAD_SCHEMA_TYPE_UNION_WITH_INTERNAL_SELECTOR 4

#endif /* NVTX_PAYLOAD_SCHEMA_TYPES_V1 */

#ifndef NVTX_PAYLOAD_SCHEMA_FLAGS_V1
#define NVTX_PAYLOAD_SCHEMA_FLAGS_V1

/**
 * \brief Flags for static and dynamic schemas.
 *
 * The schema flags are used with @ref nvtxPayloadSchemaAttr_t::flags.
 */
#define NVTX_PAYLOAD_SCHEMA_FLAG_NONE 0

/**
 * This flag indicates that a schema and the corresponding payloads can
 * contain fields which require a deep copy.
 */
#define NVTX_PAYLOAD_SCHEMA_FLAG_DEEP_COPY (1 << 1)

/**
 * This flag indicates that a schema and the corresponding payload can be
 * referenced by another payload of the same event. If the schema is not
 * intended to be visualized directly, use
 * @ref NVTX_TYPE_PAYLOAD_SCHEMA_REFERENCED instead.
 */
#define NVTX_PAYLOAD_SCHEMA_FLAG_REFERENCED (1 << 2)

/**
 * The schema defines a counter group. An NVTX handler can expect that the schema
 * contains entries with counter semantics. For counter registration and sampling,
 * use `nvtx3/nvToolsExtCounters.h`.
 */
#define NVTX_PAYLOAD_SCHEMA_FLAG_COUNTER_GROUP (1 << 3)

/**
 * The schema defines a range or marker. An NVTX handler can expect timestamp
 * entries and an optional message entry with event semantics.
 */
#define NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSHPOP (2 << 3)
/** \brief Schema represents a start/end range event. */
#define NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_STARTEND (3 << 3)
/** \brief Schema represents an instantaneous marker event. */
#define NVTX_PAYLOAD_SCHEMA_FLAG_MARK (4 << 3)
#define NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSH (5 << 3)
#define NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_POP (6 << 3)
#define NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_START (7 << 3)
#define NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_END (8 << 3)

#endif /* NVTX_PAYLOAD_SCHEMA_FLAGS_V1 */

#ifndef NVTX_PAYLOAD_SCHEMA_ATTR_FIELDS_V1
#define NVTX_PAYLOAD_SCHEMA_ATTR_FIELDS_V1

/**
 * \brief Bitmask values for @ref nvtxPayloadSchemaAttr_t::fieldMask.
 *
 * Each bit indicates that the corresponding field in @ref nvtxPayloadSchemaAttr_t
 * has been set by the caller. A tool must not read fields whose bit is not set.
 * `TYPE`, `ENTRIES`, and `NUM_ENTRIES` must be set for successful registration.
 */
#define NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME (1 << 1)
#define NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE (1 << 2)
#define NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_FLAGS (1 << 3)
#define NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES (1 << 4)
#define NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES (1 << 5)
#define NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE (1 << 6)
#define NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ALIGNMENT (1 << 7)
#define NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_SCHEMA_ID (1 << 8)
#define NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_EXTENSION (1 << 9)

#endif /* NVTX_PAYLOAD_SCHEMA_ATTR_FIELDS_V1 */

#ifndef NVTX_PAYLOAD_ENUM_ATTR_FIELDS_V1
#define NVTX_PAYLOAD_ENUM_ATTR_FIELDS_V1

/**
 * \brief Bitmask values for @ref nvtxPayloadEnumAttr_t::fieldMask.
 *
 * Each bit indicates that the corresponding field in @ref nvtxPayloadEnumAttr_t
 * has been set by the caller.
 * `ENTRIES`, `NUM_ENTRIES`, and `SIZE` must be set for successful registration.
 */
#define NVTX_PAYLOAD_ENUM_ATTR_FIELD_NAME (1 << 1)
#define NVTX_PAYLOAD_ENUM_ATTR_FIELD_ENTRIES (1 << 2)
#define NVTX_PAYLOAD_ENUM_ATTR_FIELD_NUM_ENTRIES (1 << 3)
#define NVTX_PAYLOAD_ENUM_ATTR_FIELD_SIZE (1 << 4)
#define NVTX_PAYLOAD_ENUM_ATTR_FIELD_SCHEMA_ID (1 << 5)
#define NVTX_PAYLOAD_ENUM_ATTR_FIELD_EXTENSION (1 << 6)

#endif /* NVTX_PAYLOAD_ENUM_ATTR_FIELDS_V1 */

/**
 * \anchor NVTX_SCOPE_SPECIFICATION_AND_PRECEDENCE
 * \par Scope specification and precedence
 *
 * An NVTX scope describes where an event or counter originated, or the
 * execution context it belongs to. Predefined scopes identify common execution
 * contexts; custom scopes can be registered with \ref nvtxScopeRegister.
 *
 * The `NVTX_SCOPE_CURRENT_*` values are runtime-resolved scope references: a
 * tool resolves them against the live execution context of the instrumented
 * code when an event or counter sample is taken.
 *
 * Scopes can be specified by payload entries, scope semantics
 * (\ref nvtxSemanticsScope_t), deferred-event batch attributes, or counter
 * registration attributes. If more than one scope applies to the same event
 * role, counter role, or timestamp purpose, tools should select the effective
 * scope in this order:
 *
 * <ol>
 *   <li>Purpose-specific \ref NVTX_PAYLOAD_ENTRY_TYPE_SCOPE_ID entry with a
 *       role or timestamp flag (for example
 *       \ref NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN,
 *       \ref NVTX_PAYLOAD_ENTRY_FLAG_RANGE_END,
 *       \ref NVTX_PAYLOAD_ENTRY_FLAG_MARK,
 *       \ref NVTX_PAYLOAD_ENTRY_FLAG_COUNTER, or
 *       \ref NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP).
 *   <li>Purpose-specific scope semantics attached to a payload entry with a
 *       role or timestamp flag.
 *   <li>General \ref NVTX_PAYLOAD_ENTRY_TYPE_SCOPE_ID entry without a role or
 *       timestamp flag.
 *   <li>\ref nvtxEventBatch_t::scope.
 *   <li>General scope semantics attached to an arbitrary payload entry.
 *   <li>Counter registration scope (for example
 *       \c nvtxCounterAttr_t::scopeId; see \ref nvToolsExtCounters.h "nvtxCounterRegister").
 * </ol>
 *
 * \ref NVTX_SCOPE_NONE means no scope is specified. Scopes for different
 * purposes are independent and may be different.
 */
#ifndef NVTX_SCOPES_V1
#define NVTX_SCOPES_V1

/** \brief No scope is specified. */
#define NVTX_SCOPE_NONE 0 /* No scope specified. */
#define NVTX_SCOPE_ROOT 1 /* The root in a hierarchy. */

/* Hardware events */
#define NVTX_SCOPE_CURRENT_HW_MACHINE 2 /* Node/machine name */
#define NVTX_SCOPE_CURRENT_HW_SOCKET 3
#define NVTX_SCOPE_CURRENT_HW_CPU_PHYSICAL 4 /* Physical CPU core */
#define NVTX_SCOPE_CURRENT_HW_CPU_LOGICAL 5  /* Logical CPU core */
/* Innermost HW execution context */
#define NVTX_SCOPE_CURRENT_HW_INNERMOST 15

/* Virtualized hardware, virtual machines */
#define NVTX_SCOPE_CURRENT_HYPERVISOR 16
#define NVTX_SCOPE_CURRENT_VM 17
#define NVTX_SCOPE_CURRENT_KERNEL 18
#define NVTX_SCOPE_CURRENT_CONTAINER 19
#define NVTX_SCOPE_CURRENT_OS 20

/* Software scopes */
#define NVTX_SCOPE_CURRENT_SW_PROCESS 21 /* Process scope */
#define NVTX_SCOPE_CURRENT_SW_THREAD 22  /* Thread scope */
/* Innermost SW execution context */
#define NVTX_SCOPE_CURRENT_SW_INNERMOST 31

/** Static (user-provided) scope IDs. */
#define NVTX_SCOPE_ID_STATIC_START (1 << 24)

/* Dynamically (tool) generated scope IDs */
#define NVTX_SCOPE_ID_DYNAMIC_START (NVTX_STATIC_CAST(uint64_t, 1) << 32)

#endif /* NVTX_SCOPES_V1 */

#ifndef NVTX_TIME_V1
#define NVTX_TIME_V1

/**
 * Predefined `NVTX_TIMESTAMP_TYPE_*` values identify well-known timestamp
 * sources. Where an API accepts a time domain ID, a predefined timestamp type
 * may be used directly as the time domain ID if the source is unambiguous.
 */

/**
 * Timestamp source is not known, e.g. NIC or switch. The NVTX handler can
 * assume that at least two synchronization points are created with NVTX
 * instrumentation.
 */
#define NVTX_TIMESTAMP_TYPE_NONE 0

/** The timestamp was provided by the NVTX handler via `nvtxTimestampGet()`. */
#define NVTX_TIMESTAMP_TYPE_TOOL_PROVIDED 1

/** CPU timestamp sources */
/* RDTSC on x86, CNTVCT on ARM */
#define NVTX_TIMESTAMP_TYPE_CPU_TSC 10
/* CNTPCT on ARM */
#define NVTX_TIMESTAMP_TYPE_CPU_TSC_NONVIRTUALIZED 11
/* Nanoseconds since epoch (relative to UTC), clock_gettime(CLOCK_REALTIME) */
#define NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_REALTIME 12
/* Same as above but less overhead and precision (1-10 ms) */
#define NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_REALTIME_COARSE 13
/* POSIX, Time since system boot, adjusted by NTP */
#define NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC 14
/* Linux only, Time since system boot, no NTP or frequency corrections  */
#define NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC_RAW 15
/* Same as `CLOCK_MONOTONIC`, but less overhead and precision (1-10ms) */
#define NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC_COARSE 16
/* Same as `CLOCK_MONOTONIC`, but including suspended time. */
#define NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_BOOTTIME 17
/* The total CPU time consumed by the calling process. */
#define NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_PROCESS_CPUTIME_ID 18
/* The total CPU time consumed by the calling thread. */
#define NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_THREAD_CPUTIME_ID 19

/** Windows timestamp sources */
#define NVTX_TIMESTAMP_TYPE_WIN_QPC 30     /* QueryPerformanceCounter */
#define NVTX_TIMESTAMP_TYPE_WIN_GSTAFT 31  /* GetSystemTimeAsFileTime */
#define NVTX_TIMESTAMP_TYPE_WIN_GSTAFTP 32 /* GetSystemTimePreciseAsFileTime */

/** C timestamp sources */
/* Seconds since epoch (represented in C as `time_t`) */
#define NVTX_TIMESTAMP_TYPE_C_TIME 40
/* CPU clock value (represented in C as `clock_t`) as returned by `clock()` */
#define NVTX_TIMESTAMP_TYPE_C_CLOCK 41
/* High-resolution time into struct timespec (C11) */
#define NVTX_TIMESTAMP_TYPE_C_TIMESPEC_GET 42

/** C++ timestamp sources */
/* std::chrono::steady_clock (monotonic clock), similar to `CLOCK_MONOTONIC` */
#define NVTX_TIMESTAMP_TYPE_CPP_STEADY_CLOCK 50
/* std::chrono::high_resolution_clock, similar to `CLOCK_MONOTONIC` or `CLOCK_MONOTONIC_RAW` */
#define NVTX_TIMESTAMP_TYPE_CPP_HIGH_RESOLUTION_CLOCK 51
/* std::chrono::system_clock, similar to `CLOCK_REALTIME` */
#define NVTX_TIMESTAMP_TYPE_CPP_SYSTEM_CLOCK 52
/* (since C++20) std::chrono::utc_clock, similar to `CPP_SYSTEM_CLOCK` */
#define NVTX_TIMESTAMP_TYPE_CPP_UTC_CLOCK 53
/* (since C++20) std::chrono::tai_clock */
#define NVTX_TIMESTAMP_TYPE_CPP_TAI_CLOCK 54
/* (since C++20) std::chrono::gps_clock */
#define NVTX_TIMESTAMP_TYPE_CPP_GPS_CLOCK 55
/* (since C++20) std::chrono::file_clock */
#define NVTX_TIMESTAMP_TYPE_CPP_FILE_CLOCK 56

/** GPU timestamp sources */
#define NVTX_TIMESTAMP_TYPE_GPU_GLOBALTIMER 80 /* e.g. PTIMER */

/** Returned by `nvtxTimeDomainRegister` if time domain registration failed. */
#define NVTX_TIME_DOMAIN_ID_NONE 0

/** Static (user-provided) time domain IDs. */
#define NVTX_TIME_DOMAIN_ID_STATIC_START (1 << 24)

/* Dynamically (tool) generated time domain IDs */
#define NVTX_TIME_DOMAIN_ID_DYNAMIC_START (NVTX_STATIC_CAST(uint64_t, 1) << 32)

/** Timer properties */
#define NVTX_TIMER_FLAG_NONE 0
#define NVTX_TIMER_FLAG_CLOCK_MONOTONIC (1 << 1)
#define NVTX_TIMER_FLAG_CLOCK_STEADY (1 << 2)

/** Point in time when the timer starts (its value is 0). */
#define NVTX_TIMER_START_UNKNOWN 0
#define NVTX_TIMER_START_SYSTEM_BOOT 1
#define NVTX_TIMER_START_VM_BOOT 2
#define NVTX_TIMER_START_UNIX_EPOCH 3   /* 1 January 1970 */
#define NVTX_TIMER_START_WIN_FILETIME 4 /* 1 January 1601 */

/**
 * Flags specifying whether it is safe or unsafe to call the timestamp
 * provider after process teardown.
 */
#define NVTX_TIMER_SOURCE_SAFE_CALL_AFTER_PROCESS_TEARDOWN 0
#define NVTX_TIMER_SOURCE_UNSAFE_CALL_AFTER_PROCESS_TEARDOWN 1

#endif /* NVTX_TIME_V1 */

#ifndef NVTX_BATCH_FLAGS_V1
#define NVTX_BATCH_FLAGS_V1

/**
 * Timestamp ordering flags for a batch of deferred events or counters.
 * By default, chronological order by the first timestamp of the event or
 * counter is assumed.
 */
#define NVTX_BATCH_FLAG_TIME_SORTED 0
#define NVTX_BATCH_FLAG_TIME_SORTED_PARTIALLY (1 << 1)
#define NVTX_BATCH_FLAG_TIME_SORTED_PER_SCOPE (2 << 1)
#define NVTX_BATCH_FLAG_UNSORTED (3 << 1)

#endif /* NVTX_BATCH_FLAGS_V1 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef NVTX_PAYLOAD_TYPEDEFS_V1
#define NVTX_PAYLOAD_TYPEDEFS_V1

/**
 * \brief Size and alignment information for predefined payload entry types.
 *
 * The struct contains the size and alignment in bytes. An array for the
 * predefined types is passed via nvtxExtModuleInfo_t to the NVTX client/handler.
 * The entry type value is used as the index into this array.
 *
 * Providing this array is important for cross-platform portability. Types such
 * as `NVTX_PAYLOAD_ENTRY_TYPE_INT`, `_LONG`, `_SIZE`, `_ADDRESS`, and
 * `_LONGDOUBLE` have platform-dependent sizes. Without this information, a
 * tool falls back to using its own platform's `sizeof()`, which may differ
 * from the producer's platform (e.g. 32-bit vs 64-bit, or different
 * compilers with varying `long` / `long double` sizes). The array must have
 * at least @ref NVTX_PAYLOAD_ENTRY_TYPE_INFO_ARRAY_SIZE entries.
 */
typedef struct nvtxPayloadEntryTypeInfo_v1
{
    uint16_t size;
    uint16_t align;
} nvtxPayloadEntryTypeInfo_t;

/**
 * \brief Binary payload data, size and decoding information.
 *
 * An array of `nvtxPayloadData_t` can be passed directly to the payload event
 * APIs (`nvtxMarkPayload`, `nvtxRangePushPayload`, `nvtxRangePopPayload`,
 * `nvtxRangeStartPayload`, `nvtxRangeEndPayload`, `nvtxEventSubmit`), or
 * attached to @ref nvtxEventAttributes_v2 "nvtxEventAttributes_t" via its `payload.ullValue`
 * field. The helper macros @ref nvtxPayloadMark, @ref nvtxPayloadRangePush,
 * and @ref NVTX_PAYLOAD_EVTATTR_SET_MULTIPLE simplify the latter approach.
 *
 * Payload array order and schema-entry order define the ordering of event
 * attributes supplied by extended payloads; see
 * @ref NVTX_PAYLOAD_EVENT_ATTRIBUTE_PRECEDENCE.
 */
typedef struct nvtxPayloadData_v1
{
    /**
     * \brief The schema ID, which defines the layout of the binary data.
     *
     * The value can be one of:
     * - A **predefined entry type** (`NVTX_PAYLOAD_ENTRY_TYPE_*`, i.e. a value
     *   below @ref NVTX_PAYLOAD_SCHEMA_ID_STATIC_START). The payload bytes are
     *   decoded directly as that type without requiring a registered schema.
     *   For C-string types the bytes contain inline character data (not a
     *   pointer). For numeric types, the element count is
     *   `size / elementSize`; any trailing bytes are ignored.
     * - A **registered schema ID** (statically specified or dynamically
     *   created), >= @ref NVTX_PAYLOAD_SCHEMA_ID_STATIC_START.
     * - One of @ref NVTX_TYPE_PAYLOAD_SCHEMA_REFERENCED or
     *   @ref NVTX_TYPE_PAYLOAD_SCHEMA_RAW.
     */
    uint64_t schemaId;

    /**
     * \brief Size of the payload (blob) in bytes.
     *
     * Must be non-zero; a payload with `size == 0` is ignored.
     *
     * `SIZE_MAX` (`(size_t)-1`) defers size determination to the tool. This is
     * only reliably supported when @ref schemaId is a predefined
     * null-terminated C-string type, where the tool can call the appropriate
     * string-length function. For all other schema IDs a tool may not be able
     * to determine the size and will skip the payload.
     *
     * When `schemaId` is a predefined C-string type and `size != SIZE_MAX`,
     * `size` bounds the inline string data in bytes and the payload does not
     * need to include a null terminator. `size` must be at least one code unit.
     * An empty string is encoded as a single null code unit.
     */
    size_t size;

    /**
     * Pointer to the binary payload data. Must not be NULL.
     */
    const void* payload;
} nvtxPayloadData_t;

/**
 * \brief Header of the payload entry's semantic field.
 *
 * Semantic extension structs begin with this header and are linked through
 * @ref next.
 */
typedef struct nvtxSemanticsHeader_v1
{
    uint32_t structSize;                       /** Size of semantic extension struct. */
    uint16_t semanticId;                       /** Identifies the semantic extension. */
    uint16_t version;                          /** Version of the specific semantic extension. */
    const struct nvtxSemanticsHeader_v1* next; /** Next semantic extension. */
    /* Additional fields are defined by the specific semantic extension. */
} nvtxSemanticsHeader_t;

/**
 * \brief Entry in a schema.
 *
 * Payload schemas are arrays of entries registered with
 * @ref nvtxPayloadSchemaRegister. For simple values, set `flags` to `0`;
 * `type` is the only required field. Zero-initialized optional fields mean no
 * name and implicit offset calculation.
 *
 * Example schema:
 *  nvtxPayloadSchemaEntry_t schema[] = {
 *      {0, NVTX_PAYLOAD_ENTRY_TYPE_UINT8, "one byte"},
 *      {0, NVTX_PAYLOAD_ENTRY_TYPE_INT32, "four bytes"}
 *  };
 */
typedef struct nvtxPayloadSchemaEntry_v1
{
    /**
     * \brief Flags to augment the basic type.
     *
     * This field allows additional properties of the payload entry to be
     * specified. Valid values are `NVTX_PAYLOAD_ENTRY_FLAG_*`.
     */
    uint64_t flags;

    /**
     * \brief Predefined payload schema entry type, registered enum ID, or
     * registered schema ID.
     *
     * The value can be:
     * - A **predefined type** (`NVTX_PAYLOAD_ENTRY_TYPE_*`).
     * - A **registered schema ID** (>= @ref NVTX_PAYLOAD_SCHEMA_ID_STATIC_START),
     *   nesting the referenced schema inline. Size and alignment are taken
     *   from the referenced schema.
     * - A **registered enum ID** (>= @ref NVTX_PAYLOAD_SCHEMA_ID_STATIC_START).
     *   Byte width is given by @ref nvtxPayloadEnumAttr_t::sizeOfEnum. A tool
     *   resolves the integer value to the enum entry name.
     */
    uint64_t type;

    /**
     * \brief Name or label of the payload entry. (Optional)
     *
     * A meaningful name or label helps tools organize and interpret the data.
     */
    const char* name;

    /**
     * \brief Description of the payload entry. (Optional)
     *
     * A more detailed description of the data stored with this entry.
     */
    const char* description;

    /**
     * \brief String length, array length or member selector for union types.
     *
     * If @ref type is a C string type and the entry represents an embedded
     * fixed-size string, this field specifies the string length **in string code
     * units** (not bytes), and must be at least 1. The byte footprint is this value
     * multiplied by the code-unit width of the string type
     * (see @ref NVTX_PAYLOAD_ENTRY_TYPE_CSTRING). The value 0 does not denote a
     * zero-length embedded string; it selects the pointer/null-terminated form
     * instead. An empty embedded string uses a length of at least 1 with a leading
     * null code unit.
     *
     * If @ref flags specify that the entry is an array, this field specifies
     * the array length or length-source index depending on the array flag.
     * See `NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_*` for more details.
     *
     * If @ref type is a union with schema type @ref NVTX_PAYLOAD_SCHEMA_TYPE_UNION
     * (external selection of the union member), this field contains the index
     * (starting with 0) to an entry of integral type in the same schema. The
     * associated field value specifies the selected union member.
     *
     * @note An array of schema type @ref NVTX_PAYLOAD_SCHEMA_TYPE_UNION is not
     * supported. @ref NVTX_PAYLOAD_SCHEMA_TYPE_UNION_WITH_INTERNAL_SELECTOR can
     * be used instead.
     */
    uint64_t arrayOrUnionDetail;

    /**
     * \brief Offset in the binary payload data (in bytes).
     *
     * This field specifies the byte offset from the base address of the actual
     * binary data (blob) to the start address of the data of this entry.
     *
     * It is recommended (but not required) to provide the offset. Otherwise,
     * the NVTX handler will determine the offset from natural alignment rules
     * (capped by @ref nvtxPayloadSchemaAttr_t::packAlign when set).
     *
     * **Implicit offset convention:** For the first entry (index 0), a value
     * of `0` is always treated as an explicit offset. For all subsequent
     * entries (index > 0), a value of `0` signals the tool to compute the
     * offset implicitly. Therefore, to place a later entry explicitly at byte
     * offset 0, use the first entry slot instead. In dynamic schemas, implicit
     * offsets are computed by advancing a running cursor that is aligned per
     * the entry's type alignment.
     *
     * Setting the offset can also be used to skip reserved regions in the
     * payload during parsing.
     */
    uint64_t offset;

    /**
     * \brief Additional semantics of the payload entry.
     *
     * The field points to the first element in a linked list, which enables
     * multiple semantic extensions.
     */
    const nvtxSemanticsHeader_t* semantics;

    /**
     * \brief Reserved for future use.
     */
    const void* reserved;
} nvtxPayloadSchemaEntry_t;

/**
 * \brief NVTX payload schema attributes.
 */
typedef struct nvtxPayloadSchemaAttr_v1
{
    /**
     * \brief Mask of valid fields in this struct.
     *
     * Use the `NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_*` defines.
     */
    uint64_t fieldMask;

    /**
     * \brief Name of the payload schema. (Optional)
     */
    const char* name;

    /**
     * \brief Payload schema type. (Mandatory) \anchor PAYLOAD_TYPE_FIELD
     *
     * Use the `NVTX_PAYLOAD_SCHEMA_TYPE_*` defines.
     */
    uint64_t type;

    /**
     * \brief Payload schema flags. (Optional)
     *
     * Flags defined by `NVTX_PAYLOAD_SCHEMA_FLAG_*` can be used to set
     * additional properties of the schema.
     */
    uint64_t flags;

    /**
     * \brief Entries of a payload schema. (Mandatory) \anchor ENTRIES_FIELD
     *
     * This field points to an array of schema entries, each describing a field
     * in a data structure such as a C struct or union.
     */
    const nvtxPayloadSchemaEntry_t* entries;

    /**
     * \brief Number of entries in the payload schema. (Mandatory)
     *
     * Number of entries in the array of payload entries \ref ENTRIES_FIELD.
     */
    size_t numEntries;

    /**
     * \brief The binary payload size in bytes for static payload schemas.
     *
     * If \ref PAYLOAD_TYPE_FIELD is @ref NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC this
     * value is ignored. If this field is not specified for a schema of type
     * @ref NVTX_PAYLOAD_SCHEMA_TYPE_STATIC, the size can be automatically
     * determined by a tool.
     */
    size_t payloadStaticSize;

    /**
     * \brief The byte alignment for packed structures.
     *
     * If not specified, this field defaults to `0`, which means that the fields
     * in the data structure are not packed and natural alignment rules can be
     * applied.
     *
     * A non-zero value caps each member's effective alignment identically to
     * C's `#pragma pack(N)`:
     * `effectiveAlign = min(naturalAlign, packAlign)`. For example,
     * `packAlign = 4` means no member is aligned to more than 4 bytes,
     * regardless of its natural alignment.
     */
    size_t packAlign;

    /**
     * \brief Unique identifier for this schema.
     *
     * A static payload schema ID must be unique within the domain,
     * >= NVTX_PAYLOAD_SCHEMA_ID_STATIC_START and
     * < NVTX_PAYLOAD_SCHEMA_ID_DYNAMIC_START.
     *
     * Schema IDs and enum IDs share a single namespace within a domain.
     * A given numeric ID must not be used for both a schema and an enum.
     */
    uint64_t schemaId;

    /** Reserved for future use. */
    void* extension;
} nvtxPayloadSchemaAttr_t;

/**
 * \brief Description of one enumeration value.
 *
 * Each entry maps a numeric value to a name and optionally marks it as a bit
 * flag.
 * Arrays of these entries are registered with @ref nvtxPayloadEnumRegister.
 */
typedef struct nvtxPayloadEnum_v1
{
    /**
     * Name of the enum value.
     */
    const char* name;

    /**
     * Value of the enum entry.
     */
    uint64_t value;

    /** Non-zero if this value is a bit flag that can be combined in a mask. */
    int8_t isFlag;
} nvtxPayloadEnum_t;

/**
 * \brief NVTX payload enumeration type attributes.
 *
 * A pointer to this struct is passed to @ref nvtxPayloadEnumRegister.
 */
typedef struct nvtxPayloadEnumAttr_v1
{
    /**
     * Mask of valid fields in this struct. See `NVTX_PAYLOAD_ENUM_ATTR_FIELD_*`.
     */
    uint64_t fieldMask;

    /**
     * Name of the enum. (Optional)
     */
    const char* name;

    /**
     * Entries of the enum. (Mandatory)
     */
    const nvtxPayloadEnum_t* entries;

    /**
     * Number of entries in the enum. (Mandatory)
     */
    size_t numEntries;

    /**
     * \brief Size of the enumeration type in bytes. (Mandatory)
     *
     * Must be set to a non-zero value (typically `sizeof(YourEnumType)`) for a
     * tool to be able to read enum-typed fields. If zero, the tool cannot
     * determine how many bytes to read and may skip enum-typed entries.
     */
    size_t sizeOfEnum;

    /**
     * \brief Unique identifier for this enum type.
     *
     * Same constraints as @ref nvtxPayloadSchemaAttr_t::schemaId.
     */
    uint64_t schemaId;

    /** Reserved for future use. */
    void* extension;
} nvtxPayloadEnumAttr_t;

typedef struct nvtxScopeAttr_v1
{
    /** Size of this struct in bytes (set to `sizeof(nvtxScopeAttr_t)`).
     *  Allows forward-compatible versioning when fields are appended. */
    size_t structSize;

    /**
     * Path delimited by '/' characters, relative to @ref parentScope. Leading
     * slashes are ignored. Nodes in the path may use name[key] syntax to
     * indicate an array of sibling nodes, which may be combined with other
     * non-array nodes or different arrays at the same scope. Node names should
     * be printable UTF-8 characters. '\' is used to escape '/', '[', and
     * ']' characters in node names. An empty C string "" and `NULL` are valid
     * inputs and treated equivalently.
     *
     * A GPU can be specified with the following notations:
     * "GPU[UUID:<unique alphanumeric GPU ID>]",
     * "GPU[CUDAID:<CUDA device ID>]" (sensitive to CUDA_VISIBLE_DEVICES),
     * "GPU[NVSMI:<nvidia-smi(NVML) device ID>]".
     *
     * For display purposes, a tool may show a pretty name.
     * To clearly identify a GPU, @ref parentScope should resolve to a
     * registered scope that represents the GPU's execution context.
     *
     * A CPU can be specified with the following notations:
     * "CPU[<physical package ID>:<OS logical core index>]",
     * "CPU[OS:<physical package ID>:<OS logical core index>]",
     * "CPU[HW:<physical package ID>:<hardware physical core index>]",
     * "CPU[NUMA_OS:<NUMA node ID>:<OS logical core index>]",
     * "CPU[NUMA_HW:<NUMA node ID>:<hardware physical core index>]".
     *
     * Physical package ID:
     * - Windows: GetLogicalProcessorInformationEx (RelationProcessorPackage)
     * - Linux: /sys/devices/system/cpu/cpu\<N\>/topology/physical_package_id or
     *          /proc/cpuinfo (physical id)
     *
     * NUMA node ID:
     * - Windows: GetLogicalProcessorInformationEx (RelationNumaNode)
     * - Linux: /sys/devices/system/cpu/cpu\<N\>/topology/numa_node or libnuma
     *
     * OS logical core index:
     * - Windows: GetCurrentProcessorNumber() or
     *            GetSystemCpuSetInformation (LogicalProcessorIndex)
     * - Linux: sched_getcpu() or /proc/cpuinfo (processor field)
     *
     * Hardware physical core index:
     * - Windows: GetLogicalProcessorInformationEx (RelationProcessorCore)
     * - Linux: /sys/devices/system/cpu/cpu\<N\>/topology/core_id or
     *          /proc/cpuinfo (core id)
     */
    const char* path;

    /**
     * Identifier of the parent scope, to which `path` is appended. Must be
     * `NVTX_SCOPE_ROOT`, `NVTX_SCOPE_NONE`, or a scope ID previously registered
     * with `nvtxScopeRegister` for the same domain. The runtime-resolved scopes
     * (`NVTX_SCOPE_CURRENT_*`) are not valid here.
     */
    uint64_t parentScope;

    /**
     * Static scope ID. Must be unique within the domain,
     * >= NVTX_SCOPE_ID_STATIC_START, and < NVTX_SCOPE_ID_DYNAMIC_START.
     * Use NVTX_SCOPE_NONE to let the tool create a (dynamic) scope ID.
     */
    uint64_t scopeId;
} nvtxScopeAttr_t;

#endif /* NVTX_PAYLOAD_TYPEDEFS_V1 */

#ifndef NVTX_PAYLOAD_TYPEDEFS_DEFERRED_V1
#define NVTX_PAYLOAD_TYPEDEFS_DEFERRED_V1

/** Attributes of an NVTX time domain. */
typedef struct nvtxTimeDomainAttr_v1
{
    /** Identifier of the NVTX scope the time domain is associated with. */
    uint64_t scopeId;

    /** Predefined `NVTX_TIMESTAMP_TYPE_*`. */
    uint64_t timestampTypeId;

    /**
     * Static (feed-forward) time domain ID. `0` makes the tool generate the ID.
     * The static time domain ID must be >= NVTX_TIME_DOMAIN_ID_STATIC_START and
     * < NVTX_TIME_DOMAIN_ID_DYNAMIC_START
     */
    uint64_t timeDomainId;

    /** Properties of the timer (use NVTX_TIMER_FLAG_*). */
    uint64_t timerFlags;

    /** Ticks per second (0 means unknown). */
    int64_t timerResolution;

    /** Point in time when the timer starts (use NVTX_TIMER_START_*). */
    uint64_t timerStart;
} nvtxTimeDomainAttr_t;

/**
 * \brief A pair of timestamps taken at the same instant in two different time
 * domains. Used with @ref nvtxTimeSyncPointTable.
 */
typedef struct nvtxSyncPoint_v1
{
    /** Timestamp in the source time domain (`timeDomainIdSrc`). */
    int64_t src;
    /** Timestamp in the destination time domain (`timeDomainIdDst`). */
    int64_t dst;
} nvtxSyncPoint_t;

/**
 * \brief Helper struct to submit a batch of events (marks or ranges).
 *
 * By default, events are assumed to be chronologically sorted by the first
 * timestamp in the event (start time in a range). If the events are not sorted,
 * the `flags` field must be set accordingly (see `NVTX_BATCH_FLAG_*`).
 */
typedef struct nvtxEventBatch_v1
{
    /**
     * Identifier of the data layout of a deferred event in the array of events.
     * The time domain of event timestamps is provided via time semantics in the
     * schema registration.
     */
    uint64_t eventSchemaId;

    /** Size of the array of deferred events (in bytes). */
    size_t size;

    /**
     * \brief Pointer to the array of deferred events.
     *
     * For static schemas, the stride is the fixed payload size.
     * For dynamic schemas, there is no fixed stride. The buffer is read
     * event by event, advancing the pointer by the size of the event.
     */
    const void* events;

    /**
     * Default scope of events or counters in the batch.
     *
     * A scope from a payload entry or purpose-specific scope semantics takes
     * precedence. If no such scope is specified, the batch scope takes
     * precedence over general scope semantics from the schema.
     *
     * Use @ref NVTX_SCOPE_NONE when the original execution scope is unknown or
     * intentionally unspecified. Deferred events with @ref NVTX_SCOPE_NONE
     * should not be attributed to the thread or process that submits the batch.
     */
    uint64_t scope;

    /** Timestamp ordering (sorted, partially sorted, unsorted), etc. */
    uint64_t flags;

    /**
     * \brief Flexible data blob referenced by events in the batch.
     *
     * Events may contain pointer or offset entries that reference data in this
     * blob (e.g. variable-length strings or arrays). The usable region starts
     * at byte offset @ref flexDataOffset and extends for @ref flexDataSize
     * bytes. A tool resolves event-internal offsets relative to `flexData`.
     */
    const void* flexData;

    /** Size of the usable flexible data region (in bytes). */
    size_t flexDataSize;

    /**
     * Byte offset from @ref flexData to the start of the usable region.
     * Allows the caller to prepend metadata or align the usable area.
     */
    size_t flexDataOffset;
} nvtxEventBatch_t;

#endif /* NVTX_PAYLOAD_TYPEDEFS_DEFERRED_V1 */

#ifndef NVTX_PAYLOAD_API_FUNCTIONS_V1
#define NVTX_PAYLOAD_API_FUNCTIONS_V1

/**
 * \brief Register a payload schema.
 *
 * The `attr` pointer only needs to be valid during the call.
 *
 * @param domain NVTX domain handle.
 * @param attr Pointer to the payload schema attributes.
 *
 * @return The schema ID on success, or `0` on failure (e.g. invalid layout, or
 *         duplicate ID).
 */
NVTX_DECLSPEC uint64_t NVTX_API
nvtxPayloadSchemaRegister(nvtxDomainHandle_t domain, const nvtxPayloadSchemaAttr_t* attr);

/**
 * \brief Register an enumeration type with the payload extension.
 *
 * The `attr` pointer only needs to be valid during the call.
 *
 * @param domain NVTX domain handle
 * @param attr Pointer to the payload enumeration type attributes.
 *
 * @return The enum ID on success, or `0` on failure.
 */
NVTX_DECLSPEC uint64_t NVTX_API
nvtxPayloadEnumRegister(nvtxDomainHandle_t domain, const nvtxPayloadEnumAttr_t* attr);

/**
 * \brief Register a scope.
 *
 * The `attr` pointer only needs to be valid during the call.
 *
 * @param domain NVTX domain handle
 * @param attr Pointer to the scope attributes.
 *
 * @return An identifier for the scope. If the operation was not successful,
 * `NVTX_SCOPE_NONE` is returned.
 */
NVTX_DECLSPEC uint64_t NVTX_API
nvtxScopeRegister(nvtxDomainHandle_t domain, const nvtxScopeAttr_t* attr);

/**
 * \brief Marks an instantaneous event in the application with the attributes
 * being passed via the extended payload.
 *
 * See @ref NVTX_PAYLOAD_EVENT_MESSAGE_REQUIREMENT.
 *
 * @param domain NVTX domain handle
 * @param payloadData pointer to an array of structured payloads.
 * @param count number of payload BLOBs.
 */
NVTX_DECLSPEC void NVTX_API
nvtxMarkPayload(nvtxDomainHandle_t domain, const nvtxPayloadData_t* payloadData, size_t count);

/**
 * \brief Begin a nested thread range with the attributes being passed via the
 * payload.
 *
 * See @ref NVTX_PAYLOAD_EVENT_MESSAGE_REQUIREMENT.
 *
 * @param domain NVTX domain handle
 * @param payloadData Pointer to an array of extended payloads.
 * @param count Number of payloads.
 *
 * @return The new range nesting level. If an error occurs, a negative value is
 * returned on the current thread.
 */
NVTX_DECLSPEC int NVTX_API
nvtxRangePushPayload(nvtxDomainHandle_t domain, const nvtxPayloadData_t* payloadData, size_t count);

/**
 * \brief End a nested thread range with an additional custom payload.
 *
 * NVTX event attributes passed to this function (via the payloads) are later
 * specifications of the same range's attributes; see
 * @ref NVTX_PAYLOAD_EVENT_ATTRIBUTE_PRECEDENCE. Other payload entries extend
 * the data of the range.
 *
 * See @ref NVTX_PAYLOAD_EVENT_MESSAGE_REQUIREMENT.
 *
 * @param domain NVTX domain handle
 * @param payloadData pointer to an array of structured payloads.
 * @param count number of payload BLOBs.
 *
 * @return The ended range nesting level. If an error occurs, a negative value
 * is returned on the current thread.
 */
NVTX_DECLSPEC int NVTX_API
nvtxRangePopPayload(nvtxDomainHandle_t domain, const nvtxPayloadData_t* payloadData, size_t count);

/**
 * \brief Start a thread range with attributes passed via the extended payload.
 *
 * See @ref NVTX_PAYLOAD_EVENT_MESSAGE_REQUIREMENT.
 *
 * @param domain NVTX domain handle
 * @param payloadData pointer to an array of structured payloads.
 * @param count number of payload BLOBs.
 *
 * @return A non-zero unique ID used to correlate a pair of Start and End
 * events. A return value of 0 is a null range ID and does not represent a
 * started range. Applications may initialize nvtxRangeId_t variables to 0 and
 * compare them with 0 to determine whether they reference a started range.
 */
NVTX_DECLSPEC nvtxRangeId_t NVTX_API nvtxRangeStartPayload(
    nvtxDomainHandle_t domain, const nvtxPayloadData_t* payloadData, size_t count);

/**
 * \brief End a thread range and pass a custom payload.
 *
 * Same attribute precedence as @ref nvtxRangePopPayload.
 *
 * @param domain NVTX domain handle
 * @param id The correlation ID returned from a NVTX range start call.
 * @param payloadData pointer to an array of structured payloads.
 * @param count number of payload BLOBs.
 */
NVTX_DECLSPEC void NVTX_API nvtxRangeEndPayload(
    nvtxDomainHandle_t domain,
    nvtxRangeId_t id,
    const nvtxPayloadData_t* payloadData,
    size_t count);

/**
 * \brief Checks if the given NVTX domain is enabled.
 *
 * This function can be used to guard expensive code instrumentation.
 * Applications should generally avoid making execution depend on NVTX API
 * results, such as by branching on whether instrumentation is enabled.
 *
 * If no tool is attached, this function will always return `0`.
 * If a tool is attached, but does not handle this function, `1` is returned.
 * If a tool is attached and handles this function, the return value is
 * determined by the tool. Positive (>0) return values indicate that the domain
 * is enabled, `0` indicates that the domain is disabled.
 *
 * @param domain NVTX domain handle
 * @return 0 if the domain is disabled. Values > 0 indicate an enabled domain.
 */
NVTX_DECLSPEC uint8_t NVTX_API nvtxDomainIsEnabled(nvtxDomainHandle_t domain);

#endif /* NVTX_PAYLOAD_API_FUNCTIONS_V1 */

#ifndef NVTX_PAYLOAD_API_FUNCTIONS_DEFERRED_V1
#define NVTX_PAYLOAD_API_FUNCTIONS_DEFERRED_V1

/**
 * Get a timestamp from the NVTX handler or tool. If no tool is attached, the
 * CPU TSC might be returned. No guarantees are made.
 * The returned timestamp is just meant to be used in deferred events/counters.
 */
NVTX_DECLSPEC int64_t NVTX_API nvtxTimestampGet(void);

/**
 * Register a time domain. Associates an NVTX scope with the time domain.
 * Timestamps of NVTX events or counters in the scope are interpreted according
 * to the time domain definitions.
 *
 * @param domain NVTX domain handle.
 * @param timeAttr Time domain attributes (timestamp type, scope, flags, etc.).
 * @return time domain ID.
 */
NVTX_DECLSPEC uint64_t NVTX_API
nvtxTimeDomainRegister(nvtxDomainHandle_t domain, const nvtxTimeDomainAttr_t* timeAttr);

/**
 * Provide the pointer to a function that returns a timestamp.
 * This enables the tool to create time synchronization points.
 *
 * @param domain NVTX domain handle.
 * @param timeDomainId time domain identifier or timestamp type ID, if it is
 *                     unambiguous.
 * @param flags indicates if it is safe to call the timestamp provider after
 *             process teardown.
 * @param timestampProviderFn Pointer to a function that returns a timestamp.
 */
NVTX_DECLSPEC void NVTX_API nvtxTimerSource(
    nvtxDomainHandle_t domain,
    uint64_t timeDomainId,
    uint64_t flags,
    int64_t (*timestampProviderFn)(void));

/**
 * Same as `nvtxTimerSource`, but with an additional data pointer argument.
 *
 * @param domain NVTX domain handle.
 * @param timeDomainId time domain identifier or timestamp type ID, if it is
 *                     unambiguous.
 * @param flags indicates if it is safe to call the timestamp provider after
 *             process teardown.
 * @param timestampProviderFn Pointer to a function that returns a timestamp.
 * @param data Pointer to data that is passed to the timestamp provider function.
 */
NVTX_DECLSPEC void NVTX_API nvtxTimerSourceWithData(
    nvtxDomainHandle_t domain,
    uint64_t timeDomainId,
    uint64_t flags,
    int64_t (*timestampProviderFn)(void* data),
    void* data);

/**
 * Provides a synchronization point between two time domains.
 * Two synchronization points are required to enable a timestamp conversion.
 * The tool must know one of the time domains, or at least be able to chain
 * conversions to enable the conversion between the given timestamps.
 *
 * @param domain NVTX domain handle.
 * @param timeDomainId1 time domain 1 ID or timestamp type ID, if it is
 *                      unambiguous.
 * @param timeDomainId2 time domain 2 ID or timestamp type ID, if it is
 *                      unambiguous.
 * @param timestamp1 Timestamp in the first time domain.
 * @param timestamp2 Timestamp in the second time domain.
 */
NVTX_DECLSPEC void NVTX_API nvtxTimeSyncPoint(
    nvtxDomainHandle_t domain,
    uint64_t timeDomainId1,
    uint64_t timeDomainId2,
    int64_t timestamp1,
    int64_t timestamp2);

/**
 * The same as `nvtxTimeSyncPoint` but with multiple synchronization points.
 *
 * @param domain NVTX domain handle.
 * @param timeDomainIdSrc source time domain ID or timestamp type ID, if it is
 *                        unambiguous.
 * @param timeDomainIdDst destination time domain ID or timestamp type ID, if it
 *                        is unambiguous.
 * @param syncPoints Pointer to an array of synchronization points.
 * @param count Number of synchronization points.
 */
NVTX_DECLSPEC void NVTX_API nvtxTimeSyncPointTable(
    nvtxDomainHandle_t domain,
    uint64_t timeDomainIdSrc,
    uint64_t timeDomainIdDst,
    const nvtxSyncPoint_t* syncPoints,
    size_t count);

/**
 * @brief Pass a conversion factor between two time domains to the NVTX handler.
 *
 * @param domain NVTX domain handle.
 * @param timeDomainIdSrc source time domain ID or timestamp type ID, if it is
 *                        unambiguous.
 * @param timeDomainIdDst destination time domain ID or timestamp type ID, if it
 *                        is unambiguous.
 * @param slope Conversion factor between the two time domains.
 * @param timestampSrc Timestamp in the source time domain.
 * @param timestampDst Timestamp in the destination time domain.
 */
NVTX_DECLSPEC void NVTX_API nvtxTimestampConversionFactor(
    nvtxDomainHandle_t domain,
    uint64_t timeDomainIdSrc,
    uint64_t timeDomainIdDst,
    double slope,
    int64_t timestampSrc,
    int64_t timestampDst);

/**
 * @brief Submit one deferred event.
 *
 * See @ref NVTX_PAYLOAD_EVENT_MESSAGE_REQUIREMENT.
 *
 * @param domain NVTX domain handle.
 * @param payloadData Pointer to an array of structured payloads.
 * @param numPayloads Number of payloads of the event.
 */
NVTX_DECLSPEC void NVTX_API nvtxEventSubmit(
    nvtxDomainHandle_t domain, const nvtxPayloadData_t* payloadData, size_t numPayloads);

/**
 * \brief Submit a batch of deferred events in the given domain.
 *
 * @param domain NVTX domain handle.
 * @param eventBatch Pointer to deferred events batch details.
 */
NVTX_DECLSPEC void NVTX_API
nvtxEventBatchSubmit(nvtxDomainHandle_t domain, const nvtxEventBatch_t* eventBatch);

#endif /* NVTX_PAYLOAD_API_FUNCTIONS_DEFERRED_V1 */

/**
 * \brief Callback IDs of API functions in the payload extension.
 *
 * The NVTX handler can use these values to register a handler function. When
 * `InitializeInjectionNvtxExtension(nvtxExtModuleInfo_t* moduleInfo)` is
 * executed, a handler routine can be registered as follows:
 * \code{.c}
 *      moduleInfo->segments->slots[NVTX3EXT_CBID_nvtxPayloadSchemaRegister] =
 *          (intptr_t)PayloadSchemaRegisterHandlerFn;
 * \endcode
 */
#ifndef NVTX_PAYLOAD_CALLBACK_ID_V1
#define NVTX_PAYLOAD_CALLBACK_ID_V1

#define NVTX3EXT_CBID_nvtxPayloadSchemaRegister 0
#define NVTX3EXT_CBID_nvtxPayloadEnumRegister 1
#define NVTX3EXT_CBID_nvtxMarkPayload 2
#define NVTX3EXT_CBID_nvtxRangePushPayload 3
#define NVTX3EXT_CBID_nvtxRangePopPayload 4
#define NVTX3EXT_CBID_nvtxRangeStartPayload 5
#define NVTX3EXT_CBID_nvtxRangeEndPayload 6
#define NVTX3EXT_CBID_nvtxDomainIsEnabled 7
#define NVTX3EXT_CBID_nvtxScopeRegister 12

#endif /* NVTX_PAYLOAD_CALLBACK_ID_V1 */

#ifndef NVTX_PAYLOAD_CALLBACK_ID_DEFERRED_V1
#define NVTX_PAYLOAD_CALLBACK_ID_DEFERRED_V1

#define NVTX3EXT_CBID_nvtxTimestampGet 8
#define NVTX3EXT_CBID_nvtxTimeDomainRegister 9
#define NVTX3EXT_CBID_nvtxTimerSource 10
#define NVTX3EXT_CBID_nvtxTimerSourceWithData 11
#define NVTX3EXT_CBID_nvtxTimeSyncPoint 13
#define NVTX3EXT_CBID_nvtxTimeSyncPointTable 14
#define NVTX3EXT_CBID_nvtxTimestampConversionFactor 15
#define NVTX3EXT_CBID_nvtxEventSubmit 16
#define NVTX3EXT_CBID_nvtxEventBatchSubmit 17

#endif /* NVTX_PAYLOAD_CALLBACK_ID_DEFERRED_V1 */

/*** Helper utilities ***/

/** \brief Helper macro for safe double-cast of a pointer to a uint64_t value. */
#ifndef NVTX_POINTER_AS_PAYLOAD_ULLVALUE
#ifdef __cplusplus
#define NVTX_POINTER_AS_PAYLOAD_ULLVALUE(p) static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p))
#else
#define NVTX_POINTER_AS_PAYLOAD_ULLVALUE(p)                                                        \
    (NVTX_STATIC_CAST(uint64_t, NVTX_STATIC_CAST(uintptr_t, p)))
#endif
#endif

#ifndef NVTX_PAYLOAD_EVTATTR_SET_DATA
/**
 * \brief Helper macro to attach a single payload to an NVTX event attribute.
 *
 * @param evtAttr NVTX event attributes (variable name).
 * @param pldata_addr Address of an `nvtxPayloadData_t` variable.
 * @param schema_id NVTX binary payload schema ID.
 * @param pl_addr Address of the payload.
 * @param sz Size of the payload.
 */
#define NVTX_PAYLOAD_EVTATTR_SET_DATA(evtAttr, pldata_addr, schema_id, pl_addr, sz)                \
    (pldata_addr)->schemaId = schema_id;                                                           \
    (pldata_addr)->size = sz;                                                                      \
    (pldata_addr)->payload = pl_addr;                                                              \
    (evtAttr).payload.ullValue = NVTX_POINTER_AS_PAYLOAD_ULLVALUE(pldata_addr);                    \
    (evtAttr).payloadType = NVTX_PAYLOAD_TYPE_EXT;                                                 \
    (evtAttr).reserved0 = 1;
#endif /* NVTX_PAYLOAD_EVTATTR_SET_DATA */

#ifndef NVTX_PAYLOAD_EVTATTR_SET_MULTIPLE
/**
 * \brief Helper macro to attach multiple payloads to an NVTX event attribute.
 *
 * @param evtAttr NVTX event attributes (variable name).
 * @param pldata Payload data array of type `nvtxPayloadData_t`.
 */
#define NVTX_PAYLOAD_EVTATTR_SET_MULTIPLE(evtAttr, pldata)                                         \
    (evtAttr).payloadType = NVTX_PAYLOAD_TYPE_EXT;                                                 \
    (evtAttr).reserved0 = sizeof(pldata) / sizeof(nvtxPayloadData_t);                              \
    (evtAttr).payload.ullValue = NVTX_POINTER_AS_PAYLOAD_ULLVALUE(pldata);
#endif /* NVTX_PAYLOAD_EVTATTR_SET_MULTIPLE */

#ifndef NVTX_PAYLOAD_EVTATTR_SET_MULTIPLE_N
/**
 * \brief Helper macro to attach multiple payloads to an NVTX event attribute
 * with an explicit count of payload data objects.
 *
 * @param evtAttr NVTX event attribute (variable name)
 * @param pldata Payload data array (of type `nvtxPayloadData_t`)
 * @param count Number of entries in payload data array
 */
#define NVTX_PAYLOAD_EVTATTR_SET_MULTIPLE_N(evtAttr, pldata, count) \
    (evtAttr).payloadType = NVTX_PAYLOAD_TYPE_EXT; \
    (evtAttr).reserved0 = NVTX_STATIC_CAST(int32_t, count); \
    (evtAttr).payload.ullValue = NVTX_POINTER_AS_PAYLOAD_ULLVALUE(pldata);
#endif /* NVTX_PAYLOAD_EVTATTR_SET_MULTIPLE_N */

#ifndef NVTX_PAYLOAD_EVTATTR_SET
/*
 * Do not use this macro directly! It is a helper to attach a single payload to
 * an NVTX event attribute.
 * @warning The NVTX push, start, or mark call must be in the same scope.
 */
#define NVTX_PAYLOAD_EVTATTR_SET(evtAttr, schema_id, pl_addr, sz)                                  \
    nvtxPayloadData_t _NVTX_PAYLOAD_DATA_VAR[] = {{schema_id, sz, pl_addr}};                       \
    (evtAttr)->payload.ullValue = NVTX_POINTER_AS_PAYLOAD_ULLVALUE(_NVTX_PAYLOAD_DATA_VAR);        \
    (evtAttr)->payloadType = NVTX_PAYLOAD_TYPE_EXT;                                                \
    (evtAttr)->reserved0 = 1;
#endif /* NVTX_PAYLOAD_EVTATTR_SET */

#ifndef nvtxPayloadRangePush
/**
 * \brief Helper macro to push a range with extended payload.
 *
 * @param domain NVTX domain handle
 * @param evtAttr Pointer to NVTX event attributes.
 * @param schemaId NVTX payload schema ID
 * @param plAddr Pointer to the binary payload data.
 * @param size Size of the binary payload data in bytes.
 */
#define nvtxPayloadRangePush(domain, evtAttr, schemaId, plAddr, size)                              \
    do                                                                                             \
    {                                                                                              \
        NVTX_PAYLOAD_EVTATTR_SET(evtAttr, schemaId, plAddr, size)                                  \
        nvtxDomainRangePushEx(domain, evtAttr);                                                    \
    } while (0)
#endif /* nvtxPayloadRangePush */

#ifndef nvtxPayloadMark
/**
 * \brief Helper macro to set a marker with extended payload.
 *
 * @param domain NVTX domain handle
 * @param evtAttr Pointer to NVTX event attributes.
 * @param schemaId NVTX payload schema ID
 * @param plAddr Pointer to the binary payload data.
 * @param size Size of the binary payload data in bytes.
 */
#define nvtxPayloadMark(domain, evtAttr, schemaId, plAddr, size)                                   \
    do                                                                                             \
    {                                                                                              \
        NVTX_PAYLOAD_EVTATTR_SET(evtAttr, schemaId, plAddr, size)                                  \
        nvtxDomainMarkEx(domain, evtAttr);                                                         \
    } while (0)
#endif /* nvtxPayloadMark */

/* Macros to create versioned symbols. */
#ifndef NVTX_EXT_PAYLOAD_VERSIONED_IDENTIFIERS_V1
#define NVTX_EXT_PAYLOAD_VERSIONED_IDENTIFIERS_V1
#define NVTX_EXT_PAYLOAD_VERSIONED_IDENTIFIER_L3(NAME, VERSION, COMPATID)                          \
    NAME##_v##VERSION##_bpl##COMPATID
#define NVTX_EXT_PAYLOAD_VERSIONED_IDENTIFIER_L2(NAME, VERSION, COMPATID)                          \
    NVTX_EXT_PAYLOAD_VERSIONED_IDENTIFIER_L3(NAME, VERSION, COMPATID)
#define NVTX_EXT_PAYLOAD_VERSIONED_ID(NAME)                                                        \
    NVTX_EXT_PAYLOAD_VERSIONED_IDENTIFIER_L2(NAME, NVTX_VERSION, NVTX_EXT_PAYLOAD_COMPATID)
#endif /* NVTX_EXT_PAYLOAD_VERSIONED_IDENTIFIERS_V1 */

#ifdef __GNUC__
#pragma GCC visibility push(internal)
#endif

/* Extension types are required for the implementation and the NVTX handler. */
#define NVTX_EXT_TYPES_GUARD
#include "nvtxDetail/nvtxExtTypes.h"
#undef NVTX_EXT_TYPES_GUARD

#ifndef NVTX_NO_IMPL
#define NVTX_EXT_IMPL_PAYLOAD_GUARD
#include "nvtxDetail/nvtxExtImplPayload_v1.h"
#undef NVTX_EXT_IMPL_PAYLOAD_GUARD
#endif /* NVTX_NO_IMPL */

#ifdef __GNUC__
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
