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

#if !defined(NVTXW_EVENT_HELPERS_API)
#define NVTXW_EVENT_HELPERS_API

#include <nvtxw3/nvtxw3.h>

#include <stddef.h> /* For offsetof */
#include <string.h> /* For memset/strlen */

/**
 * \file nvtxw3_event_helpers.h
 * \brief Stateless helpers for writing event payloads through NVTXW EventWrite.
 *
 * This header contains stateless helper schemas and wrappers for writing marks,
 * ranges, and range events through the core NVTXW EventWrite interface.
 *
 * User-facing types and function declarations are kept together below.
 * Payload layout structs and Impl functions are implementation details and are
 * not intended to be used directly.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** \name Event Helper Attributes
 * @{ */

/** Event attributes for event helper write functions. */
typedef struct nvtxwEventAttributes_v1
{
    /** ARGB color value. 0 means no explicit color. */
    uint32_t color;

    /** NVTX category value. 0 means no category. */
    uint32_t category;

    /** Registered string message. 0 means no message. */
    nvtxStringHandle_t message;
} nvtxwEventAttributes_t;

/** UTF-8 event attributes for event helper write functions. */
typedef struct nvtxwEventAttributesUtf8_v1
{
    /** ARGB color value. 0 means no explicit color. */
    uint32_t color;

    /** NVTX category value. 0 means no category. */
    uint32_t category;

    /** Message length in bytes, excluding null terminator. 0 (the default for
     *  a zero-initialized struct) means the message is null-terminated and its
     *  length is computed with strlen. A non-zero value is used verbatim and
     *  supports messages that are not null-terminated. */
    uint32_t messageLength;

    /** Message bytes, which must be valid UTF-8. NULL means no message. ASCII
     *  is valid UTF-8 and is the common case; text in another encoding must be
     *  transcoded to UTF-8 by the caller, as tools may render invalid sequences
     *  as replacement characters. Need not be null-terminated when
     *  messageLength is an explicit byte count. */
    const char* message;
} nvtxwEventAttributesUtf8_t;

/** Dynamic schema IDs registered by nvtxwEventSchemasRegister. */
typedef struct nvtxwEventHelperSchemaIds_v1
{
    uint64_t markUtf8;
    uint64_t markRegString;
    uint64_t rangePushPopUtf8;
    uint64_t rangePushPopRegString;
    uint64_t rangeStartEndUtf8;
    uint64_t rangeStartEndRegString;
    uint64_t rangePushUtf8;
    uint64_t rangePushRegString;
    uint64_t rangePop;
    uint64_t rangeStartUtf8;
    uint64_t rangeStartRegString;
    uint64_t rangeEnd;
    /** Shared schema for the UTF-8 event-message payload. It is registered
     *  automatically whenever any UTF-8 event schema is selected, and is used
     *  as the second payload of every UTF-8 write function. */
    uint64_t eventMessageUtf8;
} nvtxwEventHelperSchemaIds_t;

/** Schema selection flags for nvtxwEventSchemasRegister. */
typedef enum nvtxwEventHelperSchemaFlag_v1
{
    NVTXW_EVENT_HELPER_SCHEMA_MARK_UTF8 = 1u << 0,
    NVTXW_EVENT_HELPER_SCHEMA_MARK_REG_STRING = 1u << 1,
    NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSHPOP_UTF8 = 1u << 2,
    NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSHPOP_REG_STRING = 1u << 3,
    NVTXW_EVENT_HELPER_SCHEMA_RANGE_STARTEND_UTF8 = 1u << 4,
    NVTXW_EVENT_HELPER_SCHEMA_RANGE_STARTEND_REG_STRING = 1u << 5,
    NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSH_UTF8 = 1u << 6,
    NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSH_REG_STRING = 1u << 7,
    NVTXW_EVENT_HELPER_SCHEMA_RANGE_POP = 1u << 8,
    NVTXW_EVENT_HELPER_SCHEMA_RANGE_START_UTF8 = 1u << 9,
    NVTXW_EVENT_HELPER_SCHEMA_RANGE_START_REG_STRING = 1u << 10,
    NVTXW_EVENT_HELPER_SCHEMA_RANGE_END = 1u << 11,
    NVTXW_EVENT_HELPER_SCHEMA_ALL =
        NVTXW_EVENT_HELPER_SCHEMA_MARK_UTF8 |
        NVTXW_EVENT_HELPER_SCHEMA_MARK_REG_STRING |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSHPOP_UTF8 |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSHPOP_REG_STRING |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_STARTEND_UTF8 |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_STARTEND_REG_STRING |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSH_UTF8 |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSH_REG_STRING |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_POP |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_START_UTF8 |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_START_REG_STRING |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_END
} nvtxwEventHelperSchemaFlag_t;

