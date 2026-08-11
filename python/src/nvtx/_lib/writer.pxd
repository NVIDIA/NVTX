# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://nvidia.github.io/NVTX/LICENSE.txt for license information.

from libc.stddef cimport size_t
from libc.stdint cimport (
    int16_t,
    int32_t,
    int64_t,
    uint8_t,
    uint32_t,
    uint64_t,
    uintptr_t,
)

from nvtx._lib.counters cimport nvtxCounterAttr_t, nvtxCounterBatch_t
from nvtx._lib.lib cimport (
    SchemaRegistrar,
    nvtxPayloadData_t,
    nvtxPayloadSchemaAttr_t,
)


cdef extern from "nvtxw3/nvtxw3.h" nogil:

    ctypedef int32_t nvtxwResultCode_t
    ctypedef int32_t nvtxwInterfaceVersion_t

    int NVTXW_RESULT_SUCCESS
    int NVTXW_RESULT_FAILED
    int NVTXW_RESULT_INVALID_ARGUMENT
    int NVTXW_RESULT_LIBRARY_NOT_FOUND
    int NVTXW_RESULT_LIBRARY_LOAD_FAILED
    int NVTXW_RESULT_LIBRARY_SYMBOL_MISSING
    int NVTXW_RESULT_INTERFACE_VERSION_NOT_SUPPORTED
    int NVTXW_RESULT_NOT_SUPPORTED

    int NVTXW_INTERFACE_VERSION

    const char* NVTXW_GET_INTERFACE_SYMBOL_NAME
    const char* NVTXW_FINALIZE_SYMBOL_NAME

    uint64_t NVTX_SCOPE_NONE
    uint64_t NVTX_SCOPE_ROOT
    uint64_t NVTX_SCOPE_CURRENT_VM
    uint64_t NVTX_SCOPE_ID_STATIC_START
    uint64_t NVTX_SCOPE_ID_DYNAMIC_START
    uint64_t NVTX_TIME_DOMAIN_ID_NONE

    int NVTXW_STREAM_ORDER_INTERLEAVING_NONE
    int NVTXW_STREAM_ORDER_INTERLEAVING_SCOPE

    int NVTXW_STREAM_ORDERING_TYPE_UNKNOWN
    int NVTXW_STREAM_ORDERING_TYPE_STRICT
    int NVTXW_STREAM_ORDERING_TYPE_PACKED_RANGE_START
    int NVTXW_STREAM_ORDERING_TYPE_PACKED_RANGE_END

    int NVTXW_STREAM_ORDERING_SKID_NONE
    int NVTXW_STREAM_ORDERING_SKID_TIME_NS
    int NVTXW_STREAM_ORDERING_SKID_EVENT_COUNT

    ctypedef struct nvtxEventBatch_t:
        uint64_t eventSchemaId
        size_t size
        const void* events
        uint64_t scope
        uint64_t flags
        const void* flexData
        size_t flexDataSize
        size_t flexDataOffset

    ctypedef nvtxwResultCode_t (*nvtxwGetInterface_t)(
        nvtxwInterfaceVersion_t version,
        const void** ifaceOut)

    cdef struct nvtxwSession_st:
        pass
    ctypedef nvtxwSession_st* nvtxwSessionHandle_t

    cdef struct nvtxwStream_st:
        pass
    ctypedef nvtxwStream_st* nvtxwStreamHandle_t

    cdef struct nvtxDomainRegistration_st:
        pass
    ctypedef nvtxDomainRegistration_st* nvtxDomainHandle_t

    cdef struct nvtxStringRegistration_st:
        pass
    ctypedef nvtxStringRegistration_st* nvtxStringHandle_t

    ctypedef struct nvtxScopeAttr_t:
        size_t structSize
        const char* path
        uint64_t parentScope
        uint64_t scopeId

    ctypedef struct nvtxwSessionAttributes_t:
        size_t structSize
        const char* name
        const char* configString

    ctypedef struct nvtxwDomainAttributes_t:
        size_t structSize
        const char* name

    ctypedef struct nvtxwStreamAttributes_t:
        size_t structSize
        const char* name
        nvtxDomainHandle_t domain
        uint64_t scopeId
        uint64_t timeDomainId
        int16_t orderInterleaving
        int16_t orderingType
        int32_t orderingSkid
        int64_t orderingSkidAmount

    ctypedef struct nvtxResourceAttributes_t:
        pass

    ctypedef struct nvtxPayloadEnumAttr_t:
        pass

    ctypedef struct nvtxTimeDomainAttr_t:
        pass

    ctypedef struct nvtxSyncPoint_t:
        pass

    ctypedef struct nvtxwInterface_v2_t:
        nvtxwResultCode_t (*SessionBegin)(
            const nvtxwSessionAttributes_t* attr,
            nvtxwSessionHandle_t* sessionOut)
        nvtxwResultCode_t (*SessionEnd)(
            nvtxwSessionHandle_t session)
        nvtxwResultCode_t (*DomainRegister)(
            nvtxwSessionHandle_t session,
            const nvtxwDomainAttributes_t* attr,
            nvtxDomainHandle_t* domainOut)
        nvtxwResultCode_t (*CategoryRegister)(
            nvtxDomainHandle_t domain,
            uint32_t category,
            const char* name)
        nvtxwResultCode_t (*StringRegister)(
            nvtxDomainHandle_t domain,
            const char* string,
            nvtxStringHandle_t* stringHandleOut)
        nvtxwResultCode_t (*ResourceRegister)(
            nvtxDomainHandle_t domain,
            const nvtxResourceAttributes_t* attr)
        nvtxwResultCode_t (*ScopeRegister)(
            nvtxDomainHandle_t domain,
            const nvtxScopeAttr_t* attr,
            uint64_t* scopeIdOut)
        nvtxwResultCode_t (*SchemaRegister)(
            nvtxDomainHandle_t domain,
            const nvtxPayloadSchemaAttr_t* attr,
            uint64_t* schemaIdOut)
        nvtxwResultCode_t (*EnumRegister)(
            nvtxDomainHandle_t domain,
            const nvtxPayloadEnumAttr_t* attr,
            uint64_t* enumIdOut)
        nvtxwResultCode_t (*CounterRegister)(
            nvtxDomainHandle_t domain,
            const nvtxCounterAttr_t* attr,
            uint64_t* counterIdOut)
        nvtxwResultCode_t (*TimeDomainRegister)(
            nvtxDomainHandle_t domain,
            const nvtxTimeDomainAttr_t* attr,
            uint64_t* timeDomainIdOut)
        nvtxwResultCode_t (*StreamOpen)(
            nvtxwSessionHandle_t session,
            const nvtxwStreamAttributes_t* attr,
            nvtxwStreamHandle_t* streamOut)
        nvtxwResultCode_t (*StreamClose)(
            nvtxwStreamHandle_t stream)
        nvtxwResultCode_t (*EventWrite)(
            nvtxwStreamHandle_t stream,
            const nvtxPayloadData_t* payloads,
            size_t payloadCount)
        nvtxwResultCode_t (*EventBatchWrite)(
            nvtxwStreamHandle_t stream,
            const nvtxEventBatch_t* eventBatch)
        nvtxwResultCode_t (*CounterWrite)(
            nvtxwStreamHandle_t stream,
            int64_t timestamp,
            uint64_t counterId,
            const void* data,
            size_t size)
        nvtxwResultCode_t (*CounterNoValueWrite)(
            nvtxwStreamHandle_t stream,
            int64_t timestamp,
            uint64_t counterId,
            uint8_t reason)
        nvtxwResultCode_t (*CounterBatchWrite)(
            nvtxwStreamHandle_t stream,
            const nvtxCounterBatch_t* counterBatch)
        nvtxwResultCode_t (*TimeSyncPointWrite)(
            nvtxwStreamHandle_t stream,
            uint64_t timeDomainId1,
            uint64_t timeDomainId2,
            int64_t timestamp1,
            int64_t timestamp2)
        nvtxwResultCode_t (*TimeSyncPointTableWrite)(
            nvtxwStreamHandle_t stream,
            uint64_t timeDomainIdSrc,
            uint64_t timeDomainIdDst,
            const nvtxSyncPoint_t* syncPoints,
            size_t count)
        nvtxwResultCode_t (*TimestampConversionFactorWrite)(
            nvtxwStreamHandle_t stream,
            uint64_t timeDomainIdSrc,
            uint64_t timeDomainIdDst,
            double slope,
            int64_t timestampSrc,
            int64_t timestampDst)
        void (*reserved[43])()


