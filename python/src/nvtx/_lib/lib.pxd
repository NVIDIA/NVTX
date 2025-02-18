# SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


cdef class EventAttributes:
    cdef object _message
    cdef object _color
    cdef uint32_t _category
    cdef object _payload
    cdef nvtxStringHandle_t string_handle
    cdef nvtxEventAttributes_t c_obj


cdef class DomainHandle:
    cdef bytes _name
    cdef nvtxDomainHandle_t c_obj

cdef class StringHandle:
    cdef bytes _string
    cdef nvtxStringHandle_t c_obj
