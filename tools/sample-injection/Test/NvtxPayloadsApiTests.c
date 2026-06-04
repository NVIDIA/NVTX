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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_US(us) Sleep((us) / 1000 > 0 ? (us) / 1000 : 1)
#else
#include <unistd.h>
#define SLEEP_US(us) usleep(us)
#endif

#include <nvtx3/nvToolsExtPayload.h>
#include <nvtx3/nvToolsExtPayloadHelper.h>

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

static nvtxEventAttributes_t GetNvtxEventAttributes(nvtxDomainHandle_t domain, const char* message)
{
    nvtxEventAttributes_t a;
    memset(&a, 0, sizeof(a));
    a.version = NVTX_VERSION;
    a.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
    a.messageType = NVTX_MESSAGE_TYPE_REGISTERED;
    a.message.registered = nvtxDomainRegisterStringA(domain, message);
    return a;
}

/* Demonstrate payload helper macros for schema definition, registration,
 * and attaching payloads to events.*/
static int PayloadHelperMacros(nvtxDomainHandle_t domain)
{
    typedef struct CounterPayload
    {
        uint32_t counter;
        float measurement;
    } CounterPayload;

    NVTX_DEFINE_SCHEMA_FOR_STRUCT_AND_REGISTER(
        domain,
        CounterPayload,
        "CounterPayloadSchema",
        NVTX_PAYLOAD_ENTRIES(
            (counter, TYPE_UINT32, "Counter"), (measurement, TYPE_FLOAT, "Measurement")))

    NVTX_DEFINE_STRUCT_WITH_SCHEMA_AND_REGISTER(
        domain,
        SensorReading,
        "SensorReadingSchema",
        NVTX_PAYLOAD_ENTRIES(
            (int32_t, sensorId, TYPE_INT32, "SensorId"),
            (float, temperature, TYPE_FLOAT, "Temperature"),
            (uint8_t, channelId, TYPE_UINT8, "ChannelId")))

    if (CounterPayload_schemaId == 0 || SensorReading_schemaId == 0)
    {
        fprintf(stderr, "PayloadHelperMacros: failed to register schemas\n");
        return 1;
    }

    { /* nvtxPayloadMark wraps nvtxDomainMarkEx with one payload. */
        CounterPayload pl = {1, 1.5f};
        nvtxEventAttributes_t evtAttr = GetNvtxEventAttributes(domain, "PayloadMark");
        nvtxPayloadMark(domain, &evtAttr, CounterPayload_schemaId, &pl, sizeof(pl));
    }

    { /* nvtxPayloadRangePush wraps nvtxDomainRangePushEx with one payload. */
        CounterPayload rec = {2, 2.718f};
        nvtxEventAttributes_t evtAttr = GetNvtxEventAttributes(domain, "PayloadRangePush");
        nvtxPayloadRangePush(domain, &evtAttr, CounterPayload_schemaId, &rec, sizeof(rec));
        SLEEP_US(100);
        nvtxDomainRangePop(domain);
    }

    { /* nvtxRangeStartPayload wraps nvtxDomainRangeStartEx with one payload. */
        CounterPayload pl = {3, 3.5f};
        nvtxPayloadData_t data = {CounterPayload_schemaId, sizeof(pl), &pl};
        nvtxRangeId_t rangeId = nvtxRangeStartPayload(domain, &data, 1);
        SLEEP_US(100);
        nvtxRangeEndPayload(domain, rangeId, &data, 1);
    }

    { /* Attaching multiple payloads using NVTX_PAYLOAD_EVTATTR_SET_MULTIPLE. */
        CounterPayload cRec = {4, 1.618f};
        SensorReading srRec = {8, 100.0f, 4};
        nvtxPayloadData_t data[] = {
            {CounterPayload_schemaId, sizeof(cRec), &cRec},
            {SensorReading_schemaId, sizeof(srRec), &srRec}};
        nvtxEventAttributes_t evtAttr = GetNvtxEventAttributes(domain, "MultiPayloadPushPop");
        NVTX_PAYLOAD_EVTATTR_SET_MULTIPLE(evtAttr, data)
        nvtxDomainRangePushEx(domain, &evtAttr);
        SLEEP_US(100);
        nvtxDomainRangePop(domain);
    }

    return 0;
}