cdef extern from "nvtxw3/nvtxw3_counter_helpers.h" nogil:

    nvtxwResultCode_t nvtxwCounterInt64Write(
        const nvtxwInterface_v2_t* iface,
        nvtxwStreamHandle_t stream,
        int64_t timestamp,
        uint64_t counterId,
        int64_t value)

    nvtxwResultCode_t nvtxwCounterFloat64Write(
        const nvtxwInterface_v2_t* iface,
        nvtxwStreamHandle_t stream,
        int64_t timestamp,
        uint64_t counterId,
        double value)


cdef extern from "nvtxw3/nvtxw3_event_helpers.h" nogil:

    ctypedef uint64_t nvtxRangeId_t

    ctypedef struct nvtxwEventAttributes_t:
        uint32_t color
        uint32_t category
        nvtxStringHandle_t message

    ctypedef struct nvtxwEventAttributesUtf8_t:
        uint32_t color
        uint32_t category
        uint32_t messageLength
        const char* message

    ctypedef struct nvtxwEventHelperSchemaIds_t:
        uint64_t markUtf8
        uint64_t markRegString
        uint64_t rangePushPopUtf8
        uint64_t rangePushPopRegString
        uint64_t rangeStartEndUtf8
        uint64_t rangeStartEndRegString
        uint64_t rangePushUtf8
        uint64_t rangePushRegString
        uint64_t rangePop
        uint64_t rangeStartUtf8
        uint64_t rangeStartRegString
        uint64_t rangeEnd
        uint64_t eventMessageUtf8

    ctypedef struct nvtxwEventWriter_t:
        const nvtxwInterface_v2_t* iface
        nvtxwStreamHandle_t stream
        nvtxwEventHelperSchemaIds_t schemaIds

    int NVTXW_EVENT_HELPER_SCHEMA_ALL

    nvtxwResultCode_t nvtxwEventSchemasRegister(
        const nvtxwInterface_v2_t* iface,
        nvtxDomainHandle_t domain,
        uint32_t schemaMask,
        nvtxwEventHelperSchemaIds_t* schemaIdsOut)

    nvtxwResultCode_t nvtxwMarkWriteUtf8(
        const nvtxwEventWriter_t* writer,
        int64_t timestamp,
        nvtxwEventAttributesUtf8_t attr)
    nvtxwResultCode_t nvtxwMarkWrite(
        const nvtxwEventWriter_t* writer,
        int64_t timestamp,
        nvtxwEventAttributes_t attr)

    nvtxwResultCode_t nvtxwRangePushPopWriteUtf8(
        const nvtxwEventWriter_t* writer,
        int64_t timestampBegin,
        int64_t timestampEnd,
        nvtxwEventAttributesUtf8_t attr)
    nvtxwResultCode_t nvtxwRangePushPopWrite(
        const nvtxwEventWriter_t* writer,
        int64_t timestampBegin,
        int64_t timestampEnd,
        nvtxwEventAttributes_t attr)

    nvtxwResultCode_t nvtxwRangeStartEndWriteUtf8(
        const nvtxwEventWriter_t* writer,
        int64_t timestampBegin,
        int64_t timestampEnd,
        nvtxwEventAttributesUtf8_t attr)
    nvtxwResultCode_t nvtxwRangeStartEndWrite(
        const nvtxwEventWriter_t* writer,
        int64_t timestampBegin,
        int64_t timestampEnd,
        nvtxwEventAttributes_t attr)

    nvtxwResultCode_t nvtxwRangePushWriteUtf8(
        const nvtxwEventWriter_t* writer,
        int64_t timestamp,
        nvtxwEventAttributesUtf8_t attr)
    nvtxwResultCode_t nvtxwRangePushWrite(
        const nvtxwEventWriter_t* writer,
        int64_t timestamp,
        nvtxwEventAttributes_t attr)

    nvtxwResultCode_t nvtxwRangePopWrite(
        const nvtxwEventWriter_t* writer,
        int64_t timestamp)

    nvtxwResultCode_t nvtxwRangeStartWriteUtf8(
        const nvtxwEventWriter_t* writer,
        int64_t timestamp,
        nvtxRangeId_t rangeId,
        nvtxwEventAttributesUtf8_t attr)
    nvtxwResultCode_t nvtxwRangeStartWrite(
        const nvtxwEventWriter_t* writer,
        int64_t timestamp,
        nvtxRangeId_t rangeId,
        nvtxwEventAttributes_t attr)

    nvtxwResultCode_t nvtxwRangeEndWrite(
        const nvtxwEventWriter_t* writer,
        int64_t timestamp,
        nvtxRangeId_t rangeId)