/** Context for the event helper write functions.
 *
 * Groups the handles that every write needs. They are mutually constrained:
 * `schemaIds` must have been registered (via \ref nvtxwEventSchemasRegister) in
 * the same domain the `stream` was opened on, and `iface` is the interface that
 * owns both.
 *
 * `schemaIds` is held by value, so the writer is self-contained: only the
 * object `iface` points at must outlive the writer (and every write made
 * through it). To target several streams of one domain, register the schemas
 * once, build one writer, then copy it and replace `stream` for each additional
 * stream. */
typedef struct nvtxwEventWriter_v1
{
    /** Interface that owns the stream and schemas. */
    const nvtxwInterface_t* iface;

    /** Stream events are written to. Carries the domain. */
    nvtxwStreamHandle_t stream;

    /** Schema IDs registered in the stream's domain. */
    nvtxwEventHelperSchemaIds_t schemaIds;
} nvtxwEventWriter_t;

/** @} */

/** Initialize an event writer from its handles.
 *
 * Returns NVTXW_RESULT_INVALID_ARGUMENT if `writer` is NULL. `iface` and
 * `stream` are stored as-is; `schemaIds` is copied by value (a NULL `schemaIds`
 * clears the IDs to zero). The handles are not otherwise validated here; the
 * write functions validate them.
 *
 * For a one-call variant that also opens the stream and registers all event
 * schemas, see \ref nvtxwEventWriterOpen in nvtxw3_setup_helpers.h. */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwEventWriterInit(
    nvtxwEventWriter_t* writer,
    const nvtxwInterface_t* iface,
    nvtxwStreamHandle_t stream,
    const nvtxwEventHelperSchemaIds_t* schemaIds);

/** Register selected schemas used by the event helper write functions.
 *
 * Timestamp entries in these schemas do not carry per-entry time semantics, so
 * they use the stream default time domain when written through NVTXW.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwEventSchemasRegister(
    const nvtxwInterface_t* iface,
    nvtxDomainHandle_t domain,
    uint32_t schemaMask,
    nvtxwEventHelperSchemaIds_t* schemaIdsOut);

/** \name Event Helper Writes
 *
 * The timestamp arguments are the first event fields after the writer.
 * These helpers register timestamp entries without per-entry time semantics;
 * the timestamps therefore use the stream default time domain. If the stream
 * has no default, the timestamp source is unknown or implementation-defined.
 * `writer` bundles the interface, stream, and schema IDs; its `schemaIds` must
 * have been initialized by \ref nvtxwEventSchemasRegister with the schema
 * needed by the write function.
 * @{ */

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwMarkWriteUtf8(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxwEventAttributesUtf8_t attr);

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwMarkWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxwEventAttributes_t attr);

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangePushPopWriteUtf8(
    const nvtxwEventWriter_t* writer,
    int64_t timestampBegin,
    int64_t timestampEnd,
    nvtxwEventAttributesUtf8_t attr);

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangePushPopWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestampBegin,
    int64_t timestampEnd,
    nvtxwEventAttributes_t attr);

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangeStartEndWriteUtf8(
    const nvtxwEventWriter_t* writer,
    int64_t timestampBegin,
    int64_t timestampEnd,
    nvtxwEventAttributesUtf8_t attr);

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangeStartEndWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestampBegin,
    int64_t timestampEnd,
    nvtxwEventAttributes_t attr);

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangePushWriteUtf8(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxwEventAttributesUtf8_t attr);

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangePushWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxwEventAttributes_t attr);

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangePopWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp);

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangeStartWriteUtf8(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxRangeId_t rangeId,
    nvtxwEventAttributesUtf8_t attr);

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangeStartWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxRangeId_t rangeId,
    nvtxwEventAttributes_t attr);

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangeEndWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxRangeId_t rangeId);

/** @} */

/** \cond NVTXW_INTERNAL */

/* Implementation details. These payload layouts are visible only because the
 * helper schemas need complete static layouts. */

/* Fixed header for UTF-8 payloads. The UTF-8 message is not embedded in this
 * struct: it travels as a separate, standalone string payload (see the shared
 * eventMessageUtf8 schema) whose bytes point directly at the caller's message.
 * This avoids both deep copy and assembling the header and message into one
 * combined buffer, so these header schemas are fixed-size (static). */
typedef struct nvtxwEventUtf8HeaderAttr_v1
{
    uint32_t color;
    uint32_t category;
} nvtxwEventUtf8HeaderAttr_t;