/* Demonstrate nvtx*Payload APIs that accept nvtxPayloadData_t. */
static int PayloadApis(nvtxDomainHandle_t domain)
{
    typedef struct EventPayload
    {
        nvtxStringHandle_t message;
        int32_t id;
        double value;
    } EventPayload;

    nvtxPayloadSchemaEntry_t entries[] = {
        {NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE,
         NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE,
         "Message",
         "Event message handle"},
        {0, NVTX_PAYLOAD_ENTRY_TYPE_INT32, "Id", "Unique event identifier"},
        {0, NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE, "Value", "Measured quantity"}};

    nvtxPayloadSchemaAttr_t attr;
    attr.fieldMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE;
    attr.name = "EventPayloadSchema";
    attr.type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC;
    attr.entries = entries;
    attr.numEntries = ARRAY_COUNT(entries);
    attr.payloadStaticSize = sizeof(EventPayload);

    uint64_t schemaId = nvtxPayloadSchemaRegister(domain, &attr);
    if (schemaId == 0)
    {
        fprintf(stderr, "PayloadApis: failed to register schema\n");
        return 1;
    }

    {
        nvtxStringHandle_t msgMark = nvtxDomainRegisterStringA(domain, "MarkPayload");
        EventPayload pl = {msgMark, 10, 1.5};
        nvtxPayloadData_t data = {schemaId, sizeof(pl), &pl};
        nvtxMarkPayload(domain, &data, 1);
    }

    {
        nvtxStringHandle_t msgPush = nvtxDomainRegisterStringA(domain, "PushPayload");
        EventPayload pl = (EventPayload){msgPush, 20, 2.5};
        nvtxPayloadData_t data = {schemaId, sizeof(pl), &pl};
        nvtxRangePushPayload(domain, &data, 1);
        SLEEP_US(100);
        nvtxRangePopPayload(domain, NULL, 0);
    }

    {
        nvtxRangeId_t rangeId = nvtxRangeStartPayload(domain, NULL, 0);
        SLEEP_US(100);
        nvtxStringHandle_t msgEnd = nvtxDomainRegisterStringA(domain, "MsgInEndPayload");
        EventPayload pl = (EventPayload){msgEnd, 31, 3.75};
        nvtxPayloadData_t data = {schemaId, sizeof(pl), &pl};
        nvtxRangeEndPayload(domain, rangeId, &data, 1);
    }

    return 0;
}

/* Nested schema with a nested struct, a flag enum, and a plain enum.
 * Also tests unknown enum values falling through to numeric output. */