cdef class _BackendModule:
    cdef const nvtxwInterface_v2_t* _iface
    cdef object _lib
    cdef Py_ssize_t _refcount
    cdef void _initialize(self) except *
    cdef void _finalize(self) except *


cdef class Backend:
    cdef const nvtxwInterface_v2_t* _iface
    cdef object _module
    cdef object _lock
    cdef set _active_sessions
    cdef void _ensure_open(self) except *


cdef class Session:
    cdef Backend _backend
    cdef bytes _name
    cdef bytes _config
    cdef nvtxwSessionHandle_t _handle
    cdef object _lock
    cdef set _open_streams
    cdef dict _domains


cdef class RegisteredString:
    cdef nvtxStringHandle_t _handle
    cdef str _string
    cdef object _domain


cdef class Scope:
    cdef uint64_t _scope_id
    cdef object _path
    cdef object _domain


cdef class Domain:
    cdef Backend _backend
    cdef Session _session
    cdef bint _valid
    cdef nvtxDomainHandle_t _handle
    cdef bint _event_schemas_registered
    cdef nvtxwEventHelperSchemaIds_t _event_schema_ids
    cdef object _get_string_cached
    cdef object _get_scope_cached
    cdef dict _categories
    cdef dict _schemas
    cdef object _get_counter_cached
    cdef object _category_ids
    cdef set _user_category_ids
    cdef SchemaRegistrar _schema_registrar
    cdef _invalidate(self)
    cdef _ensure_valid(self)
    cdef _get_event_schema_ids(
        self, nvtxwEventHelperSchemaIds_t* schema_ids_out
    )