typedef struct nvtxwEventUtf8Payload_v1
{
    int64_t timestamp;
    nvtxwEventUtf8HeaderAttr_t attr;
} nvtxwEventUtf8Payload_t;

typedef struct nvtxwEventRegStringPayload_v1
{
    int64_t timestamp;
    nvtxwEventAttributes_t attr;
} nvtxwEventRegStringPayload_t;

typedef struct nvtxwRangePopPayload_v1
{
    int64_t timestamp;
} nvtxwRangePopPayload_t;

typedef struct nvtxwRangeEventUtf8Payload_v1
{
    int64_t timestampBegin;
    int64_t timestampEnd;
    nvtxwEventUtf8HeaderAttr_t attr;
} nvtxwRangeEventUtf8Payload_t;

typedef struct nvtxwRangeEventRegStringPayload_v1
{
    int64_t timestampBegin;
    int64_t timestampEnd;
    nvtxwEventAttributes_t attr;
} nvtxwRangeEventRegStringPayload_t;

typedef struct nvtxwRangeIdEventUtf8Payload_v1
{
    nvtxRangeId_t rangeId;
    int64_t timestamp;
    nvtxwEventUtf8HeaderAttr_t attr;
} nvtxwRangeIdEventUtf8Payload_t;

typedef struct nvtxwRangeIdEventRegStringPayload_v1
{
    nvtxRangeId_t rangeId;
    int64_t timestamp;
    nvtxwEventAttributes_t attr;
} nvtxwRangeIdEventRegStringPayload_t;

typedef struct nvtxwRangeEndPayload_v1
{
    nvtxRangeId_t rangeId;
    int64_t timestamp;
} nvtxwRangeEndPayload_t;

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwEventWriterInit(
    nvtxwEventWriter_t* writer,
    const nvtxwInterface_t* iface,
    nvtxwStreamHandle_t stream,
    const nvtxwEventHelperSchemaIds_t* schemaIds)
{
    if (!writer) return NVTXW_RESULT_INVALID_ARGUMENT;
    writer->iface = iface;
    writer->stream = stream;
    if (schemaIds)
        writer->schemaIds = *schemaIds;
    else
        memset(&writer->schemaIds, 0, sizeof(writer->schemaIds));
    return NVTXW_RESULT_SUCCESS;
}

