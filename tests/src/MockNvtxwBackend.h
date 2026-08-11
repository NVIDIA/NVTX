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
 * Recording NVTXW backend for the NVTXW tests.  Implements the full
 * nvtxwInterface_v2 table, but records received data into a singleton
 * MockNvtxwRecorder instead of producing a trace.  Header-only, so it can be
 * compiled into an in-process test or wrapped by a TU that exports the loader
 * entry points as a backend library.
 */

#if !defined(MOCK_NVTXW_BACKEND_H)
#define MOCK_NVTXW_BACKEND_H

#include <nvtx3/nvToolsExtPayload.h>
#include <nvtx3/nvToolsExtCounters.h>
#include <nvtxw3/nvtxw3.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOCK_NVTXW_MAX_EVENTS   64
#define MOCK_NVTXW_MAX_COUNTERS 64
#define MOCK_NVTXW_MAX_SCHEMAS  64
#define MOCK_NVTXW_MAX_BYTES    64

typedef struct MockNvtxwEventRecord
{
    size_t   payloadCount;
    uint64_t headerSchemaId;             /* payloads[0].schemaId */
    size_t   headerSize;                 /* payloads[0].size     */
    int      hasMessagePayload;          /* payloadCount >= 2    */
    size_t   messageSize;                /* payloads[1].size     */
    unsigned char messageBytes[MOCK_NVTXW_MAX_BYTES];
} MockNvtxwEventRecord;

typedef struct MockNvtxwCounterRecord
{
    int64_t  timestamp;
    uint64_t counterId;
    size_t   size;
    unsigned char data[MOCK_NVTXW_MAX_BYTES];
} MockNvtxwCounterRecord;

/* Facts of a registered schema, so tests can assert layout invariants
 * (e.g. the helpers' UTF-8 event-message contract) instead of the backend. */
typedef struct MockNvtxwSchemaRecord
{
    uint64_t schemaId;
    uint64_t schemaType;        /* attr->type */
    size_t   numEntries;        /* attr->numEntries */
    int      hasEventMessage;   /* an entry has NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE */
    uint64_t messageEntryType;  /* that entry's type  (valid if hasEventMessage) */
    uint64_t messageEntryFlags; /* that entry's flags (valid if hasEventMessage) */
} MockNvtxwSchemaRecord;

/* Backend-owned objects handed back as opaque handles.  Functions after
 * DomainRegister/StreamOpen receive only the domain/stream handle, so each
 * object keeps a link back to its owning session, as a real backend would. */
typedef struct MockNvtxwDomain
{
    struct MockNvtxwRecorder* session;
} MockNvtxwDomain;

typedef struct MockNvtxwStream
{
    struct MockNvtxwRecorder* session;
} MockNvtxwStream;

typedef struct MockNvtxwRecorder
{
    int sessionBeginCalls;
    int sessionEndCalls;
    int domainRegisterCalls;
    int stringRegisterCalls;
    int resourceRegisterCalls;
    int scopeRegisterCalls;
    int schemaRegisterCalls;
    int enumRegisterCalls;
    int counterRegisterCalls;
    int timeDomainRegisterCalls;
    int categoryRegisterCalls;
    int streamOpenCalls;
    int streamCloseCalls;
    int eventWriteCalls;
    int counterWriteCalls;
    int counterNoValueWriteCalls;

    /* Monotonic source for every dynamic ID handed back to the caller. */
    uint64_t nextId;

    /* Config string the backend read from its environment variable at load. */
    char loadConfig[256];

    /* Last category named via CategoryRegister, for test assertions. */
    uint32_t lastCategoryId;
    char     lastCategoryName[64];

    int eventCount;
    MockNvtxwEventRecord events[MOCK_NVTXW_MAX_EVENTS];

    int counterCount;
    MockNvtxwCounterRecord counters[MOCK_NVTXW_MAX_COUNTERS];

    int schemaCount;
    MockNvtxwSchemaRecord schemas[MOCK_NVTXW_MAX_SCHEMAS];

    /* Domain/stream objects handed out as handles; one each (single session). */
    MockNvtxwDomain domain;
    MockNvtxwStream stream;
} MockNvtxwRecorder;

