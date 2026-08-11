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

/*
 * End-to-end coverage for the NVTXW helper headers.  Unlike CoverageNvtxw
 * (breadth/call counts against a stub), this drives the full helper flow against
 * a recording mock backend obtained via its load entry point, covering:
 *   - the setup/lifecycle helpers (session begin/end, domain register, scope
 *     register via stream open, time-domain register, stream open/close), and
 *   - the data that reached the backend: schema layouts, event message bytes
 *     (including explicit, non-null-terminated lengths), and counter samples,
 *     verified byte-for-byte.
 *
 * C only; the helpers' C++ compile coverage comes from the CoverageNvtxw build.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <nvtx3/nvToolsExt.h>

#include "MockNvtxwBackend.h"
#include <nvtxw3/nvtxw3_helpers.h>

/* Reinterpret the first 8 recorded counter bytes as the requested type. */
static int64_t MockCounterAsInt64(const MockNvtxwCounterRecord* rec)
{
    int64_t value = 0;
    memcpy(&value, rec->data, sizeof(value));
    return value;
}

static double MockCounterAsFloat64(const MockNvtxwCounterRecord* rec)
{
    double value = 0.0;
    memcpy(&value, rec->data, sizeof(value));
    return value;
}

NVTX_DYNAMIC_EXPORT
extern int RunTest(int argc, const char** argv);
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    const void* ifacePtr = NULL;
    const nvtxwInterface_t* iface = NULL;
    MockNvtxwRecorder* rec;

    nvtxwSessionAttributes_t sessionAttr;
    nvtxwSessionHandle_t session = NULL;
    nvtxwStreamHandle_t stream = NULL;
    nvtxDomainHandle_t domain = NULL;
    uint64_t timeDomainId = 0;

    nvtxwEventHelperSchemaIds_t schemaIds;
    nvtxwEventWriter_t writer;
    nvtxwEventWriter_t writerNoIface;
    nvtxwEventAttributesUtf8_t utf8Attr;
    uint64_t intCounterId = 0;
    uint64_t floatCounterId = 0;
    const char* expectedMsg = "coverage";
    size_t expectedMsgLen;

    (void)argc;
    (void)argv;

    expectedMsgLen = strlen(expectedMsg);
    MockNvtxwReset();
    rec = MockNvtxwGetRecorder();

    /* Obtain the interface as a loader would: resolve the single backend entry
     * point and request the core interface table by ID. */
    if (MockNvtxwGetInterface(NVTXW_INTERFACE_VERSION, &ifacePtr) != NVTXW_RESULT_SUCCESS ||
        !ifacePtr)
        return 1;
    iface = (const nvtxwInterface_t*)ifacePtr;

    /* Begin a session, then register a domain/scope and open a stream. */
    if (nvtxwSessionAttributesInit(&sessionAttr) != NVTXW_RESULT_SUCCESS) return 3;
    sessionAttr.name = "e2e";
    if (iface->SessionBegin(&sessionAttr, &session) != NVTXW_RESULT_SUCCESS) return 4;

    if (nvtxwTimeDomainRegister(
            iface, NULL, NVTX_TIMESTAMP_TYPE_NONE, &timeDomainId)
        != NVTXW_RESULT_INVALID_ARGUMENT)
        return 5; /* setup helper must reject NULL domain */

    /* Register the domain once; its handle is reused for schema/counter
     * registration and for opening the stream. */
    if (nvtxwDomainRegister(iface, session, "domain", &domain)
        != NVTXW_RESULT_SUCCESS)
        return 6;

    if (nvtxwStreamOpen(
            iface, session, "stream", domain, "scope/path",
            NVTX_TIME_DOMAIN_ID_NONE, &stream)
        != NVTXW_RESULT_SUCCESS)
        return 7;

    if (rec->domainRegisterCalls != 1) return 8;
    if (rec->scopeRegisterCalls != 1) return 9;
    if (rec->streamOpenCalls != 1) return 47;

    /* Resource registration has no setup helper, so drive the interface
     * directly: reject bad arguments, then confirm a valid call is recorded. */
    {
        nvtxResourceAttributes_t resourceAttr;
        memset(&resourceAttr, 0, sizeof(resourceAttr));
        resourceAttr.version = NVTX_VERSION;
        resourceAttr.size = NVTX_RESOURCE_ATTRIB_STRUCT_SIZE;
        resourceAttr.identifierType = NVTX_RESOURCE_TYPE_GENERIC_POINTER;
        resourceAttr.identifier.pValue = (const void*)&resourceAttr;
        resourceAttr.messageType = NVTX_MESSAGE_TYPE_ASCII;
        resourceAttr.message.ascii = "resource";

        if (iface->ResourceRegister(NULL, &resourceAttr)
            != NVTXW_RESULT_INVALID_ARGUMENT)
            return 48;
        if (iface->ResourceRegister(domain, NULL)
            != NVTXW_RESULT_INVALID_ARGUMENT)
            return 49;
        if (iface->ResourceRegister(domain, &resourceAttr)
            != NVTXW_RESULT_SUCCESS)
            return 50;
        if (rec->resourceRegisterCalls != 1) return 51;
    }

    /* Category naming: the helper must validate its arguments, and a valid call
     * must reach the backend with the ID and name intact. */
    {
        const char* categoryName = "Memory Transfer";

        if (nvtxwCategoryRegister(NULL, domain, 7, categoryName)
            != NVTXW_RESULT_INVALID_ARGUMENT) return 61;
        if (nvtxwCategoryRegister(iface, NULL, 7, categoryName)
            != NVTXW_RESULT_INVALID_ARGUMENT) return 62;
        if (nvtxwCategoryRegister(iface, domain, 7, NULL)
            != NVTXW_RESULT_INVALID_ARGUMENT) return 63;
        if (nvtxwCategoryRegister(iface, domain, 0, categoryName)
            != NVTXW_RESULT_INVALID_ARGUMENT) return 69;

        if (iface->CategoryRegister(NULL, 7, categoryName)
            != NVTXW_RESULT_INVALID_ARGUMENT) return 64;

        if (nvtxwCategoryRegister(iface, domain, 7, categoryName)
            != NVTXW_RESULT_SUCCESS) return 65;
        if (rec->categoryRegisterCalls != 1) return 66;
        if (rec->lastCategoryId != 7) return 67;
        if (strcmp(rec->lastCategoryName, categoryName) != 0) return 68;
    }

    /* Exercise the time-domain helper against the backend. */
    if (nvtxwTimeDomainRegister(
            iface, domain, NVTX_TIMESTAMP_TYPE_NONE, &timeDomainId)
        != NVTXW_RESULT_SUCCESS)
        return 10;
    if (rec->timeDomainRegisterCalls != 1) return 11;

    /* Register all event schemas; confirm a well-formed UTF-8 message schema. */
    if (nvtxwEventSchemasRegister(
            iface, domain, NVTXW_EVENT_HELPER_SCHEMA_ALL, &schemaIds)
        != NVTXW_RESULT_SUCCESS)
        return 12;

    if (nvtxwEventWriterInit(&writer, iface, stream, &schemaIds)
        != NVTXW_RESULT_SUCCESS)
        return 52;

    /* The UTF-8 event message must travel as its own standalone payload: exactly
     * one registered schema carries the standalone UTF-8 event-message entry,
     * which must be a single zero-terminated, inline UTF-8 string in a dynamic schema. */
    {
        int i;
        int messageSchemas = 0;
        for (i = 0; i < rec->schemaCount; ++i)
        {
            const MockNvtxwSchemaRecord* sc = &rec->schemas[i];
            if (!sc->hasEventMessage) continue;
            if (sc->messageEntryType != NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF8) continue;
            ++messageSchemas;
            if (sc->messageEntryFlags & NVTX_PAYLOAD_ENTRY_FLAG_POINTER) return 13;
            if (sc->messageEntryFlags & NVTX_PAYLOAD_ENTRY_FLAG_DEEP_COPY) return 13;
            if ((sc->messageEntryFlags & NVTX_PAYLOAD_ENTRY_FLAG_IS_ARRAY) !=
                NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED) return 13;
            if (sc->schemaType != NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC) return 13;
            if (sc->numEntries != 1) return 13;
        }
        if (messageSchemas != 1) return 13;
    }

    /* 12 event schemas plus the shared standalone UTF-8 message schema. */
    if (rec->schemaRegisterCalls != 13) return 14;

    /* Each UTF-8 write must deliver the message verbatim as a second payload. */
    memset(&utf8Attr, 0, sizeof(utf8Attr));
    utf8Attr.color = 0xFF00FF00u;
    utf8Attr.category = 1;
    utf8Attr.message = expectedMsg;

    if (nvtxwMarkWriteUtf8(&writer, 10, utf8Attr)
        != NVTXW_RESULT_SUCCESS) return 16;
    if (nvtxwRangePushPopWriteUtf8(&writer, 10, 20, utf8Attr)
        != NVTXW_RESULT_SUCCESS) return 17;
    if (nvtxwRangeStartEndWriteUtf8(&writer, 10, 20, utf8Attr)
        != NVTXW_RESULT_SUCCESS) return 18;

    if (rec->eventWriteCalls != 3) return 19;
    {
        int i;
        for (i = 0; i < rec->eventCount; ++i)
        {
            const MockNvtxwEventRecord* ev = &rec->events[i];
            if (!ev->hasMessagePayload) return 20;
            if (ev->messageSize != expectedMsgLen) return 21;
            if (memcmp(ev->messageBytes, expectedMsg, expectedMsgLen) != 0) return 22;
            if (ev->headerSchemaId == 0) return 23;
        }
    }

    /* An explicit messageLength sends exactly that many bytes (no strlen, no
     * terminator): 8 bytes of a longer buffer must yield exactly "coverage". */
    utf8Attr.message = "coverage-plus-trailing-bytes";
    utf8Attr.messageLength = (uint32_t)expectedMsgLen;
    if (nvtxwMarkWriteUtf8(&writer, 30, utf8Attr)
        != NVTXW_RESULT_SUCCESS) return 24;
    utf8Attr.messageLength = 0;
    {
        const MockNvtxwEventRecord* ev = &rec->events[rec->eventCount - 1];
        if (ev->messageSize != expectedMsgLen) return 25;
        if (memcmp(ev->messageBytes, expectedMsg, expectedMsgLen) != 0) return 26;
    }

    /* Counters: values must arrive byte-for-byte. */
    if (nvtxwCounterInt64Register(iface, domain, "int-counter", NULL, &intCounterId)
        != NVTXW_RESULT_SUCCESS) return 27;
    if (nvtxwCounterInt64Write(iface, stream, 1, intCounterId, 42)
        != NVTXW_RESULT_SUCCESS) return 28;
    if (nvtxwCounterFloat64Register(iface, domain, "double-counter", NULL, &floatCounterId)
        != NVTXW_RESULT_SUCCESS) return 29;
    if (nvtxwCounterFloat64Write(iface, stream, 2, floatCounterId, 3.5)
        != NVTXW_RESULT_SUCCESS) return 30;

    if (rec->counterRegisterCalls != 2) return 31;
    if (rec->counterWriteCalls != 2) return 32;
    if (rec->counterCount != 2) return 33;

    if (rec->counters[0].counterId != intCounterId) return 34;
    if (rec->counters[0].timestamp != 1) return 35;
    if (rec->counters[0].size != sizeof(int64_t)) return 36;
    if (MockCounterAsInt64(&rec->counters[0]) != 42) return 37;

    if (rec->counters[1].counterId != floatCounterId) return 38;
    if (rec->counters[1].timestamp != 2) return 39;
    if (rec->counters[1].size != sizeof(double)) return 40;
    if (MockCounterAsFloat64(&rec->counters[1]) != 3.5) return 41;

    /* A missing interface must be rejected by the helpers before any call. */
    if (nvtxwEventWriterInit(&writerNoIface, NULL, stream, &schemaIds)
        != NVTXW_RESULT_SUCCESS) return 53;
    if (nvtxwMarkWriteUtf8(&writerNoIface, 0, utf8Attr)
        != NVTXW_RESULT_INVALID_ARGUMENT) return 42;

    /* The convenience constructor opens a stream, registers all event schemas,
     * and returns a writer that drives a write end-to-end in one call. */
    {
        nvtxwEventWriter_t convWriter;
        const int schemasBefore = rec->schemaRegisterCalls;
        const int eventsBefore = rec->eventWriteCalls;
        if (nvtxwEventWriterOpen(
                iface, session, "conv-stream", domain, NULL,
                NVTX_TIME_DOMAIN_ID_NONE, &convWriter)
            != NVTXW_RESULT_SUCCESS) return 54;
        /* It registers the full schema set (12 event schemas + shared UTF-8). */
        if (rec->schemaRegisterCalls != schemasBefore + 13) return 55;
        if (convWriter.iface != iface) return 56;
        utf8Attr.message = expectedMsg;
        utf8Attr.messageLength = 0;
        if (nvtxwMarkWriteUtf8(&convWriter, 70, utf8Attr)
            != NVTXW_RESULT_SUCCESS) return 57;
        if (rec->eventWriteCalls != eventsBefore + 1) return 58;
    }
    /* A NULL writer output must be rejected. */
    if (nvtxwEventWriterOpen(
            iface, session, "x", domain, NULL,
            NVTX_TIME_DOMAIN_ID_NONE, NULL)
        != NVTXW_RESULT_INVALID_ARGUMENT) return 59;

    if (iface->StreamClose(stream) != NVTXW_RESULT_SUCCESS) return 43;
    if (iface->SessionEnd(session) != NVTXW_RESULT_SUCCESS) return 44;
    if (rec->streamCloseCalls != 1) return 45;
    if (rec->sessionEndCalls != 1) return 46;

    return 0;
}