/* Register selected schemas used by the event helper write functions. */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwEventSchemasRegister(
    const nvtxwInterface_t* iface,
    nvtxDomainHandle_t domain,
    uint32_t schemaMask,
    nvtxwEventHelperSchemaIds_t* schemaIdsOut)
{
#define NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(PAYLOAD_TYPE, FIELD, FLAGS) \
    { \
        NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP | (FLAGS), \
        NVTX_PAYLOAD_ENTRY_TYPE_INT64, \
        #FIELD, \
        NVTX_NULLPTR, \
        0, \
        offsetof(PAYLOAD_TYPE, FIELD), \
        NVTX_NULLPTR, \
        NVTX_NULLPTR \
    }
#define NVTXW_EVENT_HELPER_ENTRY_COLOR(PAYLOAD_TYPE) \
    { \
        0, \
        NVTX_PAYLOAD_ENTRY_TYPE_COLOR_ARGB, \
        "color", \
        NVTX_NULLPTR, \
        0, \
        offsetof(PAYLOAD_TYPE, attr.color), \
        NVTX_NULLPTR, \
        NVTX_NULLPTR \
    }
#define NVTXW_EVENT_HELPER_ENTRY_CATEGORY(PAYLOAD_TYPE) \
    { \
        0, \
        NVTX_PAYLOAD_ENTRY_TYPE_CATEGORY, \
        "category", \
        NVTX_NULLPTR, \
        0, \
        offsetof(PAYLOAD_TYPE, attr.category), \
        NVTX_NULLPTR, \
        NVTX_NULLPTR \
    }
#define NVTXW_EVENT_HELPER_ENTRY_RANGE_ID(PAYLOAD_TYPE) \
    { \
        0, \
        NVTX_PAYLOAD_ENTRY_TYPE_RANGE_ID, \
        "rangeId", \
        NVTX_NULLPTR, \
        0, \
        offsetof(PAYLOAD_TYPE, rangeId), \
        NVTX_NULLPTR, \
        NVTX_NULLPTR \
    }
#define NVTXW_EVENT_HELPER_ENTRY_MESSAGE_REG_STRING(PAYLOAD_TYPE) \
    { \
        NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE, \
        NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE, \
        "message", \
        NVTX_NULLPTR, \
        0, \
        offsetof(PAYLOAD_TYPE, attr.message), \
        NVTX_NULLPTR, \
        NVTX_NULLPTR \
    }

    /* The sole entry of the UTF-8 message schema. The payload is the message
     * itself (pointing directly at the caller's string), so the entry is a
     * zero-terminated UTF-8 string. This requires a dynamic schema. */
    static const nvtxPayloadSchemaEntry_t eventMessageUtf8Entries[] =
    {
        {
            NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE |
                NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED,
            NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF8,
            "message",
            NVTX_NULLPTR,
            0,
            0,
            NVTX_NULLPTR,
            NVTX_NULLPTR
        }
    };

    static const nvtxPayloadSchemaEntry_t markUtf8Entries[] =
    {
        NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(
            nvtxwEventUtf8Payload_t,
            timestamp,
            NVTX_PAYLOAD_ENTRY_FLAG_MARK),
        NVTXW_EVENT_HELPER_ENTRY_COLOR(nvtxwEventUtf8Payload_t),
        NVTXW_EVENT_HELPER_ENTRY_CATEGORY(nvtxwEventUtf8Payload_t)
    };

    static const nvtxPayloadSchemaEntry_t markRegStringEntries[] =
    {
        NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(
            nvtxwEventRegStringPayload_t,
            timestamp,
            NVTX_PAYLOAD_ENTRY_FLAG_MARK),
        NVTXW_EVENT_HELPER_ENTRY_COLOR(nvtxwEventRegStringPayload_t),
        NVTXW_EVENT_HELPER_ENTRY_CATEGORY(nvtxwEventRegStringPayload_t),
        NVTXW_EVENT_HELPER_ENTRY_MESSAGE_REG_STRING(nvtxwEventRegStringPayload_t)
    };

    static const nvtxPayloadSchemaEntry_t completeRangeUtf8Entries[] =
    {
        NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(
            nvtxwRangeEventUtf8Payload_t,
            timestampBegin,
            NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN),
        NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(
            nvtxwRangeEventUtf8Payload_t,
            timestampEnd,
            NVTX_PAYLOAD_ENTRY_FLAG_RANGE_END),
        NVTXW_EVENT_HELPER_ENTRY_COLOR(nvtxwRangeEventUtf8Payload_t),
        NVTXW_EVENT_HELPER_ENTRY_CATEGORY(nvtxwRangeEventUtf8Payload_t)
    };

    static const nvtxPayloadSchemaEntry_t completeRangeRegStringEntries[] =
    {
        NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(
            nvtxwRangeEventRegStringPayload_t,
            timestampBegin,
            NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN),
        NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(
            nvtxwRangeEventRegStringPayload_t,
            timestampEnd,
            NVTX_PAYLOAD_ENTRY_FLAG_RANGE_END),
        NVTXW_EVENT_HELPER_ENTRY_COLOR(nvtxwRangeEventRegStringPayload_t),
        NVTXW_EVENT_HELPER_ENTRY_CATEGORY(nvtxwRangeEventRegStringPayload_t),
        NVTXW_EVENT_HELPER_ENTRY_MESSAGE_REG_STRING(nvtxwRangeEventRegStringPayload_t)
    };

    static const nvtxPayloadSchemaEntry_t rangePushUtf8Entries[] =
    {
        NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(
            nvtxwEventUtf8Payload_t,
            timestamp,
            NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN),
        NVTXW_EVENT_HELPER_ENTRY_COLOR(nvtxwEventUtf8Payload_t),
        NVTXW_EVENT_HELPER_ENTRY_CATEGORY(nvtxwEventUtf8Payload_t)
    };

    static const nvtxPayloadSchemaEntry_t rangePushRegStringEntries[] =
    {
        NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(
            nvtxwEventRegStringPayload_t,
            timestamp,
            NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN),
        NVTXW_EVENT_HELPER_ENTRY_COLOR(nvtxwEventRegStringPayload_t),
        NVTXW_EVENT_HELPER_ENTRY_CATEGORY(nvtxwEventRegStringPayload_t),
        NVTXW_EVENT_HELPER_ENTRY_MESSAGE_REG_STRING(nvtxwEventRegStringPayload_t)
    };

    static const nvtxPayloadSchemaEntry_t rangePopEntries[] =
    {
        NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(
            nvtxwRangePopPayload_t,
            timestamp,
            NVTX_PAYLOAD_ENTRY_FLAG_RANGE_END)
    };

    static const nvtxPayloadSchemaEntry_t rangeStartUtf8Entries[] =
    {
        NVTXW_EVENT_HELPER_ENTRY_RANGE_ID(nvtxwRangeIdEventUtf8Payload_t),
        NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(
            nvtxwRangeIdEventUtf8Payload_t,
            timestamp,
            NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN),
        NVTXW_EVENT_HELPER_ENTRY_COLOR(nvtxwRangeIdEventUtf8Payload_t),
        NVTXW_EVENT_HELPER_ENTRY_CATEGORY(nvtxwRangeIdEventUtf8Payload_t)
    };

    static const nvtxPayloadSchemaEntry_t rangeStartRegStringEntries[] =
    {
        NVTXW_EVENT_HELPER_ENTRY_RANGE_ID(nvtxwRangeIdEventRegStringPayload_t),
        NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(
            nvtxwRangeIdEventRegStringPayload_t,
            timestamp,
            NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN),
        NVTXW_EVENT_HELPER_ENTRY_COLOR(nvtxwRangeIdEventRegStringPayload_t),
        NVTXW_EVENT_HELPER_ENTRY_CATEGORY(nvtxwRangeIdEventRegStringPayload_t),
        NVTXW_EVENT_HELPER_ENTRY_MESSAGE_REG_STRING(nvtxwRangeIdEventRegStringPayload_t)
    };

    static const nvtxPayloadSchemaEntry_t rangeEndEntries[] =
    {
        NVTXW_EVENT_HELPER_ENTRY_RANGE_ID(nvtxwRangeEndPayload_t),
        NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP(
            nvtxwRangeEndPayload_t,
            timestamp,
            NVTX_PAYLOAD_ENTRY_FLAG_RANGE_END)
    };

#undef NVTXW_EVENT_HELPER_ENTRY_TIMESTAMP
#undef NVTXW_EVENT_HELPER_ENTRY_COLOR
#undef NVTXW_EVENT_HELPER_ENTRY_CATEGORY
#undef NVTXW_EVENT_HELPER_ENTRY_RANGE_ID
#undef NVTXW_EVENT_HELPER_ENTRY_MESSAGE_REG_STRING

    typedef struct nvtxwEventHelperSchemaDesc_v1
    {
        uint32_t selector;
        uint64_t flags;
        uint32_t type;
        const nvtxPayloadSchemaEntry_t* entries;
        size_t numEntries;
        size_t payloadStaticSize;
        uint64_t* schemaId;
    } nvtxwEventHelperSchemaDesc_t;

    nvtxPayloadSchemaAttr_t attr;
    nvtxwEventHelperSchemaDesc_t schemas[13];
    nvtxwResultCode_t result;
    size_t i;

    /* The standalone UTF-8 message schema is shared by all UTF-8 event schemas,
     * so it is registered whenever any UTF-8 schema is selected. */
    const uint32_t utf8Selectors =
        NVTXW_EVENT_HELPER_SCHEMA_MARK_UTF8 |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSHPOP_UTF8 |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_STARTEND_UTF8 |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSH_UTF8 |
        NVTXW_EVENT_HELPER_SCHEMA_RANGE_START_UTF8;

    if (!iface || !iface->SchemaRegister || !domain || !schemaIdsOut || !schemaMask)
        return NVTXW_RESULT_INVALID_ARGUMENT;
    if (schemaMask & ~NVTXW_EVENT_HELPER_SCHEMA_ALL)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(schemaIdsOut, 0, sizeof(*schemaIdsOut));

#define NVTXW_EVENT_HELPER_SCHEMA_DESC( \
    INDEX, SELECTOR, FLAGS, TYPE, ENTRIES, PAYLOAD_TYPE, SCHEMA_ID_FIELD) \
    do \
    { \
        schemas[INDEX].selector = (SELECTOR); \
        schemas[INDEX].flags = (FLAGS); \
        schemas[INDEX].type = (TYPE); \
        schemas[INDEX].entries = (ENTRIES); \
        schemas[INDEX].numEntries = sizeof(ENTRIES) / sizeof((ENTRIES)[0]); \
        schemas[INDEX].payloadStaticSize = sizeof(PAYLOAD_TYPE); \
        schemas[INDEX].schemaId = &schemaIdsOut->SCHEMA_ID_FIELD; \
    } while (0)

    NVTXW_EVENT_HELPER_SCHEMA_DESC(0, NVTXW_EVENT_HELPER_SCHEMA_MARK_UTF8,
        NVTX_PAYLOAD_SCHEMA_FLAG_MARK, NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        markUtf8Entries, nvtxwEventUtf8Payload_t, markUtf8);
    NVTXW_EVENT_HELPER_SCHEMA_DESC(1, NVTXW_EVENT_HELPER_SCHEMA_MARK_REG_STRING,
        NVTX_PAYLOAD_SCHEMA_FLAG_MARK, NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        markRegStringEntries, nvtxwEventRegStringPayload_t, markRegString);
    NVTXW_EVENT_HELPER_SCHEMA_DESC(2, NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSHPOP_UTF8,
        NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSHPOP, NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        completeRangeUtf8Entries, nvtxwRangeEventUtf8Payload_t, rangePushPopUtf8);
    NVTXW_EVENT_HELPER_SCHEMA_DESC(3, NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSHPOP_REG_STRING,
        NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSHPOP, NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        completeRangeRegStringEntries, nvtxwRangeEventRegStringPayload_t, rangePushPopRegString);
    NVTXW_EVENT_HELPER_SCHEMA_DESC(4, NVTXW_EVENT_HELPER_SCHEMA_RANGE_STARTEND_UTF8,
        NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_STARTEND, NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        completeRangeUtf8Entries, nvtxwRangeEventUtf8Payload_t, rangeStartEndUtf8);
    NVTXW_EVENT_HELPER_SCHEMA_DESC(5, NVTXW_EVENT_HELPER_SCHEMA_RANGE_STARTEND_REG_STRING,
        NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_STARTEND, NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        completeRangeRegStringEntries, nvtxwRangeEventRegStringPayload_t, rangeStartEndRegString);
    NVTXW_EVENT_HELPER_SCHEMA_DESC(6, NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSH_UTF8,
        NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSH, NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        rangePushUtf8Entries, nvtxwEventUtf8Payload_t, rangePushUtf8);
    NVTXW_EVENT_HELPER_SCHEMA_DESC(7, NVTXW_EVENT_HELPER_SCHEMA_RANGE_PUSH_REG_STRING,
        NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSH, NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        rangePushRegStringEntries, nvtxwEventRegStringPayload_t, rangePushRegString);
    NVTXW_EVENT_HELPER_SCHEMA_DESC(8, NVTXW_EVENT_HELPER_SCHEMA_RANGE_POP,
        NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_POP, NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        rangePopEntries, nvtxwRangePopPayload_t, rangePop);
    NVTXW_EVENT_HELPER_SCHEMA_DESC(9, NVTXW_EVENT_HELPER_SCHEMA_RANGE_START_UTF8,
        NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_START, NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        rangeStartUtf8Entries, nvtxwRangeIdEventUtf8Payload_t, rangeStartUtf8);
    NVTXW_EVENT_HELPER_SCHEMA_DESC(10, NVTXW_EVENT_HELPER_SCHEMA_RANGE_START_REG_STRING,
        NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_START, NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        rangeStartRegStringEntries, nvtxwRangeIdEventRegStringPayload_t, rangeStartRegString);
    NVTXW_EVENT_HELPER_SCHEMA_DESC(11, NVTXW_EVENT_HELPER_SCHEMA_RANGE_END,
        NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_END, NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        rangeEndEntries, nvtxwRangeEndPayload_t, rangeEnd);