static MockNvtxwRecorder g_mockNvtxwRecorder;

static MockNvtxwRecorder* MockNvtxwGetRecorder(void)
{
    return &g_mockNvtxwRecorder;
}

static void MockNvtxwReset(void)
{
    memset(&g_mockNvtxwRecorder, 0, sizeof(g_mockNvtxwRecorder));
}

/* The session handle is just the recorder (the mock's session state).  Every
 * other handle is recovered from the argument the caller passes back in, so the
 * test exercises the same parameter threading a real backend relies on. */
static MockNvtxwRecorder* MockNvtxwRecorderFromSession(nvtxwSessionHandle_t session)
{
    return (MockNvtxwRecorder*)(void*)session;
}

static MockNvtxwRecorder* MockNvtxwRecorderFromDomain(nvtxDomainHandle_t domain)
{
    return ((MockNvtxwDomain*)(void*)domain)->session;
}

static MockNvtxwRecorder* MockNvtxwRecorderFromStream(nvtxwStreamHandle_t stream)
{
    return ((MockNvtxwStream*)(void*)stream)->session;
}

static nvtxwResultCode_t MockNvtxwSessionBegin(
    const nvtxwSessionAttributes_t* attr,
    nvtxwSessionHandle_t* sessionOut)
{
    if (!attr || attr->structSize < sizeof(*attr) || !sessionOut)
        return NVTXW_RESULT_INVALID_ARGUMENT;
    ++g_mockNvtxwRecorder.sessionBeginCalls;
    *sessionOut = (nvtxwSessionHandle_t)(void*)&g_mockNvtxwRecorder;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwSessionEnd(nvtxwSessionHandle_t session)
{
    MockNvtxwRecorder* rec;
    if (!session) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromSession(session);
    ++rec->sessionEndCalls;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwDomainRegister(
    nvtxwSessionHandle_t session,
    const nvtxwDomainAttributes_t* attr,
    nvtxDomainHandle_t* domainOut)
{
    MockNvtxwRecorder* rec;
    if (!session || !attr || !domainOut) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromSession(session);
    ++rec->domainRegisterCalls;
    rec->domain.session = rec;
    *domainOut = (nvtxDomainHandle_t)(void*)&rec->domain;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwCategoryRegister(
    nvtxDomainHandle_t domain,
    uint32_t category,
    const char* name)
{
    MockNvtxwRecorder* rec;
    size_t copy;
    if (!domain || !name) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromDomain(domain);
    ++rec->categoryRegisterCalls;
    rec->lastCategoryId = category;
    copy = strlen(name);
    if (copy >= sizeof(rec->lastCategoryName))
        copy = sizeof(rec->lastCategoryName) - 1;
    memcpy(rec->lastCategoryName, name, copy);
    rec->lastCategoryName[copy] = '\0';
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwStringRegister(
    nvtxDomainHandle_t domain,
    const char* string,
    nvtxStringHandle_t* stringHandleOut)
{
    MockNvtxwRecorder* rec;
    if (!domain || !string || !stringHandleOut) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromDomain(domain);
    ++rec->stringRegisterCalls;
    *stringHandleOut = (nvtxStringHandle_t)(uintptr_t)(++rec->nextId);
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwResourceRegister(
    nvtxDomainHandle_t domain,
    const nvtxResourceAttributes_t* attr)
{
    MockNvtxwRecorder* rec;
    if (!domain || !attr) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromDomain(domain);
    ++rec->resourceRegisterCalls;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwScopeRegister(
    nvtxDomainHandle_t domain,
    const nvtxScopeAttr_t* attr,
    uint64_t* scopeIdOut)
{
    MockNvtxwRecorder* rec;
    if (!domain || !attr) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromDomain(domain);
    ++rec->scopeRegisterCalls;
    if (scopeIdOut) *scopeIdOut = ++rec->nextId;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwSchemaRegister(
    nvtxDomainHandle_t domain,
    const nvtxPayloadSchemaAttr_t* attr,
    uint64_t* schemaIdOut)
{
    MockNvtxwRecorder* rec;
    if (!domain || !attr || !schemaIdOut) return NVTXW_RESULT_INVALID_ARGUMENT;
    if (attr->numEntries > 0 && !attr->entries) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromDomain(domain);

    ++rec->schemaRegisterCalls;
    *schemaIdOut = ++rec->nextId;

    /* Record the schema's layout facts; tests assert any contract on them. */
    if (rec->schemaCount < MOCK_NVTXW_MAX_SCHEMAS)
    {
        size_t i;
        MockNvtxwSchemaRecord* sc = &rec->schemas[rec->schemaCount++];
        sc->schemaId = *schemaIdOut;
        sc->schemaType = attr->type;
        sc->numEntries = attr->numEntries;
        for (i = 0; i < attr->numEntries; ++i)
        {
            const nvtxPayloadSchemaEntry_t* e = &attr->entries[i];
            if (e->flags & NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE)
            {
                sc->hasEventMessage = 1;
                sc->messageEntryType = e->type;
                sc->messageEntryFlags = e->flags;
            }
        }
    }
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwEnumRegister(
    nvtxDomainHandle_t domain,
    const nvtxPayloadEnumAttr_t* attr,
    uint64_t* enumIdOut)
{
    MockNvtxwRecorder* rec;
    if (!domain || !attr) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromDomain(domain);
    ++rec->enumRegisterCalls;
    if (enumIdOut) *enumIdOut = ++rec->nextId;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwCounterRegister(
    nvtxDomainHandle_t domain,
    const nvtxCounterAttr_t* attr,
    uint64_t* counterIdOut)
{
    MockNvtxwRecorder* rec;
    if (!domain || !attr || !counterIdOut) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromDomain(domain);
    ++rec->counterRegisterCalls;
    *counterIdOut = ++rec->nextId;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwTimeDomainRegister(
    nvtxDomainHandle_t domain,
    const nvtxTimeDomainAttr_t* attr,
    uint64_t* timeDomainIdOut)
{
    MockNvtxwRecorder* rec;
    if (!domain || !attr) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromDomain(domain);
    ++rec->timeDomainRegisterCalls;
    if (timeDomainIdOut) *timeDomainIdOut = ++rec->nextId;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwStreamOpen(
    nvtxwSessionHandle_t session,
    const nvtxwStreamAttributes_t* attr,
    nvtxwStreamHandle_t* streamOut)
{
    MockNvtxwRecorder* rec;
    if (!session || !attr || attr->structSize < sizeof(*attr) || !streamOut)
        return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromSession(session);
    ++rec->streamOpenCalls;
    rec->stream.session = rec;
    *streamOut = (nvtxwStreamHandle_t)(void*)&rec->stream;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwStreamClose(nvtxwStreamHandle_t stream)
{
    MockNvtxwRecorder* rec;
    if (!stream) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromStream(stream);
    ++rec->streamCloseCalls;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwEventWrite(
    nvtxwStreamHandle_t stream,
    const nvtxPayloadData_t* payloads,
    size_t payloadCount)
{
    MockNvtxwRecorder* rec;
    if (!stream || !payloads || payloadCount == 0) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromStream(stream);

    ++rec->eventWriteCalls;
    if (rec->eventCount < MOCK_NVTXW_MAX_EVENTS)
    {
        MockNvtxwEventRecord* ev = &rec->events[rec->eventCount++];
        ev->payloadCount = payloadCount;
        ev->headerSchemaId = payloads[0].schemaId;
        ev->headerSize = payloads[0].size;
        ev->hasMessagePayload = (payloadCount >= 2) ? 1 : 0;
        if (payloadCount >= 2 && payloads[1].payload)
        {
            size_t copy = payloads[1].size;
            ev->messageSize = copy;
            if (copy > MOCK_NVTXW_MAX_BYTES) copy = MOCK_NVTXW_MAX_BYTES;
            memcpy(ev->messageBytes, payloads[1].payload, copy);
        }
    }
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwEventBatchWrite(
    nvtxwStreamHandle_t stream,
    const nvtxEventBatch_t* eventBatch)
{
    MockNvtxwRecorder* rec;
    if (!stream || !eventBatch) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromStream(stream);
    ++rec->eventWriteCalls;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwCounterWrite(
    nvtxwStreamHandle_t stream,
    int64_t timestamp,
    uint64_t counterId,
    const void* data,
    size_t size)
{
    MockNvtxwRecorder* rec;
    if (!stream || !data || size == 0) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromStream(stream);

    ++rec->counterWriteCalls;
    if (rec->counterCount < MOCK_NVTXW_MAX_COUNTERS)
    {
        MockNvtxwCounterRecord* ctr = &rec->counters[rec->counterCount++];
        size_t copy = size;
        ctr->timestamp = timestamp;
        ctr->counterId = counterId;
        ctr->size = size;
        if (copy > MOCK_NVTXW_MAX_BYTES) copy = MOCK_NVTXW_MAX_BYTES;
        memcpy(ctr->data, data, copy);
    }
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwCounterNoValueWrite(
    nvtxwStreamHandle_t stream,
    int64_t timestamp,
    uint64_t counterId,
    uint8_t reason)
{
    MockNvtxwRecorder* rec;
    (void)timestamp;
    (void)counterId;
    (void)reason;
    if (!stream) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromStream(stream);
    ++rec->counterNoValueWriteCalls;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwCounterBatchWrite(
    nvtxwStreamHandle_t stream,
    const nvtxCounterBatch_t* counterBatch)
{
    MockNvtxwRecorder* rec;
    if (!stream || !counterBatch) return NVTXW_RESULT_INVALID_ARGUMENT;
    rec = MockNvtxwRecorderFromStream(stream);
    ++rec->counterWriteCalls;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwTimeSyncPointWrite(
    nvtxwStreamHandle_t stream,
    uint64_t timeDomainId1,
    uint64_t timeDomainId2,
    int64_t timestamp1,
    int64_t timestamp2)
{
    (void)timeDomainId1;
    (void)timeDomainId2;
    (void)timestamp1;
    (void)timestamp2;
    if (!stream) return NVTXW_RESULT_INVALID_ARGUMENT;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwTimeSyncPointTableWrite(
    nvtxwStreamHandle_t stream,
    uint64_t timeDomainIdSrc,
    uint64_t timeDomainIdDst,
    const nvtxSyncPoint_t* syncPoints,
    size_t count)
{
    (void)timeDomainIdSrc;
    (void)timeDomainIdDst;
    if (!stream || !syncPoints || count == 0) return NVTXW_RESULT_INVALID_ARGUMENT;
    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t MockNvtxwTimestampConversionFactorWrite(
    nvtxwStreamHandle_t stream,
    uint64_t timeDomainIdSrc,
    uint64_t timeDomainIdDst,
    double slope,
    int64_t timestampSrc,
    int64_t timestampDst)
{
    (void)timeDomainIdSrc;
    (void)timeDomainIdDst;
    (void)slope;
    (void)timestampSrc;
    (void)timestampDst;
    if (!stream) return NVTXW_RESULT_INVALID_ARGUMENT;
    return NVTXW_RESULT_SUCCESS;
}

static const nvtxwInterface_v2_t* MockNvtxwInterfaceV2(void)
{
    static const nvtxwInterface_v2_t iface =
    {
        MockNvtxwSessionBegin,
        MockNvtxwSessionEnd,
        MockNvtxwDomainRegister,
        MockNvtxwCategoryRegister,
        MockNvtxwStringRegister,
        MockNvtxwResourceRegister,
        MockNvtxwScopeRegister,
        MockNvtxwSchemaRegister,
        MockNvtxwEnumRegister,
        MockNvtxwCounterRegister,
        MockNvtxwTimeDomainRegister,
        MockNvtxwStreamOpen,
        MockNvtxwStreamClose,
        MockNvtxwEventWrite,
        MockNvtxwEventBatchWrite,
        MockNvtxwCounterWrite,
        MockNvtxwCounterNoValueWrite,
        MockNvtxwCounterBatchWrite,
        MockNvtxwTimeSyncPointWrite,
        MockNvtxwTimeSyncPointTableWrite,
        MockNvtxwTimestampConversionFactorWrite,
        {0} /* reserved */
    };
    return &iface;
}

/* Non-zero if the config has a line "key=value" with a non-zero value. */
static int MockNvtxwConfigFlag(const char* config, const char* key)
{
    const char* cur = config;
    size_t keyLen;

    if (!config || !key) return 0;
    keyLen = strlen(key);

    while (*cur)
    {
        const char* lineEnd = cur + strcspn(cur, "\n");
        if ((size_t)(lineEnd - cur) > keyLen + 1 &&
            strncmp(cur, key, keyLen) == 0 &&
            cur[keyLen] == '=')
        {
            return cur[keyLen + 1] != '0';
        }
        cur = (*lineEnd) ? lineEnd + 1 : lineEnd;
    }
    return 0;
}

/* Name of the environment variable the mock backend reads its configuration
 * from.  This demonstrates a backend obtaining load-time config without the
 * loader forwarding it.  Recognized flags:
 *   MockFailLoad=1      -> return NVTXW_RESULT_FAILED
 *   MockNullInterface=1 -> succeed but leave *ifaceOut NULL */
#define MOCK_NVTXW_CONFIG_ENV_VAR "NVTXW_MOCK_CONFIG"

/* Shared backend entry point (a wrapping TU exports it as nvtxwGetInterface).
 * Maps an interface version to a function table, after honoring any environment
 * configuration. */
static nvtxwResultCode_t MockNvtxwGetInterface(
    nvtxwInterfaceVersion_t version,
    const void** ifaceOut)
{
    /* Disable the MSVC deprecation warning for getenv -- this usage is safe
     * because the returned value is used before any subsequent call. */
#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4996 )
#endif
    const char* envConfig = getenv(MOCK_NVTXW_CONFIG_ENV_VAR);
#if defined(_MSC_VER)
#pragma warning( pop )
#endif

    if (!ifaceOut) return NVTXW_RESULT_INVALID_ARGUMENT;
    *ifaceOut = NULL;

    if (envConfig)
    {
        size_t copy = strlen(envConfig);
        if (copy >= sizeof(g_mockNvtxwRecorder.loadConfig))
            copy = sizeof(g_mockNvtxwRecorder.loadConfig) - 1;
        memcpy(g_mockNvtxwRecorder.loadConfig, envConfig, copy);
        g_mockNvtxwRecorder.loadConfig[copy] = '\0';

        if (MockNvtxwConfigFlag(envConfig, "MockFailLoad"))
            return NVTXW_RESULT_FAILED;
        if (MockNvtxwConfigFlag(envConfig, "MockNullInterface"))
            return NVTXW_RESULT_SUCCESS; /* leaves *ifaceOut == NULL */
    }
    else
    {
        g_mockNvtxwRecorder.loadConfig[0] = '\0';
    }

    if (version == NVTXW_INTERFACE_VERSION)
    {
        *ifaceOut = MockNvtxwInterfaceV2();
        return NVTXW_RESULT_SUCCESS;
    }

    return NVTXW_RESULT_INTERFACE_VERSION_NOT_SUPPORTED;
}

#ifdef __cplusplus
}
#endif

#endif /* MOCK_NVTXW_BACKEND_H */
