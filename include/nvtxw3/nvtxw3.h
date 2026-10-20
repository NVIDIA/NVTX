/*
 *  SPDX-FileCopyrightText: Copyright (c) 2023-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *  SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  Licensed under the Apache License v2.0 with LLVM Exceptions.
 *  See https://nvidia.github.io/NVTX/LICENSE.txt for license information.
 */

/** \file nvtxw3.h
 * \brief NVTX Writer API for feed-forward trace data.
 *
 * NVTXW accepts payload, counter, and timing data that was produced outside
 * the live NVTX injection path, while reusing the NVTX payload and counter
 * schema types.
 *
 * This header defines the core producer/backend contract: result codes, the
 * `GetInterface` mechanism, and the \ref nvtxwInterface_v2_t function table.
 * The optional reference loader for locating and loading a backend library
 * (\ref nvtxwLoad, config utilities) lives in nvtxw3_loader.h.
 */

#if !defined(NVTXW_API)
#define NVTXW_API

/* NVTXW uses payload and counter types, but not their implementations. Define
 * `NVTX_NO_IMPL` before this header to skip importing the implementations. */
#include <nvtx3/nvToolsExtPayload.h>
#include <nvtx3/nvToolsExtCounters.h>

#include <stddef.h> /* For size_t */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef int32_t nvtxwResultCode_t;

/**
 * \brief Result codes for NVTXW interface and loader functions.
 *
 * The library/interface codes follow the backend-loading pipeline: locate the
 * library, load it, then resolve its interface table.
 */
#define NVTXW_RESULT_SUCCESS                          0
#define NVTXW_RESULT_FAILED                           1
#define NVTXW_RESULT_INVALID_ARGUMENT                 2
#define NVTXW_RESULT_LIBRARY_NOT_FOUND                3
#define NVTXW_RESULT_LIBRARY_LOAD_FAILED              4
#define NVTXW_RESULT_LIBRARY_SYMBOL_MISSING           5
#define NVTXW_RESULT_INTERFACE_VERSION_NOT_SUPPORTED  6
/** Valid call, but the backend does not support the operation or feature. */
#define NVTXW_RESULT_NOT_SUPPORTED                    7

/*--------- Backend entry point ---------*/

/* A backend library exposes a single exported entry point, `nvtxwGetInterface`,
 * which maps an interface version to a function table. The optional reference
 * loader in nvtxw3_loader.h (`nvtxwLoad`) locates the backend and resolves this
 * symbol; an application may instead load a backend by any mechanism it prefers,
 * resolve the "nvtxwGetInterface" symbol itself, and request the desired
 * nvtxwInterface_* function table.
 *
 * Configuration: NVTXW does not define a mechanism for load-time or
 * backend-global configuration. A backend may define its own mechanism, such as
 * a backend-specific environment variable. Per-session options are passed
 * through `nvtxwSessionAttributes_t::configString`. */

/**
 * \brief The current interface version, matching the function-table suffix
 * (\ref nvtxwInterface_v2_t).
 *
 * A breaking ABI change adds a new version and table; producers request the
 * desired version and fall back on failure.
 */
#define NVTXW_INTERFACE_VERSION  2

/** \brief Name of the backend's single exported symbol used to acquire an interface. */
#define NVTXW_GET_INTERFACE_SYMBOL_NAME "nvtxwGetInterface"

/** \brief Name of the backend's optional exported finalize symbol (see \ref nvtxwFinalize_t). */
#define NVTXW_FINALIZE_SYMBOL_NAME "nvtxwFinalize"

typedef int32_t nvtxwInterfaceVersion_t;

/**
 * \brief Backend entry point: resolve a versioned interface table.
 *
 * On success returns `NVTXW_RESULT_SUCCESS` and writes the table to `ifaceOut`.
 * Returns `NVTXW_RESULT_INTERFACE_VERSION_NOT_SUPPORTED` if the backend has no
 * table for the requested version. Negotiation across major interface versions
 * is done by requesting a specific version and falling back on failure.
 */
typedef nvtxwResultCode_t (*nvtxwGetInterface_t)(
    nvtxwInterfaceVersion_t version,
    const void** ifaceOut);