#undef NVTXW_EVENT_HELPER_SCHEMA_DESC

    /* Shared standalone UTF-8 message schema. It carries no event-role flag and
     * has no fixed part: its payload bytes are the zero-terminated message. It
     * is registered whenever any UTF-8 event schema is selected. */
    schemas[12].selector = utf8Selectors;
    schemas[12].flags = 0;
    schemas[12].type = NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC;
    schemas[12].entries = eventMessageUtf8Entries;
    schemas[12].numEntries =
        sizeof(eventMessageUtf8Entries) / sizeof(eventMessageUtf8Entries[0]);
    schemas[12].payloadStaticSize = 0;
    schemas[12].schemaId = &schemaIdsOut->eventMessageUtf8;

    memset(&attr, 0, sizeof(attr));
    attr.fieldMask =
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_FLAGS |
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES |
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE;

    for (i = 0; i < sizeof(schemas) / sizeof(schemas[0]); ++i)
    {
        if (!(schemaMask & schemas[i].selector)) continue;

        /* The UTF-8 message schema embeds a variable-length string and is
         * therefore dynamic; all other schemas are static. */
        attr.type = schemas[i].type;
        attr.flags = schemas[i].flags;
        attr.entries = schemas[i].entries;
        attr.numEntries = schemas[i].numEntries;
        attr.payloadStaticSize = schemas[i].payloadStaticSize;
        result = iface->SchemaRegister(domain, &attr, schemas[i].schemaId);
        if (result != NVTXW_RESULT_SUCCESS) return result;
    }

    return NVTXW_RESULT_SUCCESS;
}

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwEventHelperImplWritePayload(
    const nvtxwInterface_t* iface,
    nvtxwStreamHandle_t stream,
    uint64_t schemaId,
    const void* payload,
    size_t payloadSize)
{
    nvtxPayloadData_t payloadData;

    if (!iface || !iface->EventWrite || !stream ||
        !schemaId || !payload || !payloadSize)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    payloadData.schemaId = schemaId;
    payloadData.size = payloadSize;
    payloadData.payload = payload;

    return iface->EventWrite(stream, &payloadData, 1);
}