cdef class Counter:
    cdef Domain _domain
    cdef object _name
    cdef object _dtype
    cdef object _description
    cdef object _semantics
    cdef uint64_t _scope_id
    cdef uint64_t _counter_id
    cdef uint64_t _schema_id
    cdef object _timestamp_field


cdef class Schema:
    cdef Domain _domain
    cdef uint64_t _schema_id
    cdef object _dtype
    cdef object _kind


# Signatures of the bin/utf8 nvtxw write functions, grouped by event shape
# (single timestamp, begin/end span), so the Stream._emit_* helpers can
# dispatch to a function pair instead of duplicating each call.
ctypedef nvtxwResultCode_t (*_at_bin_fn)(
    const nvtxwEventWriter_t*, int64_t, nvtxwEventAttributes_t)
ctypedef nvtxwResultCode_t (*_at_utf8_fn)(
    const nvtxwEventWriter_t*, int64_t, nvtxwEventAttributesUtf8_t)
ctypedef nvtxwResultCode_t (*_span_bin_fn)(
    const nvtxwEventWriter_t*, int64_t, int64_t, nvtxwEventAttributes_t)
ctypedef nvtxwResultCode_t (*_span_utf8_fn)(
    const nvtxwEventWriter_t*, int64_t, int64_t, nvtxwEventAttributesUtf8_t)


cdef class Stream:
    cdef Backend _backend
    cdef Session _session
    cdef Domain _domain
    cdef bytes _name
    cdef uint64_t _scope_id
    cdef uint64_t _time_domain_id
    cdef int16_t _order_interleaving
    cdef int16_t _ordering_type
    cdef int32_t _ordering_skid
    cdef int64_t _ordering_skid_amount
    cdef bint _writer_ready
    cdef nvtxwEventWriter_t _writer
    cdef void _ensure_writable(self) except *
    cdef void _ensure_counter(self, Counter counter) except *
    cdef nvtxwEventWriter_t* _get_writer(self) except NULL
    cdef object _resolve_event_attrs(
        self,
        object message,
        object color,
        object category,
        nvtxwEventAttributes_t* attr,
        nvtxwEventAttributesUtf8_t* uattr,
    )
    cdef void _emit_at(
        self, int64_t timestamp, object message, object color, object category,
        _at_bin_fn bin_fn, _at_utf8_fn utf8_fn, str op, str utf8_op,
    ) except *
    cdef void _emit_span(
        self, int64_t begin, int64_t end, object message, object color,
        object category, _span_bin_fn bin_fn, _span_utf8_fn utf8_fn,
        str op, str utf8_op,
    ) except *


cdef bytes _as_bytes(object s)
cdef object _as_str(object s)