/**
 * \brief Optional backend cleanup hook exported as "nvtxwFinalize".
 *
 * Frees the backend's process-global state so a tool can pass a memory checker.
 * It is a whole-backend hook, not a per-interface release, and runs after the
 * caller has ended all sessions and streams. \ref nvtxwUnload calls it on each
 * unload, so it may run more than once and repeated calls must be safe.
 */
typedef void (*nvtxwFinalize_t)(void);

/*--------- Interface v2 ---------*/

/**
 * \brief Opaque NVTXW session handle.
 *
 * Session handles are created by SessionBegin and remain valid until SessionEnd
 * is called for that session.
 */
typedef struct nvtxwSession_st* nvtxwSessionHandle_t;

/**
 * \brief Opaque NVTXW stream handle.
 *
 * Stream handles are created by StreamOpen and remain valid until StreamClose,
 * or until SessionEnd for the owning session.
 */
typedef struct nvtxwStream_st* nvtxwStreamHandle_t;

/* NVTXW reuses the NVTX domain handle type (nvtxDomainHandle_t). Domain handles
 * are created by DomainRegister, owned by their session, and remain valid until
 * SessionEnd is called for that session. */

/** Attributes used to create an NVTXW session. */
typedef struct nvtxwSessionAttributes_v1
{
    /** Size of this struct in bytes (set to `sizeof(nvtxwSessionAttributes_t)`).
     * Guaranteed to increase when new members are added at the end. */
    size_t structSize;

    /**
     * \brief Name of the session.
     *
     * Tools may display this name, or use it to name a file or directory
     * representing the session.
     */
    const char* name;

    /**
     * \brief String containing configuration options for the session.
     *
     * Format is key=value entries separated by any of \\r (carriage return),
     * \\n (line feed), or | (pipe); a delimiter must not appear in a key or
     * value. The first = (equals sign) separates the key from the value, so a
     * key must not contain =. Tools shall use reasonable defaults for supported
     * options the caller omits, and ignore keys they do not support.
     * See tool-specific documentation for lists of supported keys.
     */
    const char* configString;
} nvtxwSessionAttributes_t;

/** Attributes used to register or retrieve an NVTXW domain. */
typedef struct nvtxwDomainAttributes_v1
{
    /** Size of this struct in bytes (set to `sizeof(nvtxwDomainAttributes_t)`).
     * Guaranteed to increase when new members are added at the end. */
    size_t structSize;

    /**
     * \brief Name of the NVTX domain.
     *
     * NULL or an empty string means the session default domain. Within a
     * session, registering the same name returns the same domain handle.
     */
    const char* name;
} nvtxwDomainAttributes_t;

/* Define whether event ordering in a stream is based on the scope. */

/**
 * Ordering is defined at the stream level, independent of scopes within the stream.
 */
#define NVTXW_STREAM_ORDER_INTERLEAVING_NONE          NVTX_STATIC_CAST(int16_t, 0)

/**
 * Ordering is defined at the scope level. This means ordering guarantees
 * described by the other fields only apply to events or counters of the same
 * scope within the stream. The order of events or counters in different scopes
 * is unspecified.
 */
#define NVTXW_STREAM_ORDER_INTERLEAVING_SCOPE         NVTX_STATIC_CAST(int16_t, 1)


/* Define how events are fully or partially sorted in a stream. */

/**
 * No guarantees can be made about event ordering in the stream.
 * Events may need to be sorted by the tool.
 */
#define NVTXW_STREAM_ORDERING_TYPE_UNKNOWN            NVTX_STATIC_CAST(int16_t, 0)

/**
 * All events represent single points in time and are fully or
 * partially sorted in the order in which they occurred.
 */
#define NVTXW_STREAM_ORDERING_TYPE_STRICT             NVTX_STATIC_CAST(int16_t, 1)

/**
 * Events that represent single points in time are fully or
 * partially sorted in the order in which they occurred, and
 * events representing time ranges in order of begin time.
 */
#define NVTXW_STREAM_ORDERING_TYPE_PACKED_RANGE_START NVTX_STATIC_CAST(int16_t, 2)

/**
 * Events that represent single points in time are fully or
 * partially sorted in the order in which they occurred, and
 * events representing time ranges in order of end time.
 */
#define NVTXW_STREAM_ORDERING_TYPE_PACKED_RANGE_END   NVTX_STATIC_CAST(int16_t, 3)