static int NestedSchemas(nvtxDomainHandle_t domain)
{
    /* Define nested struct and schema. */
    typedef struct SourceInfo
    {
        int32_t line;
        int32_t column;
    } SourceInfo;

    nvtxPayloadSchemaEntry_t innerEntries[] = {
        {0, NVTX_PAYLOAD_ENTRY_TYPE_INT32, "line"}, {0, NVTX_PAYLOAD_ENTRY_TYPE_INT32, "column"}};

    /* Schema name is optional; omitted here since the outer entry name suffices. */
    nvtxPayloadSchemaAttr_t schemaAttr;
    schemaAttr.fieldMask =
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE;
    schemaAttr.type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC;
    schemaAttr.entries = innerEntries;
    schemaAttr.numEntries = ARRAY_COUNT(innerEntries);
    schemaAttr.payloadStaticSize = sizeof(SourceInfo);
    uint64_t innerSchemaId = nvtxPayloadSchemaRegister(domain, &schemaAttr);

    /* Flag enum (bitwise combinable). */
    typedef enum AccessMode
    {
        ACCESS_READ = (1 << 0),
        ACCESS_WRITE = (1 << 1),
        ACCESS_EXECUTE = (1 << 2)
    } AccessMode;

    nvtxPayloadEnum_t accessValues[] = {
        {"Read", ACCESS_READ, 1}, {"Write", ACCESS_WRITE, 1}, {"Execute", ACCESS_EXECUTE, 1}};

    nvtxPayloadEnumAttr_t enumAttr;
    enumAttr.fieldMask = NVTX_PAYLOAD_ENUM_ATTR_FIELD_NAME | NVTX_PAYLOAD_ENUM_ATTR_FIELD_ENTRIES |
                         NVTX_PAYLOAD_ENUM_ATTR_FIELD_NUM_ENTRIES |
                         NVTX_PAYLOAD_ENUM_ATTR_FIELD_SIZE;
    enumAttr.name = "AccessModeFlags";
    enumAttr.entries = accessValues;
    enumAttr.numEntries = ARRAY_COUNT(accessValues);
    enumAttr.sizeOfEnum = sizeof(AccessMode);
    uint64_t accessEnumId = nvtxPayloadEnumRegister(domain, &enumAttr);

    /* Plain enum (exact-match lookup). */
    typedef enum Priority
    {
        PRIORITY_LOW = 0,
        PRIORITY_HIGH = 1
    } Priority;

    nvtxPayloadEnum_t priorityValues[] = {{"Low", PRIORITY_LOW, 0}, {"High", PRIORITY_HIGH, 0}};

    enumAttr.name = "PriorityEnum";
    enumAttr.entries = priorityValues;
    enumAttr.numEntries = ARRAY_COUNT(priorityValues);
    enumAttr.sizeOfEnum = sizeof(Priority);

    uint64_t priorityEnumId = nvtxPayloadEnumRegister(domain, &enumAttr);

    if (innerSchemaId == 0 || accessEnumId == 0 || priorityEnumId == 0)
    {
        fprintf(stderr, "NestedSchemas: failed to register inner types\n");
        return 1;
    }

    typedef struct TaskRecord
    {
        AccessMode access;
        Priority priority;
        SourceInfo source;
        double progress;
    } TaskRecord;

    nvtxPayloadSchemaEntry_t entries[] = {
        {0, accessEnumId, "access"},
        {0, priorityEnumId, "priority"},
        {0, innerSchemaId, "source"},
        {0, NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE, "progress"}};

    schemaAttr.fieldMask |= NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME;
    schemaAttr.name = "TaskRecordSchema";
    schemaAttr.entries = entries;
    schemaAttr.numEntries = ARRAY_COUNT(entries);
    schemaAttr.payloadStaticSize = sizeof(TaskRecord);

    uint64_t schemaId = nvtxPayloadSchemaRegister(domain, &schemaAttr);
    if (schemaId == 0)
    {
        fprintf(stderr, "NestedSchemas: failed to register schema\n");
        return 1;
    }

    /* Known enum values for both flag and plain enums. */
    {
        TaskRecord rec = {ACCESS_READ | ACCESS_WRITE, PRIORITY_HIGH, {42, 10}, 0.75};
        nvtxPayloadData_t data = {schemaId, sizeof(rec), &rec};
        nvtxMarkPayload(domain, &data, 1);
    }

    /* Unknown plain enum value: numeric fallback. */
    {
        TaskRecord rec = {ACCESS_EXECUTE, (Priority)99, {1, 1}, 0.5};
        nvtxPayloadData_t data = {schemaId, sizeof(rec), &rec};
        nvtxMarkPayload(domain, &data, 1);
    }

    return 0;
}

/* Dynamic schema with a length-indexed array: the first field ("count")
 * specifies the number of elements in the second field ("values"). */
static int LengthPrefixedArray(nvtxDomainHandle_t domain)
{
    nvtxPayloadSchemaEntry_t entries[] = {
        {0, NVTX_PAYLOAD_ENTRY_TYPE_INT32, "count"},
        {NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX, NVTX_PAYLOAD_ENTRY_TYPE_INT32, "values"}};

    nvtxPayloadSchemaAttr_t attr;
    attr.fieldMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES;
    attr.name = "DynamicLengthIndexedSchema";
    attr.type = NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC;
    attr.entries = entries;
    attr.numEntries = ARRAY_COUNT(entries);

    uint64_t schemaId = nvtxPayloadSchemaRegister(domain, &attr);
    if (schemaId == 0)
    {
        fprintf(stderr, "LengthPrefixedArray: failed to register schema\n");
        return 1;
    }

    {
        int32_t payload[] = {4, 11, 22, 33, 44};
        nvtxPayloadData_t data = {schemaId, sizeof(payload), payload};
        nvtxMarkPayload(domain, &data, 1);
    }

    return 0;
}

/* Event message as a flexible array member: dynamic schema with length-prefixed
 * C string as the event message. Uses nvtx*Payload APIs (no helper macros). */
