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

#if !defined(NVTXW_SETUP_HELPERS_API)
#define NVTXW_SETUP_HELPERS_API

#include <nvtxw3/nvtxw3_loader.h>
#include <nvtxw3/nvtxw3_event_helpers.h>

/**
 * \file nvtxw3_setup_helpers.h
 * \brief Setup helpers for loading NVTXW, registering domains, scopes, and time
 * domains, and opening streams.
 */

#include <string.h> /* For memset in the attribute initialization utilities */

#ifdef __cplusplus
extern "C" {
#endif

/*--------- Attribute initialization utilities ---------*/

/**
 * \brief Zero-fill an \ref nvtxwSessionAttributes_t and stamp its `structSize`.
 *
 * Call this before populating the struct and passing it to SessionBegin.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwSessionAttributesInit(
    nvtxwSessionAttributes_t* attr)
{
    if (!attr) return NVTXW_RESULT_INVALID_ARGUMENT;
    memset(attr, 0, sizeof(*attr));
    attr->structSize = sizeof(*attr);
    return NVTXW_RESULT_SUCCESS;
}

/**
 * \brief Zero-fill an \ref nvtxwDomainAttributes_t and stamp its `structSize`.
 *
 * Call this before populating the struct and passing it to DomainRegister.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwDomainAttributesInit(
    nvtxwDomainAttributes_t* attr)
{
    if (!attr) return NVTXW_RESULT_INVALID_ARGUMENT;
    memset(attr, 0, sizeof(*attr));
    attr->structSize = sizeof(*attr);
    return NVTXW_RESULT_SUCCESS;
}

/**
 * \brief Zero-fill an \ref nvtxwStreamAttributes_t, stamp its `structSize`, and
 * apply the documented defaults.
 *
 * Call this before populating the struct and passing it to StreamOpen.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwStreamAttributesInit(
    nvtxwStreamAttributes_t* attr)
{
    if (!attr) return NVTXW_RESULT_INVALID_ARGUMENT;
    memset(attr, 0, sizeof(*attr));
    attr->structSize = sizeof(*attr);
    attr->scopeId = NVTX_SCOPE_NONE;
    attr->timeDomainId = NVTX_TIME_DOMAIN_ID_NONE;
    attr->orderInterleaving = NVTXW_STREAM_ORDER_INTERLEAVING_NONE;
    attr->orderingType = NVTXW_STREAM_ORDERING_TYPE_UNKNOWN;
    attr->orderingSkid = NVTXW_STREAM_ORDERING_SKID_NONE;
    return NVTXW_RESULT_SUCCESS;
}

/**
 * \brief Load and initialize an NVTXW backend and return the core interface table.
 *
 * `library` is passed directly to \ref nvtxwLoad -- NULL uses the default search
 * (overridable by the `NVTXW3_LIBRARY` environment variable), or a backend
 * filename/path loads that library specifically.
 * If `moduleOut` is non-NULL, it receives the implementation-defined module
 * handle that keeps the backend loaded. The caller must keep that module
 * handle alive while using the returned interface. If `moduleOut` is NULL, the
 * backend remains loaded until process exit.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwLoadInterface(
    const char* library,
    const nvtxwInterface_t** interfaceOut,
    nvtxwModuleHandle_t* moduleOut)
{
    nvtxwGetInterface_t getInterface;
    const void* iface;
    nvtxwModuleHandle_t module;
    nvtxwResultCode_t result;

    if (!interfaceOut) return NVTXW_RESULT_INVALID_ARGUMENT;

    *interfaceOut = NVTX_NULLPTR;
    if (moduleOut) *moduleOut = NVTX_NULLPTR;

    getInterface = NVTX_NULLPTR;
    module = NVTX_NULLPTR;
    result = nvtxwLoad(
        library,
        &getInterface,
        &module);
    if (result != NVTXW_RESULT_SUCCESS) return result;
    if (!getInterface)
    {
        nvtxwUnload(module);
        return NVTXW_RESULT_LIBRARY_LOAD_FAILED;
    }

    iface = NVTX_NULLPTR;
    result = getInterface(NVTXW_INTERFACE_VERSION, &iface);
    if (result != NVTXW_RESULT_SUCCESS || !iface)
    {
        nvtxwUnload(module);
        return result != NVTXW_RESULT_SUCCESS
            ? result
            : NVTXW_RESULT_INTERFACE_VERSION_NOT_SUPPORTED;
    }

    *interfaceOut = NVTX_STATIC_CAST(const nvtxwInterface_t*, iface);
    if (moduleOut) *moduleOut = module;
    return NVTXW_RESULT_SUCCESS;
}

/** \brief Load and initialize an NVTXW backend using the default search. */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwLoadInterfaceDefault(
    const nvtxwInterface_t** interfaceOut,
    nvtxwModuleHandle_t* moduleOut)
{
    return nvtxwLoadInterface(
        NVTX_NULLPTR,
        interfaceOut,
        moduleOut);
}

/**
 * \brief Register or retrieve a domain.
 *
 * NULL or empty `domainName` means the session default domain. The returned
 * domain handle is owned by the session and is valid until its end.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwDomainRegister(
    const nvtxwInterface_t* iface,
    nvtxwSessionHandle_t session,
    const char* domainName,
    nvtxDomainHandle_t* domainOut)
{
    nvtxwDomainAttributes_t domainAttr;
    nvtxwResultCode_t result;

    if (!iface || !iface->DomainRegister || !session || !domainOut)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    *domainOut = NVTX_NULLPTR;

    result = nvtxwDomainAttributesInit(&domainAttr);
    if (result != NVTXW_RESULT_SUCCESS) return result;
    domainAttr.name = domainName;

    return iface->DomainRegister(session, &domainAttr, domainOut);
}

/**
 * \brief Name an NVTX category in a domain.
 *
 * `domain` and `name` must be non-NULL. `category` is the caller-defined ID
 * events reference through a `NVTX_PAYLOAD_ENTRY_TYPE_CATEGORY` payload entry;
 * it must be non-zero, as 0 is reserved to mean "no category".
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwCategoryRegister(
    const nvtxwInterface_t* iface,
    nvtxDomainHandle_t domain,
    uint32_t category,
    const char* name)
{
    if (!iface || !iface->CategoryRegister || !domain || !name || category == 0)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    return iface->CategoryRegister(domain, category, name);
}

/**
 * \brief Register a scope in a domain.
 *
 * The domain handle must be non-NULL. NULL or empty scopePath registers the
 * domain root scope. The implementation allocates a dynamic scope ID, which
 * is returned via `scopeIdOut` (which must be non-NULL).
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwScopeRegister(
    const nvtxwInterface_t* iface,
    nvtxDomainHandle_t domain,
    const char* scopePath,
    uint64_t* scopeIdOut)
{
    nvtxScopeAttr_t scopeAttr;

    if (!iface || !iface->ScopeRegister || !domain || !scopeIdOut)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    *scopeIdOut = NVTX_SCOPE_NONE;

    memset(&scopeAttr, 0, sizeof(scopeAttr));
    scopeAttr.structSize = sizeof(scopeAttr);
    scopeAttr.path = scopePath;
    scopeAttr.parentScope = NVTX_SCOPE_ROOT;

    return iface->ScopeRegister(domain, &scopeAttr, scopeIdOut);
}

/**
 * \brief Register a time domain from a predefined timestamp type.
 *
 * `timestampTypeId` must be a predefined `NVTX_TIMESTAMP_TYPE_*` value, or
 * `NVTX_TIMESTAMP_TYPE_NONE` when the source is unknown. The implementation
 * allocates a dynamic time domain ID.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwTimeDomainRegister(
    const nvtxwInterface_t* iface,
    nvtxDomainHandle_t domain,
    uint64_t timestampTypeId,
    uint64_t* timeDomainIdOut)
{
    nvtxTimeDomainAttr_t timeAttr;

    if (!iface || !iface->TimeDomainRegister || !domain || !timeDomainIdOut)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    *timeDomainIdOut = NVTX_TIME_DOMAIN_ID_NONE;

    memset(&timeAttr, 0, sizeof(timeAttr));
    timeAttr.timestampTypeId = timestampTypeId;

    return iface->TimeDomainRegister(domain, &timeAttr, timeDomainIdOut);
}

/**
 * \brief Open a stream in a session.
 *
 * `domain` (from \ref nvtxwDomainRegister, or NULL for the session default
 * domain) is the stream's implicit domain for all events and counters written
 * to it. `scopePath`, when not NULL, is registered as the stream's default
 * scope. `timeDomainId` sets the stream's default time domain
 * (`NVTX_TIME_DOMAIN_ID_NONE` if none), overridden by per-timestamp semantics.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwStreamOpen(
    const nvtxwInterface_t* iface,
    nvtxwSessionHandle_t session,
    const char* streamName,
    nvtxDomainHandle_t domain,
    const char* scopePath,
    uint64_t timeDomainId,
    nvtxwStreamHandle_t* streamOut)
{
    nvtxwStreamAttributes_t streamAttr;
    uint64_t scopeId;
    nvtxwResultCode_t result;

    if (!iface || !iface->StreamOpen || !session || !streamOut)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    *streamOut = NVTX_NULLPTR;

    scopeId = NVTX_SCOPE_NONE;
    if (scopePath && *scopePath)
    {
        nvtxDomainHandle_t scopeDomain = domain;
        if (!scopeDomain)
        {
            result = nvtxwDomainRegister(
                iface,
                session,
                NVTX_NULLPTR,
                &scopeDomain);
            if (result != NVTXW_RESULT_SUCCESS) return result;
        }

        result = nvtxwScopeRegister(
            iface,
            scopeDomain,
            scopePath,
            &scopeId);
        if (result != NVTXW_RESULT_SUCCESS) return result;
    }

    result = nvtxwStreamAttributesInit(&streamAttr);
    if (result != NVTXW_RESULT_SUCCESS) return result;

    streamAttr.name = streamName;
    streamAttr.domain = domain;
    streamAttr.scopeId = scopeId;
    streamAttr.timeDomainId = timeDomainId;

    return iface->StreamOpen(session, &streamAttr, streamOut);
}

/**
 * \brief Open a stream and produce a ready-to-use event writer in one call.
 *
 * Convenience for the common case of one stream per domain: registers all event
 * helper schemas in `domain`, opens a stream on it (see \ref nvtxwStreamOpen
 * for the `scopePath` and `timeDomainId` arguments), and fills `writerOut` with
 * the interface, the opened stream, and the (by-value) schema IDs. The
 * resulting writer is passed directly to the event write functions.
 *
 * For several streams sharing one domain, prefer registering the schemas once
 * (\ref nvtxwEventSchemasRegister) and building each writer with
 * \ref nvtxwEventWriterInit, to avoid registering the schemas once per stream.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwEventWriterOpen(
    const nvtxwInterface_t* iface,
    nvtxwSessionHandle_t session,
    const char* streamName,
    nvtxDomainHandle_t domain,
    const char* scopePath,
    uint64_t timeDomainId,
    nvtxwEventWriter_t* writerOut)
{
    nvtxwEventHelperSchemaIds_t schemaIds;
    nvtxwStreamHandle_t stream;
    nvtxwResultCode_t result;

    if (!iface || !iface->SchemaRegister || !iface->StreamOpen ||
        !session || !writerOut)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    memset(writerOut, 0, sizeof(*writerOut));

    result = nvtxwEventSchemasRegister(
        iface,
        domain,
        NVTXW_EVENT_HELPER_SCHEMA_ALL,
        &schemaIds);
    if (result != NVTXW_RESULT_SUCCESS) return result;

    stream = NVTX_NULLPTR;
    result = nvtxwStreamOpen(
        iface,
        session,
        streamName,
        domain,
        scopePath,
        timeDomainId,
        &stream);
    if (result != NVTXW_RESULT_SUCCESS) return result;

    return nvtxwEventWriterInit(writerOut, iface, stream, &schemaIds);
}

#ifdef __cplusplus
}
#endif

#endif