/* Define how to quantify skid when events are partially sorted. Only considered
 * when orderingType is not UNKNOWN. Which events in the stream this applies to
 * depends on the value of orderInterleaving. Which timestamp is used for ordering
 * in an event with multiple timestamps depends on the value of orderingType. */

/** Events are fully sorted. */
#define NVTXW_STREAM_ORDERING_SKID_NONE          NVTX_STATIC_CAST(int32_t, 0)

/**
 * Events are partially sorted. The orderingSkidAmount field defines "skid" as
 * a number of nanoseconds. For any two events A and B in the stream or scope
 * (depending on interleaving level), where A is written into the stream before
 * B, the tool must handle the case where B has a lower timestamp than A, but
 * can assume B's timestamp cannot be more than the "skid" number of nanoseconds
 * earlier than A's timestamp. Note that timestamp values in events cannot be
 * assumed to be in units of nanoseconds, so this value cannot be added directly
 * to timestamp values without conversion.
 */
#define NVTXW_STREAM_ORDERING_SKID_TIME_NS       NVTX_STATIC_CAST(int32_t, 1)

/**
 * Events are partially sorted. The orderingSkidAmount field defines "skid" as
 * a number of events. Regarding only events in a stream or scope (depending on
 * interleaving level), for any event A, the next "skid" number of events after
 * A may have a lower timestamp than A (by any amount of time), but no events
 * written after that can have a lower timestamp than A.
 */
#define NVTXW_STREAM_ORDERING_SKID_EVENT_COUNT   NVTX_STATIC_CAST(int32_t, 2)

/** Attributes used to open an NVTXW stream. */
typedef struct nvtxwStreamAttributes_v1
{
    /** Size of this struct in bytes (set to `sizeof(nvtxwStreamAttributes_t)`).
     * Guaranteed to increase when new members are added at the end. */
    size_t structSize;

    /**
     * \brief Name of the stream.
     *
     * Tools typically will not display stream names. No two streams in the
     * same session may have the same name.
     */
    const char* name;

    /**
     * \brief Domain used implicitly for all events and counters written into
     * this stream.
     *
     * NULL means the session default domain. Schemas, enums, scopes, counters,
     * strings, and resources used by data written to this stream must be
     * registered in this domain. Multiple streams may share a domain; all of
     * them see the entities registered in it.
     */
    nvtxDomainHandle_t domain;

    /**
     * \brief Default scope for events and counters written into this stream.
     *
     * A per-event scope or process/thread payload entry overrides it. Other
     * values than `NVTX_SCOPE_NONE` or `NVTX_SCOPE_ROOT` must be registered in
     * this stream's domain. Runtime-resolved scopes (`NVTX_SCOPE_CURRENT_*`)
     * are invalid here.
     */
    uint64_t scopeId;

    /**
     * \brief Default time domain for event and counter timestamps written into
     * this stream.
     *
     * A per-event or per-counter time domain overrides it. A non-zero value
     * must be a predefined `NVTX_TIMESTAMP_TYPE_*` value or a time domain ID
     * registered in this stream's domain.
     */
    uint64_t timeDomainId;

    /**
     * \name Event ordering
     * Information about event ordering inside the stream. See comments for the
     * ordering macros above.
     * @{
     */
    int16_t orderInterleaving;  /**< NVTXW_STREAM_ORDER_INTERLEAVING_*    */
    int16_t orderingType;       /**< NVTXW_STREAM_ORDERING_TYPE_*         */
    int32_t orderingSkid;       /**< NVTXW_STREAM_ORDERING_SKID_*         */
    int64_t orderingSkidAmount; /**< Numeric value, dependent on skid type */
    /** @} */
} nvtxwStreamAttributes_t;