static int EventMessageFlexibleArrayMember(nvtxDomainHandle_t domain)
{
    /* Schema: length (int32) then variable-length message (C string, length from entry 0). */
    nvtxPayloadSchemaEntry_t entries[] = {
        {0, NVTX_PAYLOAD_ENTRY_TYPE_INT32, "length"},
        {NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX | NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE,
         NVTX_PAYLOAD_ENTRY_TYPE_CSTRING,
         "message"}};

    nvtxPayloadSchemaAttr_t attr;
    attr.fieldMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES;
    attr.name = "EventMessageFamSchema";
    attr.type = NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC;
    attr.entries = entries;
    attr.numEntries = ARRAY_COUNT(entries);

    uint64_t schemaId = nvtxPayloadSchemaRegister(domain, &attr);
    if (schemaId == 0)
    {
        fprintf(stderr, "EventMessageFlexibleArrayMember: failed to register schema\n");
        return 1;
    }

    {
        const char* msg = "Hello from FAM";
        const int32_t length = (int32_t)strlen(msg);
        size_t payloadSize = (size_t)(sizeof(length) + length);
        char payloadBuf[64];
        if (payloadSize > sizeof(payloadBuf))
        {
            fprintf(stderr, "EventMessageFlexibleArrayMember: payload too large\n");
            return 1;
        }
        memcpy(payloadBuf, &length, sizeof(length));
        memcpy(payloadBuf + sizeof(length), msg, (size_t)length);

        nvtxPayloadData_t data = {schemaId, payloadSize, payloadBuf};
        nvtxMarkPayload(domain, &data, 1);
    }

    /* Same idea with a zero-terminated string as the event message (no length prefix). */
    nvtxPayloadSchemaEntry_t zeroTermEntries[] = {
        {NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED | NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE,
         NVTX_PAYLOAD_ENTRY_TYPE_CSTRING,
         "message"}};

    attr.name = "EventMessageFamSchemaZeroTerm";
    attr.type = NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC;
    attr.entries = zeroTermEntries;
    attr.numEntries = ARRAY_COUNT(zeroTermEntries);

    uint64_t schemaIdZeroTerm = nvtxPayloadSchemaRegister(domain, &attr);
    if (schemaIdZeroTerm == 0)
    {
        fprintf(stderr, "EventMessageFlexibleArrayMember: failed to register zero-term schema\n");
        return 1;
    }

    {
        const char* msgZeroTerm = "Zero-term FAM";
        size_t payloadSizeZeroTerm = strlen(msgZeroTerm) + 1u;
        nvtxPayloadData_t data = {schemaIdZeroTerm, payloadSizeZeroTerm, msgZeroTerm};
        nvtxMarkPayload(domain, &data, 1);
    }

    return 0;
}

/* Fixed-size array of C-string pointers via a nested schema.
 *
 * The inner CStringWrapper schema is registered manually (no helper macros).
 * The outer schema references it as a fixed-size array. */
static int FixedSizeArrayOfStrings(nvtxDomainHandle_t domain)
{
#define FIXED_STRING_SIZE 6
    typedef struct CStringWrapper
    {
        const char str[FIXED_STRING_SIZE]; /* Fixed-size string. */
    } CStringWrapper;

    nvtxPayloadSchemaEntry_t stringWrapSchema[] = {
        {0, NVTX_PAYLOAD_ENTRY_TYPE_CSTRING, "str", NULL, FIXED_STRING_SIZE}};

    nvtxPayloadSchemaAttr_t attr;
    attr.fieldMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE;
    attr.name = "CStringWrapper";
    attr.type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC;
    attr.entries = stringWrapSchema;
    attr.numEntries = ARRAY_COUNT(stringWrapSchema);
    attr.payloadStaticSize = sizeof(CStringWrapper);

    uint64_t innerSchemaId = nvtxPayloadSchemaRegister(domain, &attr);
    if (innerSchemaId == 0)
    {
        fprintf(stderr, "FixedSizeArrayOfStrings: failed to register inner schema\n");
        return 1;
    }

    /* Define and register the actual payload structure and register its schema. */
    typedef struct StringArrayPayload
    {
        uint64_t value;
        CStringWrapper text[3]; /* Array of 3 CStringWrapper (inline fixed-size strings). */
    } StringArrayPayload;

    nvtxPayloadSchemaEntry_t stringArraySchema[] = {
        {0, NVTX_PAYLOAD_ENTRY_TYPE_UINT64, "value"},
        {NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE, innerSchemaId, "text", NULL, 3}};

    /* Reuse schema attributes from CStringWrapper schema. */
    attr.name = "StringArraySchema";
    attr.entries = stringArraySchema;
    attr.numEntries = ARRAY_COUNT(stringArraySchema);
    attr.payloadStaticSize = sizeof(StringArrayPayload);

    uint64_t schemaId = nvtxPayloadSchemaRegister(domain, &attr);
    if (schemaId == 0)
    {
        fprintf(stderr, "FixedSizeArrayOfStrings: failed to register outer schema\n");
        return 1;
    }

    /* Emit the event with payload (inline fixed-size strings). */
    {
        StringArrayPayload rec = {17, {{"hello"}, {"world"}, {"test"}}};
        nvtxPayloadData_t data = {schemaId, sizeof(rec), &rec};
        nvtxMarkPayload(domain, &data, 1);
    }

    return 0;
}