/* Write a UTF-8 event as two payloads: the fixed event header and a message
 * payload whose bytes point directly at the caller's message string. This avoids
 * deep copy, heap allocation, and copying the message into a combined buffer.
 *
 * `messageLength` is the message length in bytes, or 0 to derive it with
 * `strlen`. A non-zero length supports non-null-terminated strings and is
 * never read past, so the message need not be null-terminated. A NULL message
 * is sent as an empty (present, zero-length) message so the event always
 * carries a message. */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwEventHelperImplWriteUtf8(
    const nvtxwInterface_t* iface,
    nvtxwStreamHandle_t stream,
    uint64_t schemaId,
    uint64_t messageSchemaId,
    const void* header,
    size_t headerSize,
    const char* message,
    uint32_t messageLength)
{
    nvtxPayloadData_t payloads[2];
    size_t length;

    if (!iface || !iface->EventWrite || !stream ||
        !schemaId || !messageSchemaId || !header || !headerSize)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    if (!message)
        length = 0;
    else if (messageLength == 0)
        length = strlen(message);
    else
        length = messageLength;

    payloads[0].schemaId = schemaId;
    payloads[0].size = headerSize;
    payloads[0].payload = header;

    /* The message bytes are the payload. Payloads of size 0 are ignored, so an
     * empty message is sent as a single null code unit (a present empty value). */
    payloads[1].schemaId = messageSchemaId;
    if (length == 0)
    {
        payloads[1].size = 1;
        payloads[1].payload = "";
    }
    else
    {
        payloads[1].size = length;
        payloads[1].payload = message;
    }

    return iface->EventWrite(stream, payloads, 2);
}

