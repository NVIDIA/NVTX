# SPDX-FileCopyrightText: Copyright (c) 2020-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

from libc.stdint cimport uint8_t, uint16_t, uint32_t, uint64_t, int32_t, int64_t
from libc.stddef cimport wchar_t

cdef extern from "nvtx3/nvToolsExt.h" nogil:

    cdef int NVTX_VERSION
    cdef int NVTX_EVENT_ATTRIB_STRUCT_SIZE

    cdef void nvtxInitialize(const void* reserved)

    cdef struct nvtxDomainRegistration_st:
        pass
    ctypedef struct nvtxDomainRegistration_st:
        pass
    ctypedef nvtxDomainRegistration_st nvtxDomainRegistration
    ctypedef nvtxDomainRegistration* nvtxDomainHandle_t

    cdef struct nvtxStringRegistration_st:
        pass
    ctypedef nvtxStringRegistration_st nvtxStringRegistration
    ctypedef nvtxStringRegistration* nvtxStringHandle_t

    ctypedef uint64_t nvtxRangeId_t

    ctypedef enum nvtxColorType_t:
        NVTX_COLOR_UNKNOWN = 0
        NVTX_COLOR_ARGB = 1

    ctypedef enum nvtxMessageType_t:
        NVTX_MESSAGE_UNKNOWN = 0
        NVTX_MESSAGE_TYPE_ASCII = 1
        NVTX_MESSAGE_TYPE_UNICODE = 2
        NVTX_MESSAGE_TYPE_REGISTERED = 3

    ctypedef union nvtxMessageValue_t:
        const char* ascii
        const wchar_t* unicode
        nvtxStringHandle_t registered

    ctypedef enum nvtxPayloadType_t:
        NVTX_PAYLOAD_UNKNOWN = 0
        NVTX_PAYLOAD_TYPE_UNSIGNED_INT64 = 1
        NVTX_PAYLOAD_TYPE_INT64 = 2
        NVTX_PAYLOAD_TYPE_DOUBLE = 3
        NVTX_PAYLOAD_TYPE_UNSIGNED_INT32 = 4
        NVTX_PAYLOAD_TYPE_INT32 = 5
        NVTX_PAYLOAD_TYPE_FLOAT = 6

    cdef union payload_t:
        uint64_t ullValue
        int64_t llValue
        double dValue
        uint32_t uiValue
        int32_t iValue
        float fValue

    ctypedef struct nvtxEventAttributes_v2:
        uint16_t version
        uint16_t size
        uint32_t category
        int32_t colorType
        uint32_t color
        int32_t payloadType
        int32_t reserved0
        payload_t payload
        int32_t messageType
        nvtxMessageValue_t message

    ctypedef nvtxEventAttributes_v2 nvtxEventAttributes_t

    cdef nvtxDomainHandle_t nvtxDomainCreateA(const char* name)
    cdef void nvtxDomainDestroy(nvtxDomainHandle_t domain)

    cdef nvtxStringHandle_t nvtxDomainRegisterStringA(nvtxDomainHandle_t domain, const char* string)

    cdef int nvtxDomainRangePushEx(
        nvtxDomainHandle_t domain,
        const nvtxEventAttributes_t* eventAttrib
    )
    cdef int nvtxDomainRangePop(nvtxDomainHandle_t domain)

    cdef nvtxRangeId_t nvtxDomainRangeStartEx(
        nvtxDomainHandle_t domain,
        const nvtxEventAttributes_t* eventAttrib
    )
    cdef void nvtxDomainRangeEnd(
        nvtxDomainHandle_t domain,
        nvtxRangeId_t
    )

    cdef void nvtxDomainMarkEx(
        nvtxDomainHandle_t domain,
        const nvtxEventAttributes_t* eventAttrib
    )

    cdef void nvtxDomainNameCategoryA(
        nvtxDomainHandle_t domain,
        uint32_t category,
        const char* name
    )

cdef extern from "nvtx3/nvToolsExtPayload.h" nogil:
    cdef uint8_t nvtxDomainIsEnabled(nvtxDomainHandle_t domain)

    cdef int NVTX_PAYLOAD_ENTRY_FLAG_UNUSED
    cdef int NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE
    cdef int NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX

    cdef int NVTX_PAYLOAD_TYPE_EXT

    cdef int NVTX_PAYLOAD_ENTRY_TYPE_INVALID
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_INT8
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_UINT8
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_INT16
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_UINT16
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_INT32
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_UINT32
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_INT64
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_UINT64
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_FLOAT16
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_FLOAT32
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_FLOAT128
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_BYTE
    cdef int NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32

    cdef int NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE
    cdef int NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES
    cdef int NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES
    cdef int NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE

    cdef int NVTX_PAYLOAD_SCHEMA_TYPE_STATIC
    cdef int NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC

    ctypedef struct nvtxPayloadData_v1:
        uint64_t schemaId
        size_t size
        const void* payload

    ctypedef nvtxPayloadData_v1 nvtxPayloadData_t


    ctypedef struct nvtxSemanticsHeader_t:
        uint32_t structSize
        uint16_t semanticId
        uint16_t version
        const nvtxSemanticsHeader_t* next

    ctypedef struct nvtxPayloadSchemaEntry_t:
        uint64_t flags
        uint64_t type
        const char* name
        const char* description
        uint64_t arrayOrUnionDetail
        uint64_t offset
        const nvtxSemanticsHeader_t* semantics
        const void* reserved

    ctypedef struct nvtxPayloadSchemaAttr_t:
        uint64_t fieldMask
        const char* name
        uint64_t type
        uint64_t flags
        const nvtxPayloadSchemaEntry_t* entries
        size_t numEntries
        size_t payloadStaticSize
        size_t packAlign
        uint64_t schemaId
        void* extension

    cdef uint64_t nvtxPayloadSchemaRegister(
        nvtxDomainHandle_t domain,
        const nvtxPayloadSchemaAttr_t* attr
    )

cdef class EventAttributes:
    cdef object domain
    cdef object _message
    cdef object _color
    cdef uint32_t _category
    cdef object _payload
    cdef nvtxStringHandle_t string_handle
    cdef nvtxEventAttributes_t c_obj
    cdef nvtxPayloadData_t _payload_data

    # Dynamic memory allocation is required for array payloads.
    # This pointer is used to track the memory that should be freed.
    cdef void* _allocated_payload

    cdef _set_binary_payload(self, void* payload, uint64_t schema, size_t nbytes, size_t size)
    cdef _clear_payload(self)


cdef class DomainHandle:
    cdef bytes _name
    cdef nvtxDomainHandle_t c_obj

cdef class StringHandle:
    cdef bytes _string
    cdef nvtxStringHandle_t c_obj