/**
 * \brief Function table for the NVTX Writer (NVTXW) API.
 *
 * Breaking changes will not be made to this interface without also introducing
 * a new interface version requested from \ref nvtxwGetInterface_t (and a new
 * versioned table).
 *
 * The table is fixed-size for the major version's lifetime: its baseline
 * members precede the trailing `reserved` array, and later minor versions add
 * members in `reserved` slots without changing any offset. Every member address
 * stays valid, so a consumer need only NULL-check it (\ref NVTXW_INTERFACE_HAS)
 * before calling.
 *
 * Contract:
 *   - Unimplemented slots MUST read as NULL to the consumer (whether producer
 *     or backend zero-initializes the table is incidental).
 *   - Baseline members MUST be implemented and non-NULL. A backend that does
 *     not support a baseline operation MUST still provide the member and return
 *     `NVTXW_RESULT_NOT_SUPPORTED` from it, rather than leaving the slot NULL.
 *   - Members added by later minor versions MAY be NULL on older backends.
 *   - Unless a member documents otherwise, pointer arguments are only valid for
 *     the duration of the call; the backend MUST NOT retain them and must copy
 *     anything it needs before returning. Handles and IDs returned through
 *     output pointers keep their own documented lifetimes.
 *
 * All `*Register` APIs are tied to the NVTX domain. Register an entity before
 * anything that references it, so consumers can resolve IDs in one pass.
 */