/* Event write wrapper implementations. */

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwMarkWriteUtf8(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxwEventAttributesUtf8_t attr)
{
    nvtxwEventUtf8Payload_t header;

    if (!writer)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(&header, 0, sizeof(header));
    header.timestamp = timestamp;
    header.attr.color = attr.color;
    header.attr.category = attr.category;

    return nvtxwEventHelperImplWriteUtf8(
        writer->iface,
        writer->stream,
        writer->schemaIds.markUtf8,
        writer->schemaIds.eventMessageUtf8,
        &header,
        sizeof(header),
        attr.message,
        attr.messageLength);
}

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwMarkWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxwEventAttributes_t attr)
{
    nvtxwEventRegStringPayload_t payload;

    if (!writer)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(&payload, 0, sizeof(payload));
    payload.timestamp = timestamp;
    payload.attr = attr;

    return nvtxwEventHelperImplWritePayload(
        writer->iface,
        writer->stream,
        writer->schemaIds.markRegString,
        &payload,
        sizeof(payload));
}

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangePushPopWriteUtf8(
    const nvtxwEventWriter_t* writer,
    int64_t timestampBegin,
    int64_t timestampEnd,
    nvtxwEventAttributesUtf8_t attr)
{
    nvtxwRangeEventUtf8Payload_t header;

    if (!writer)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(&header, 0, sizeof(header));
    header.timestampBegin = timestampBegin;
    header.timestampEnd = timestampEnd;
    header.attr.color = attr.color;
    header.attr.category = attr.category;

    return nvtxwEventHelperImplWriteUtf8(
        writer->iface,
        writer->stream,
        writer->schemaIds.rangePushPopUtf8,
        writer->schemaIds.eventMessageUtf8,
        &header,
        sizeof(header),
        attr.message,
        attr.messageLength);
}

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangePushPopWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestampBegin,
    int64_t timestampEnd,
    nvtxwEventAttributes_t attr)
{
    nvtxwRangeEventRegStringPayload_t payload;

    if (!writer)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(&payload, 0, sizeof(payload));
    payload.timestampBegin = timestampBegin;
    payload.timestampEnd = timestampEnd;
    payload.attr = attr;

    return nvtxwEventHelperImplWritePayload(
        writer->iface,
        writer->stream,
        writer->schemaIds.rangePushPopRegString,
        &payload,
        sizeof(payload));
}

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangeStartEndWriteUtf8(
    const nvtxwEventWriter_t* writer,
    int64_t timestampBegin,
    int64_t timestampEnd,
    nvtxwEventAttributesUtf8_t attr)
{
    nvtxwRangeEventUtf8Payload_t header;

    if (!writer)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(&header, 0, sizeof(header));
    header.timestampBegin = timestampBegin;
    header.timestampEnd = timestampEnd;
    header.attr.color = attr.color;
    header.attr.category = attr.category;

    return nvtxwEventHelperImplWriteUtf8(
        writer->iface,
        writer->stream,
        writer->schemaIds.rangeStartEndUtf8,
        writer->schemaIds.eventMessageUtf8,
        &header,
        sizeof(header),
        attr.message,
        attr.messageLength);
}

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangeStartEndWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestampBegin,
    int64_t timestampEnd,
    nvtxwEventAttributes_t attr)
{
    nvtxwRangeEventRegStringPayload_t payload;

    if (!writer)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(&payload, 0, sizeof(payload));
    payload.timestampBegin = timestampBegin;
    payload.timestampEnd = timestampEnd;
    payload.attr = attr;

    return nvtxwEventHelperImplWritePayload(
        writer->iface,
        writer->stream,
        writer->schemaIds.rangeStartEndRegString,
        &payload,
        sizeof(payload));
}

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangePushWriteUtf8(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxwEventAttributesUtf8_t attr)
{
    nvtxwEventUtf8Payload_t header;

    if (!writer)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(&header, 0, sizeof(header));
    header.timestamp = timestamp;
    header.attr.color = attr.color;
    header.attr.category = attr.category;

    return nvtxwEventHelperImplWriteUtf8(
        writer->iface,
        writer->stream,
        writer->schemaIds.rangePushUtf8,
        writer->schemaIds.eventMessageUtf8,
        &header,
        sizeof(header),
        attr.message,
        attr.messageLength);
}

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangePushWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxwEventAttributes_t attr)
{
    nvtxwEventRegStringPayload_t payload;

    if (!writer)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(&payload, 0, sizeof(payload));
    payload.timestamp = timestamp;
    payload.attr = attr;

    return nvtxwEventHelperImplWritePayload(
        writer->iface,
        writer->stream,
        writer->schemaIds.rangePushRegString,
        &payload,
        sizeof(payload));
}

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangePopWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp)
{
    nvtxwRangePopPayload_t payload;

    if (!writer)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(&payload, 0, sizeof(payload));
    payload.timestamp = timestamp;

    return nvtxwEventHelperImplWritePayload(
        writer->iface,
        writer->stream,
        writer->schemaIds.rangePop,
        &payload,
        sizeof(payload));
}

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangeStartWriteUtf8(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxRangeId_t rangeId,
    nvtxwEventAttributesUtf8_t attr)
{
    nvtxwRangeIdEventUtf8Payload_t header;

    if (!writer)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(&header, 0, sizeof(header));
    header.rangeId = rangeId;
    header.timestamp = timestamp;
    header.attr.color = attr.color;
    header.attr.category = attr.category;

    return nvtxwEventHelperImplWriteUtf8(
        writer->iface,
        writer->stream,
        writer->schemaIds.rangeStartUtf8,
        writer->schemaIds.eventMessageUtf8,
        &header,
        sizeof(header),
        attr.message,
        attr.messageLength);
}

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangeStartWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxRangeId_t rangeId,
    nvtxwEventAttributes_t attr)
{
    nvtxwRangeIdEventRegStringPayload_t payload;

    if (!writer)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(&payload, 0, sizeof(payload));
    payload.rangeId = rangeId;
    payload.timestamp = timestamp;
    payload.attr = attr;

    return nvtxwEventHelperImplWritePayload(
        writer->iface,
        writer->stream,
        writer->schemaIds.rangeStartRegString,
        &payload,
        sizeof(payload));
}

NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwRangeEndWrite(
    const nvtxwEventWriter_t* writer,
    int64_t timestamp,
    nvtxRangeId_t rangeId)
{
    nvtxwRangeEndPayload_t payload;

    if (!writer)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(&payload, 0, sizeof(payload));
    payload.rangeId = rangeId;
    payload.timestamp = timestamp;

    return nvtxwEventHelperImplWritePayload(
        writer->iface,
        writer->stream,
        writer->schemaIds.rangeEnd,
        &payload,
        sizeof(payload));
}

/** \endcond */

#ifdef __cplusplus
}
#endif

#endif
