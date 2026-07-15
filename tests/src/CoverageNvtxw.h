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
 * Surface/compile coverage for the NVTXW writer helper headers.  The NVTXW
 * helpers operate entirely through a backend-provided interface function table,
 * so a minimal stub table is enough to exercise the inline logic without loading
 * a real backend.  This test focuses on breadth and portability:
 *   - the inline config-string parser (nvtxwConsumeConfigString),
 *   - the attribute initializers (zero-fill, structSize, defaults, null reject),
 *   - every event- and counter-write helper variant, confirming each forwards to
 *     the right core interface member with valid arguments and the expected
 *     call counts, and
 *   - that the headers compile as both C and C++ (built as both languages).
 *
 * It deliberately does NOT cover the setup/lifecycle helpers (domain, scope,
 * stream, time-domain, session) or verify the data that reaches the backend;
 * the stub retains nothing.  Those require a recording backend and are the job
 * of CoverageNvtxwE2E.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <nvtx3/nvToolsExtPayload.h>
#include <nvtx3/nvToolsExtCounters.h>
#include <nvtxw3/nvtxw3_helpers.h>

typedef struct StubState
{
    int schemaRegisterCalls;
    int eventWriteCalls;
    int counterRegisterCalls;
    int counterWriteCalls;
    int categoryRegisterCalls;
    uint64_t nextId;
} StubState;

static StubState g_stub;

/* Stub backend: minimal implementations of the NVTXW interface callbacks that
 * the helpers call into.  Each validates its arguments, records that it was
 * called in g_stub, and hands back dummy IDs, without retaining the data. */