typedef struct nvtxwInterface_v2
{
    /**
     * \brief Create a session, which represents a collection of trace data from
     * one or more streams.
     */
    nvtxwResultCode_t (*SessionBegin)(
        const nvtxwSessionAttributes_t* attr,
        nvtxwSessionHandle_t* sessionOut);

    /**
     * \brief Notify the implementation that all trace data for the session has
     * been provided, and the session may be destroyed.
     *
     * Depending on configuration options, ending a session may trigger behavior
     * like writing an output file or opening a data viewer.
     */
    nvtxwResultCode_t (*SessionEnd)(
        nvtxwSessionHandle_t session);

    /**
     * \brief Register or retrieve a domain within a session.
     *
     * If `attr` is NULL, `attr->name` is NULL, or `attr->name` is an empty
     * string, the session default domain is used. Domain handles are
     * session-owned and valid until SessionEnd. Re-registering the same domain
     * name in one session returns the existing domain handle.
     */
    nvtxwResultCode_t (*DomainRegister)(
        nvtxwSessionHandle_t session,
        const nvtxwDomainAttributes_t* attr,
        nvtxDomainHandle_t* domainOut);

    /**
     * \brief Name an NVTX category in a domain.
     *
     * The domain handle must not be NULL. `category` is the caller-defined ID
     * events reference through an `NVTX_PAYLOAD_ENTRY_TYPE_CATEGORY` payload
     * entry; 0 is reserved to mean "no category".
     */
    nvtxwResultCode_t (*CategoryRegister)(
        nvtxDomainHandle_t domain,
        uint32_t category,
        const char* name);

    /**
     * \brief Register an immutable string in a domain.
     *
     * The domain handle must not be NULL. Events in the same domain may use the
     * returned handle in place of the string. It is recommended to use string
     * registration if the string will be used many times.
     */
    nvtxwResultCode_t (*StringRegister)(
        nvtxDomainHandle_t domain,
        const char* string,
        nvtxStringHandle_t* stringHandleOut);

    /**
     * \brief Register resource metadata in a domain.
     *
     * The domain handle must not be NULL.
     */
    nvtxwResultCode_t (*ResourceRegister)(
        nvtxDomainHandle_t domain,
        const nvtxResourceAttributes_t* attr);

    /**
     * \brief Register a scope in a domain.
     *
     * The domain handle must not be NULL. `scopeIdOut` receives the registered
     * ID and must not be NULL unless a static ID is given in `attr->scopeId`,
     * which must then be unique within the domain, >= `NVTX_SCOPE_ID_STATIC_START`,
     * and < `NVTX_SCOPE_ID_DYNAMIC_START`. `NVTX_SCOPE_CURRENT_*` is invalid for
     * `attr->scopeId`.
     *
     * For NVTXW, `attr->parentScope` must be `NVTX_SCOPE_ROOT`,
     * `NVTX_SCOPE_NONE`, a system anchor (`NVTX_SCOPE_CURRENT_HW_MACHINE` or
     * `NVTX_SCOPE_CURRENT_VM`), or a scope registered in this domain. Other
     * `NVTX_SCOPE_CURRENT_*` values are invalid. A backend resolves a system
     * anchor against the output report's machine or VM when unambiguous.
     */
    nvtxwResultCode_t (*ScopeRegister)(
        nvtxDomainHandle_t domain,
        const nvtxScopeAttr_t* attr,
        uint64_t* scopeIdOut);

    /**
     * \brief Register a payload schema in a domain.
     *
     * A schema describes the binary layout of a payload. The domain handle must
     * not be NULL. Events in the same domain may use the registered schema. The
     * same schema ID may be reused in different domains without collision.
     * Schema IDs and enum IDs share one namespace within a domain.
     * `schemaIdOut` receives the registered ID and must not be NULL unless
     * `attr->schemaId`provides a static schema ID. Static schema IDs must be
     * unique within the domain,>= `NVTX_PAYLOAD_SCHEMA_ID_STATIC_START`, and
     * < `NVTX_PAYLOAD_SCHEMA_ID_DYNAMIC_START`.
     */
    nvtxwResultCode_t (*SchemaRegister)(
        nvtxDomainHandle_t domain,
        const nvtxPayloadSchemaAttr_t* attr,
        uint64_t* schemaIdOut);

    /**
     * \brief Register an enum schema in a domain.
     *
     * The enum schema maps enum values to their name strings. The domain handle
     * must not be NULL. Schema and enum IDs share one namespace within a domain.
     * `enumIdOut` receives the registered ID and must not be NULL unless
     * `attr->schemaId` provides a static enum ID. Static enum IDs must be
     * unique within the domain, >= `NVTX_PAYLOAD_SCHEMA_ID_STATIC_START`, and
     * < `NVTX_PAYLOAD_SCHEMA_ID_DYNAMIC_START`.
     */
    nvtxwResultCode_t (*EnumRegister)(
        nvtxDomainHandle_t domain,
        const nvtxPayloadEnumAttr_t* attr,
        uint64_t* enumIdOut);

    /**
     * \brief Register a counter or counter group in a domain.
     *
     * The domain handle must not be NULL. `counterIdOut` receives the
     * registered ID and must not be NULL unless `attr->counterId` provides a
     * static counter ID, which must be unique within the domain,
     * >= `NVTX_COUNTER_ID_STATIC_START`, and < `NVTX_COUNTER_ID_DYNAMIC_START`.
     * If a scope ID other than `NVTX_SCOPE_NONE` or `NVTX_SCOPE_ROOT` is
     * provided in `attr->scopeId`, it must refer to a scope registered in this
     * domain. Runtime-resolved scopes (`NVTX_SCOPE_CURRENT_*`) are invalid here.
     */
    nvtxwResultCode_t (*CounterRegister)(
        nvtxDomainHandle_t domain,
        const nvtxCounterAttr_t* attr,
        uint64_t* counterIdOut);

    /**
     * \brief Register a time domain in an NVTX domain.
     *
     * The domain handle must not be NULL. `timeDomainIdOut` receives the
     * registered ID and must not be NULL unless `attr->timeDomainId` provides a
     * static time domain ID, which must be unique within the domain,
     * >= `NVTX_TIME_DOMAIN_ID_STATIC_START`, and
     * < `NVTX_TIME_DOMAIN_ID_DYNAMIC_START`.
     */
    nvtxwResultCode_t (*TimeDomainRegister)(
        nvtxDomainHandle_t domain,
        const nvtxTimeDomainAttr_t* attr,
        uint64_t* timeDomainIdOut);

    /**
     * \brief Open a stream within a session.
     *
     * A stream is the object events and counters are written to. Its domain,
     * default scope, and default time domain are set through
     * \ref nvtxwStreamAttributes_t. The stream references but does not own them;
     * they remain owned by the session until SessionEnd.
     */
    nvtxwResultCode_t (*StreamOpen)(
        nvtxwSessionHandle_t session,
        const nvtxwStreamAttributes_t* attr,
        nvtxwStreamHandle_t* streamOut);

    /**
     * \brief Close the stream.
     *
     * Closing a stream only releases the stream object; it does not signal that
     * data collection is finished. Use SessionEnd to indicate that all data for
     * the session has been provided.
     */
    nvtxwResultCode_t (*StreamClose)(
        nvtxwStreamHandle_t stream);

    /**
     * \brief Write a single event into the stream.
     *
     * The payloads together form a single event: each payload's `schemaId`
     * defines its byte layout, and entries may reference other payloads by
     * their index in this array. Timestamp entries' time semantics override
     * the stream default time domain.
     */
    nvtxwResultCode_t (*EventWrite)(
        nvtxwStreamHandle_t stream,
        const nvtxPayloadData_t* payloads,
        size_t payloadCount);

    /**
     * \brief Write a batch of events into the stream.
     *
     * All events in the batch share a single schema (`eventBatch->eventSchemaId`);
     * see `nvtxEventBatch_t` for the event array, scope, ordering, and flexible
     * data it carries. Timestamp entries' time semantics override the stream
     * default time domain.
     */
    nvtxwResultCode_t (*EventBatchWrite)(
        nvtxwStreamHandle_t stream,
        const nvtxEventBatch_t* eventBatch);

    /**
     * \brief Write a counter sample into the stream.
     *
     * The counter ID refers to the counter attributes, which include the name,
     * counter group layout (schema), etc. `data` points to a single sample with
     * potentially multiple values. Counter time semantics override the stream
     * default time domain.
     */
    nvtxwResultCode_t (*CounterWrite)(
        nvtxwStreamHandle_t stream,
        int64_t timestamp,
        uint64_t counterId,
        const void* data,
        size_t size);

    /**
     * \brief Write a no-value counter sample into the stream.
     *
     * `reason` is one of the `NVTX_COUNTER_SAMPLE_*` values, indicating why the
     * sample has no value. The counter ID refers to the registered counter
     * (group). Counter time semantics override the stream default time domain.
     */
    nvtxwResultCode_t (*CounterNoValueWrite)(
        nvtxwStreamHandle_t stream,
        int64_t timestamp,
        uint64_t counterId,
        uint8_t reason);

    /**
     * \brief Write a batch of counters into the stream.
     *
     * All counter samples share the same layout and attributes
     * (`counterBatch->counterId`). Time semantics override the stream default
     * time domain.
     */
    nvtxwResultCode_t (*CounterBatchWrite)(
        nvtxwStreamHandle_t stream,
        const nvtxCounterBatch_t* counterBatch);

    /**
     * \brief Write a synchronization point relating two time domains.
     *
     * Records that the two timestamps, one per time domain, denote the same
     * point in time. Two such points let a tool derive the conversion between
     * the time domains.
     */
    nvtxwResultCode_t (*TimeSyncPointWrite)(
        nvtxwStreamHandle_t stream,
        uint64_t timeDomainId1,
        uint64_t timeDomainId2,
        int64_t timestamp1,
        int64_t timestamp2);

    /**
     * \brief Write a batch of synchronization points.
     *
     * Each `nvtxSyncPoint_t` entry pairs a source and destination timestamp
     * that denote the same point in time, letting a tool derive the conversion
     * between the two time domains.
     */
    nvtxwResultCode_t (*TimeSyncPointTableWrite)(
        nvtxwStreamHandle_t stream,
        uint64_t timeDomainIdSrc,
        uint64_t timeDomainIdDst,
        const nvtxSyncPoint_t* syncPoints,
        size_t count);

    /**
     * \brief Write a linear conversion between two time domains.
     *
     * An alternative to supplying two synchronization points: `slope` and an
     * anchor pair of timestamps define the linear conversion between the two
     * time domains.
     */
    nvtxwResultCode_t (*TimestampConversionFactorWrite)(
        nvtxwStreamHandle_t stream,
        uint64_t timeDomainIdSrc,
        uint64_t timeDomainIdDst,
        double slope,
        int64_t timestampSrc,
        int64_t timestampDst);

    /** Reserved function pointer slots for future minor versions.
     * Never reorder or insert above the baseline. */
    void (*reserved[43])(void);

} nvtxwInterface_v2_t;

/**
 * \brief Current interface alias.
 *
 * Code that requires a specific interface version can use the versioned typedef
 * above and matching interface version.
 */
typedef nvtxwInterface_v2_t nvtxwInterface_t;

/**
 * \brief Nonzero if `iface` is non-NULL and `member` is implemented (non-NULL),
 * so the member is safe to call, e.g.:
 * \code
 *     if (NVTXW_INTERFACE_HAS(iface, EventWrite))
 *         iface->EventWrite(stream, payloads, count);
 * \endcode
 * `iface` is evaluated more than once; pass a plain pointer, not an expression
 * with side effects.
 */
#define NVTXW_INTERFACE_HAS(iface, member) \
    ((iface) != NVTX_NULLPTR && (iface)->member != NVTX_NULLPTR)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NVTXW_API */