/* Predefined-type payloads: the schemaId in nvtxPayloadData_t is set to a
 * predefined entry type (< NVTX_PAYLOAD_SCHEMA_ID_STATIC_START).  The parser
 * decodes these directly without schema registration. For single numeric values
 * this does not make a lot of sense, since regular payloads are more efficient. */
static int PredefinedTypePayloads(nvtxDomainHandle_t domain)
{
    /* Single double value. */
    {
        double value = 3.14159;
        nvtxEventAttributes_t evtAttr = GetNvtxEventAttributes(domain, "PredefinedDouble");
        nvtxPayloadMark(domain, &evtAttr, NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE, &value, sizeof(value));
    }

    /* Inline C string (payload bytes are the string characters, not a pointer). */
    {
        const char* str = "Hello predefined";
        nvtxEventAttributes_t evtAttr = GetNvtxEventAttributes(domain, "PredefinedCString");
        nvtxPayloadMark(domain, &evtAttr, NVTX_PAYLOAD_ENTRY_TYPE_CSTRING, str, strlen(str) + 1);
    }

    /* Empty C string: encoded as a single null code unit (size = 1 byte for CSTRING),
     * not size 0. The parser reads the one null byte and decodes it as "". */
    {
        nvtxEventAttributes_t evtAttr = GetNvtxEventAttributes(domain, "PredefinedEmptyCString");
        nvtxPayloadMark(domain, &evtAttr, NVTX_PAYLOAD_ENTRY_TYPE_CSTRING, "", 1);
    }

    /* Array of int32: payload size is a multiple of element size, so the parser
     * auto-detects it as a fixed-size array of 3 elements. */
    {
        int32_t values[] = {10, 20, 30};
        nvtxEventAttributes_t evtAttr = GetNvtxEventAttributes(domain, "PredefinedInt32Array");
        nvtxPayloadMark(domain, &evtAttr, NVTX_PAYLOAD_ENTRY_TYPE_INT32, values, sizeof(values));
    }

    return 0;
}

/* Null-terminated C string with size set to SIZE_MAX: the handler determines
 * the payload size by scanning for the null terminator. */
static int NullTerminatedStringSizeMax(nvtxDomainHandle_t domain)
{
    const char* str = "SizeMax CString";
    nvtxEventAttributes_t evtAttr = GetNvtxEventAttributes(domain, "SizeMaxCString");
    nvtxPayloadMark(domain, &evtAttr, NVTX_PAYLOAD_ENTRY_TYPE_CSTRING, str, (size_t)(-1));

    return 0;
}

/* Hex-bytes types: ADDRESS (pointer) and BYTE are both rendered as hex. */
static int HexBytesTypes(nvtxDomainHandle_t domain)
{
    typedef struct HexPayload
    {
        void* ptr;
        uint8_t flags;
    } HexPayload;

    nvtxPayloadSchemaEntry_t entries[] = {
        {0, NVTX_PAYLOAD_ENTRY_TYPE_ADDRESS, "ptr"}, {0, NVTX_PAYLOAD_ENTRY_TYPE_BYTE, "flags"}};

    nvtxPayloadSchemaAttr_t attr;
    attr.fieldMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE;
    attr.name = "HexPayloadSchema";
    attr.type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC;
    attr.entries = entries;
    attr.numEntries = ARRAY_COUNT(entries);
    attr.payloadStaticSize = sizeof(HexPayload);

    uint64_t schemaId = nvtxPayloadSchemaRegister(domain, &attr);
    if (schemaId == 0)
    {
        fprintf(stderr, "HexBytesTypes: failed to register schema\n");
        return 1;
    }

    {
        HexPayload pl = {(void*)0x00007FFF12345678ULL, 0xAB};
        nvtxPayloadData_t data = {schemaId, sizeof(pl), &pl};
        nvtxMarkPayload(domain, &data, 1);
    }

    return 0;
}

/* Schema with pack alignment (NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ALIGNMENT).
 * Implicit offsets use packAlign to cap member alignment, matching #pragma pack.
 *
 * Natural alignment:  float(0) + pad(4) + double(8) = 16 bytes.
 * packAlign=4:        float(0) + double(4) = 12 bytes. */