static nvtxwResultCode_t StubSchemaRegister(
    nvtxDomainHandle_t domain,
    const nvtxPayloadSchemaAttr_t* attr,
    uint64_t* schemaIdOut)
{
    (void)domain;
    if (!attr || !schemaIdOut) return NVTXW_RESULT_INVALID_ARGUMENT;

    ++g_stub.schemaRegisterCalls;
    *schemaIdOut = ++g_stub.nextId;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t StubEventWrite(
    nvtxwStreamHandle_t stream,
    const nvtxPayloadData_t* payloads,
    size_t payloadCount)
{
    (void)stream;
    if (!payloads || payloadCount == 0) return NVTXW_RESULT_INVALID_ARGUMENT;

    ++g_stub.eventWriteCalls;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t StubCounterRegister(
    nvtxDomainHandle_t domain,
    const nvtxCounterAttr_t* attr,
    uint64_t* counterIdOut)
{
    (void)domain;
    if (!attr || !counterIdOut) return NVTXW_RESULT_INVALID_ARGUMENT;
    ++g_stub.counterRegisterCalls;
    *counterIdOut = ++g_stub.nextId;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t StubCounterWrite(
    nvtxwStreamHandle_t stream,
    int64_t timestamp,
    uint64_t counterId,
    const void* data,
    size_t size)
{
    (void)stream;
    (void)timestamp;
    (void)counterId;
    if (!data || size == 0) return NVTXW_RESULT_INVALID_ARGUMENT;
    ++g_stub.counterWriteCalls;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t StubCategoryRegister(
    nvtxDomainHandle_t domain,
    uint32_t category,
    const char* name)
{
    (void)domain;
    (void)category;
    if (!name) return NVTXW_RESULT_INVALID_ARGUMENT;
    ++g_stub.categoryRegisterCalls;
    return NVTXW_RESULT_SUCCESS;
}

/* A backend that does not support categories still implements the member; it
 * reports the gap by returning NVTXW_RESULT_NOT_SUPPORTED. */
static nvtxwResultCode_t StubCategoryRegisterUnsupported(
    nvtxDomainHandle_t domain,
    uint32_t category,
    const char* name)
{
    (void)domain;
    (void)category;
    (void)name;
    return NVTXW_RESULT_NOT_SUPPORTED;
}

static int StubConfigConsumer(
    void* state,
    const char* keyBegin,
    const char* keyEnd,
    const char* valBegin,
    const char* valEnd)
{
    int* count = NVTX_STATIC_CAST(int*, state);
    (void)keyBegin;
    (void)keyEnd;
    (void)valBegin;
    (void)valEnd;
    if (count) ++*count;
    return 0;
}

#ifdef __cplusplus
extern "C" {
#endif
NVTX_DYNAMIC_EXPORT
extern int RunTest(int argc, const char** argv);
#ifdef __cplusplus
}
#endif
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    nvtxwInterface_t iface;
    nvtxDomainHandle_t domain =
        NVTX_REINTERPRET_CAST(nvtxDomainHandle_t, NVTX_STATIC_CAST(uintptr_t, 1));
    nvtxwStreamHandle_t stream =
        NVTX_REINTERPRET_CAST(nvtxwStreamHandle_t, NVTX_STATIC_CAST(uintptr_t, 2));
    nvtxwEventHelperSchemaIds_t schemaIds;
    nvtxwEventWriter_t writer;
    nvtxwEventWriter_t writerNoIface;
    nvtxwEventAttributesUtf8_t utf8Attr;
    nvtxwEventAttributes_t regAttr;
    nvtxwSessionAttributes_t sessionAttr;
    nvtxwDomainAttributes_t domainAttr;
    nvtxwStreamAttributes_t streamAttr;
    uint64_t counterId = 0;
    int configPairs = 0;
    nvtxRangeId_t rangeId = NVTX_STATIC_CAST(nvtxRangeId_t, 1);

    (void)argc;
    (void)argv;

    memset(&g_stub, 0, sizeof(g_stub));

    /* nvtxwConsumeConfigString is a fully inline utility (no backend needed). */
    nvtxwConsumeConfigString(
        "# comment\nInitMode=2|InitModeString=backend",
        StubConfigConsumer,
        &configPairs);
    if (configPairs != 2) return 1;

    memset(&regAttr, 0, sizeof(regAttr));
    memset(&utf8Attr, 0, sizeof(utf8Attr));

    /* The attribute initializers in nvtxw3_setup_helpers.h must zero-fill the struct, stamp
     * structSize, and apply the documented defaults.  Pre-dirty the structs so a
     * missing memset would be caught. */
    memset(&sessionAttr, 0xFF, sizeof(sessionAttr));
    if (nvtxwSessionAttributesInit(&sessionAttr) != NVTXW_RESULT_SUCCESS) return 30;
    if (sessionAttr.structSize != sizeof(sessionAttr) ||
        sessionAttr.name != NVTX_NULLPTR ||
        sessionAttr.configString != NVTX_NULLPTR) return 31;

    memset(&domainAttr, 0xFF, sizeof(domainAttr));
    if (nvtxwDomainAttributesInit(&domainAttr) != NVTXW_RESULT_SUCCESS) return 32;
    if (domainAttr.structSize != sizeof(domainAttr) ||
        domainAttr.name != NVTX_NULLPTR) return 33;

    memset(&streamAttr, 0xFF, sizeof(streamAttr));
    if (nvtxwStreamAttributesInit(&streamAttr) != NVTXW_RESULT_SUCCESS) return 34;
    if (streamAttr.structSize != sizeof(streamAttr) ||
        streamAttr.name != NVTX_NULLPTR || streamAttr.domain != NVTX_NULLPTR ||
        streamAttr.scopeId != NVTX_SCOPE_NONE ||
        streamAttr.timeDomainId != NVTX_TIME_DOMAIN_ID_NONE ||
        streamAttr.orderInterleaving != NVTXW_STREAM_ORDER_INTERLEAVING_NONE ||
        streamAttr.orderingType != NVTXW_STREAM_ORDERING_TYPE_UNKNOWN ||
        streamAttr.orderingSkid != NVTXW_STREAM_ORDERING_SKID_NONE) return 35;

    /* A null attribute pointer must be rejected by each initializer. */
    if (nvtxwSessionAttributesInit(NVTX_NULLPTR)
        != NVTXW_RESULT_INVALID_ARGUMENT) return 36;
    if (nvtxwDomainAttributesInit(NVTX_NULLPTR)
        != NVTXW_RESULT_INVALID_ARGUMENT) return 37;
    if (nvtxwStreamAttributesInit(NVTX_NULLPTR)
        != NVTXW_RESULT_INVALID_ARGUMENT) return 38;

    memset(&iface, 0, sizeof(iface));
    iface.SchemaRegister = StubSchemaRegister;
    iface.EventWrite = StubEventWrite;
    iface.CounterRegister = StubCounterRegister;
    iface.CounterWrite = StubCounterWrite;
    iface.CategoryRegister = StubCategoryRegister;

    if (nvtxwEventSchemasRegister(
            &iface, domain, NVTXW_EVENT_HELPER_SCHEMA_ALL, &schemaIds)
        != NVTXW_RESULT_SUCCESS)
        return 4;
    /* 12 event schemas plus the shared standalone UTF-8 message schema. */
    if (g_stub.schemaRegisterCalls != 13) return 6;

    if (nvtxwEventWriterInit(&writer, &iface, stream, &schemaIds)
        != NVTXW_RESULT_SUCCESS) return 23;
    if (writer.iface != &iface || writer.stream != stream ||
        memcmp(&writer.schemaIds, &schemaIds, sizeof(schemaIds)) != 0) return 24;
    if (nvtxwEventWriterInit(NVTX_NULLPTR, &iface, stream, &schemaIds)
        != NVTXW_RESULT_INVALID_ARGUMENT) return 47;
    /* A null schemaIds must clear the embedded IDs to zero. */
    {
        nvtxwEventHelperSchemaIds_t zeroIds;
        memset(&zeroIds, 0, sizeof(zeroIds));
        if (nvtxwEventWriterInit(&writerNoIface, &iface, stream, NVTX_NULLPTR)
            != NVTXW_RESULT_SUCCESS) return 50;
        if (memcmp(&writerNoIface.schemaIds, &zeroIds, sizeof(zeroIds)) != 0)
            return 51;
    }

    /* nvtxwEventWriterOpen must reject missing arguments up front, before it
     * registers any schemas (the stub has no StreamOpen; the full happy path is
     * covered against a recording backend in CoverageNvtxwE2E). */
    if (nvtxwEventWriterOpen(
            &iface, NVTX_NULLPTR, "s", domain, NVTX_NULLPTR,
            NVTX_TIME_DOMAIN_ID_NONE, &writer)
        != NVTXW_RESULT_INVALID_ARGUMENT) return 52;
    if (nvtxwEventWriterOpen(
            &iface,
            NVTX_REINTERPRET_CAST(
                nvtxwSessionHandle_t, NVTX_STATIC_CAST(uintptr_t, 3)),
            "s", domain, NVTX_NULLPTR, NVTX_TIME_DOMAIN_ID_NONE, NVTX_NULLPTR)
        != NVTXW_RESULT_INVALID_ARGUMENT) return 53;
    if (g_stub.schemaRegisterCalls != 13) return 54;

    utf8Attr.color = 0xFF00FF00u;
    utf8Attr.category = 1;
    utf8Attr.message = "coverage";

    if (nvtxwMarkWriteUtf8(&writer, 10, utf8Attr)
        != NVTXW_RESULT_SUCCESS) return 7;
    if (nvtxwRangePushPopWriteUtf8(&writer, 10, 20, utf8Attr)
        != NVTXW_RESULT_SUCCESS) return 8;
    if (nvtxwRangeStartEndWriteUtf8(&writer, 10, 20, utf8Attr)
        != NVTXW_RESULT_SUCCESS) return 9;
    if (nvtxwRangePushWriteUtf8(&writer, 10, utf8Attr)
        != NVTXW_RESULT_SUCCESS) return 10;
    if (nvtxwRangePopWrite(&writer, 20)
        != NVTXW_RESULT_SUCCESS) return 11;
    if (nvtxwRangeStartWriteUtf8(&writer, 10, rangeId, utf8Attr)
        != NVTXW_RESULT_SUCCESS) return 12;
    if (nvtxwRangeEndWrite(&writer, 20, rangeId)
        != NVTXW_RESULT_SUCCESS) return 13;

    /* Exercise the explicit-messageLength path (non-null-terminated message).
     * The bytes that reach the backend are verified in CoverageNvtxwE2E. */
    utf8Attr.message = "coverage-plus-trailing-bytes";
    utf8Attr.messageLength = 8;
    if (nvtxwMarkWriteUtf8(&writer, 10, utf8Attr)
        != NVTXW_RESULT_SUCCESS) return 25;
    utf8Attr.messageLength = 0;

    if (nvtxwMarkWrite(&writer, 10, regAttr)
        != NVTXW_RESULT_SUCCESS) return 14;
    if (nvtxwRangePushPopWrite(&writer, 10, 20, regAttr)
        != NVTXW_RESULT_SUCCESS) return 15;

    if (nvtxwCounterInt64Register(
            &iface, domain, "int-counter", NVTX_NULLPTR, &counterId)
        != NVTXW_RESULT_SUCCESS) return 16;
    if (nvtxwCounterInt64Write(&iface, stream, 1, counterId, 42)
        != NVTXW_RESULT_SUCCESS) return 17;
    if (nvtxwCounterFloat64Register(
            &iface, domain, "double-counter", NVTX_NULLPTR, &counterId)
        != NVTXW_RESULT_SUCCESS) return 18;
    if (nvtxwCounterFloat64Write(&iface, stream, 2, counterId, 3.5)
        != NVTXW_RESULT_SUCCESS) return 19;

    /* A missing interface must be rejected before any backend call. */
    if (nvtxwEventWriterInit(&writerNoIface, NVTX_NULLPTR, stream, &schemaIds)
        != NVTXW_RESULT_SUCCESS) return 48;
    if (nvtxwMarkWriteUtf8(&writerNoIface, 0, utf8Attr)
        != NVTXW_RESULT_INVALID_ARGUMENT) return 20;
    /* A null writer must also be rejected. */
    if (nvtxwMarkWriteUtf8(NVTX_NULLPTR, 0, utf8Attr)
        != NVTXW_RESULT_INVALID_ARGUMENT) return 49;

    if (g_stub.eventWriteCalls != 10) return 21;
    if (g_stub.counterWriteCalls != 2) return 22;

    /* nvtxwCategoryRegister forwards to the CategoryRegister member and rejects
     * a NULL interface, member, domain, or name. */
    if (nvtxwCategoryRegister(NVTX_NULLPTR, domain, 1, "cat")
        != NVTXW_RESULT_INVALID_ARGUMENT) return 60;
    if (nvtxwCategoryRegister(&iface, NVTX_NULLPTR, 1, "cat")
        != NVTXW_RESULT_INVALID_ARGUMENT) return 61;
    if (nvtxwCategoryRegister(&iface, domain, 1, NVTX_NULLPTR)
        != NVTXW_RESULT_INVALID_ARGUMENT) return 62;
    /* Category 0 is reserved for "no category" and must be rejected. */
    if (nvtxwCategoryRegister(&iface, domain, 0, "cat")
        != NVTXW_RESULT_INVALID_ARGUMENT) return 66;
    if (nvtxwCategoryRegister(&iface, domain, 1, "cat")
        != NVTXW_RESULT_SUCCESS) return 63;
    if (g_stub.categoryRegisterCalls != 1) return 64;
    /* Baseline members are always non-NULL; a null member is a malformed
     * interface and is rejected as an invalid argument, not dereferenced. */
    {
        nvtxwInterface_t bare;
        memset(&bare, 0, sizeof(bare));
        if (nvtxwCategoryRegister(&bare, domain, 1, "cat")
            != NVTXW_RESULT_INVALID_ARGUMENT) return 65;
    }
    /* A backend that does not support categories implements the member and
     * returns NVTXW_RESULT_NOT_SUPPORTED, which the helper forwards unchanged. */
    {
        nvtxwInterface_t unsupported = iface;
        unsupported.CategoryRegister = StubCategoryRegisterUnsupported;
        if (nvtxwCategoryRegister(&unsupported, domain, 1, "cat")
            != NVTXW_RESULT_NOT_SUPPORTED) return 67;
    }

    return 0;
}