static int PackAlignSchema(nvtxDomainHandle_t domain)
{
#pragma pack(push, 4)
    typedef struct Packed4Payload
    {
        float a;
        double b;
    } Packed4Payload;
#pragma pack(pop)

    nvtxPayloadSchemaEntry_t entries[] = {
        {0, NVTX_PAYLOAD_ENTRY_TYPE_FLOAT, "a"}, {0, NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE, "b"}};

    nvtxPayloadSchemaAttr_t attr;
    attr.fieldMask =
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES |
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ALIGNMENT;
    attr.name = "Packed4Schema";
    attr.type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC;
    attr.entries = entries;
    attr.numEntries = ARRAY_COUNT(entries);
    attr.payloadStaticSize = sizeof(Packed4Payload);
    attr.packAlign = 4;

    uint64_t schemaId = nvtxPayloadSchemaRegister(domain, &attr);
    if (schemaId == 0)
    {
        fprintf(stderr, "PackAlignSchema: failed to register schema\n");
        return 1;
    }

    {
        Packed4Payload pl = {1.5f, 2.718281828};
        nvtxPayloadData_t data = {schemaId, sizeof(pl), &pl};
        nvtxMarkPayload(domain, &data, 1);
    }

    return 0;
}

/* Sparse struct with user-provided offsets: the schema entries use explicit
 * byte offsets to reach fields across reserved regions. The reserved bytes are
 * filled with 0xFF so that a parser bug that falls back to natural alignment
 * (uint8@0 -> double@8 -> int64@16) would read garbage instead of the real values. */
static int SparseStructWithOffsets(nvtxDomainHandle_t domain)
{
    typedef struct SparsePayload
    {
        uint8_t tag;
        char _reserved1[23];
        double value;
        char _reserved2[16];
        int64_t timestamp;
    } SparsePayload;

    nvtxPayloadSchemaEntry_t entries[] = {
        {0, NVTX_PAYLOAD_ENTRY_TYPE_UINT8, "tag", NULL, 0, offsetof(SparsePayload, tag)},
        {0, NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE, "value", NULL, 0, offsetof(SparsePayload, value)},
        {0,
         NVTX_PAYLOAD_ENTRY_TYPE_INT64,
         "timestamp",
         NULL,
         0,
         offsetof(SparsePayload, timestamp)}};

    nvtxPayloadSchemaAttr_t attr;
    attr.fieldMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES |
                     NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE;
    attr.name = "SparsePayloadSchema";
    attr.type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC;
    attr.entries = entries;
    attr.numEntries = ARRAY_COUNT(entries);
    attr.payloadStaticSize = sizeof(SparsePayload);

    uint64_t schemaId = nvtxPayloadSchemaRegister(domain, &attr);
    if (schemaId == 0)
    {
        fprintf(stderr, "SparseStructWithOffsets: failed to register schema\n");
        return 1;
    }

    {
        SparsePayload pl;
        memset(&pl, 0xFF, sizeof(pl));
        pl.tag = 66;
        pl.value = 3.14;
        pl.timestamp = 1234567890LL;
        nvtxPayloadData_t data = {schemaId, sizeof(pl), &pl};
        nvtxMarkPayload(domain, &data, 1);
    }

    return 0;
}

int main(void)
{
    nvtxDomainHandle_t domain = nvtxDomainCreateA("PayloadsDomain");

    int result = 0;

#define RUN_TEST(fn)                                                                               \
    do                                                                                             \
    {                                                                                              \
        fflush(stdout);                                                                            \
        printf("\n--- " #fn " ---\n");                                                             \
        fflush(stdout);                                                                            \
        result |= fn(domain);                                                                      \
    } while (0)

    RUN_TEST(PayloadHelperMacros);
    RUN_TEST(PayloadApis);
    RUN_TEST(NestedSchemas);
    RUN_TEST(LengthPrefixedArray);
    RUN_TEST(EventMessageFlexibleArrayMember);
    RUN_TEST(FixedSizeArrayOfStrings);
    RUN_TEST(PredefinedTypePayloads);
    RUN_TEST(NullTerminatedStringSizeMax);
    RUN_TEST(HexBytesTypes);
    RUN_TEST(PackAlignSchema);
    RUN_TEST(SparseStructWithOffsets);

#undef RUN_TEST

    nvtxDomainDestroy(domain);
    return result;
}
