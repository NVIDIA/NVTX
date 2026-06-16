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
from libc.stdint cimport int64_t, uint8_t, uint64_t
from nvtx._lib.lib cimport nvtxDomainHandle_t, nvtxSemanticsHeader_t

cdef extern from "nvtx3/nvToolsExtCounters.h" nogil:
    cdef int NVTX_COUNTER_SAMPLE_ZERO
    cdef int NVTX_COUNTER_SAMPLE_UNCHANGED
    cdef int NVTX_COUNTER_SAMPLE_UNAVAILABLE
    cdef uint64_t NVTX_COUNTER_ID_NONE

    ctypedef struct nvtxCounterAttr_t:
        size_t structSize
        uint64_t schemaId
        const char* name
        const char* description
        uint64_t scopeId
        const nvtxSemanticsHeader_t* semantics
        uint64_t counterId

    ctypedef struct nvtxCounterBatch_t:
        uint64_t counterId
        const void* counters
        size_t countersSize
        uint64_t flags
        const int64_t* timestamps
        size_t timestampsSize

    cdef uint64_t nvtxCounterRegister(
        nvtxDomainHandle_t domain,
        const nvtxCounterAttr_t* attr
    )
    cdef void nvtxCounterSampleInt64(
        nvtxDomainHandle_t domain,
        uint64_t counterId,
        int64_t value
    )
    cdef void nvtxCounterSampleFloat64(
        nvtxDomainHandle_t domain,
        uint64_t counterId,
        double value
    )
    cdef void nvtxCounterSample(
        nvtxDomainHandle_t domain,
        uint64_t counterId,
        const void* value,
        size_t size
    )
    cdef void nvtxCounterSampleNoValue(
        nvtxDomainHandle_t domain,
        uint64_t counterId,
        uint8_t reason
    )
    cdef void nvtxCounterBatchSubmit(
        nvtxDomainHandle_t domain,
        const nvtxCounterBatch_t* counterData
    )

cdef extern from "nvtx3/nvToolsExtSemanticsCounters.h" nogil:
    cdef int NVTX_SEMANTIC_ID_COUNTERS_V1
    cdef int NVTX_COUNTER_SEMANTIC_VERSION

    cdef uint64_t NVTX_COUNTER_FLAG_LIMIT_MIN
    cdef uint64_t NVTX_COUNTER_FLAG_LIMIT_MAX

    cdef uint64_t NVTX_COUNTER_FLAG_VALUETYPE_ABSOLUTE
    cdef uint64_t NVTX_COUNTER_FLAG_VALUETYPE_DELTA
    cdef uint64_t NVTX_COUNTER_FLAG_VALUETYPE_DELTA_SINCE_START

    cdef uint64_t NVTX_COUNTER_FLAG_INTERPOLATION_POINT
    cdef uint64_t NVTX_COUNTER_FLAG_INTERPOLATION_SINCE_LAST
    cdef uint64_t NVTX_COUNTER_FLAG_INTERPOLATION_UNTIL_NEXT
    cdef uint64_t NVTX_COUNTER_FLAG_INTERPOLATION_LINEAR

    cdef int64_t NVTX_COUNTER_LIMIT_UNDEFINED
    cdef int64_t NVTX_COUNTER_LIMIT_I64
    cdef int64_t NVTX_COUNTER_LIMIT_U64
    cdef int64_t NVTX_COUNTER_LIMIT_F64

    ctypedef union nvtxCounterLimit_t:
        int64_t i64
        uint64_t u64
        double f64

    ctypedef struct nvtxSemanticsCounter_t:
        nvtxSemanticsHeader_t header
        uint64_t flags
        const char* unit
        uint64_t unitScaleNumerator
        uint64_t unitScaleDenominator
        int64_t limitType
        nvtxCounterLimit_t min
        nvtxCounterLimit_t max

cdef class Counter:
    cdef readonly object domain
    cdef readonly object name
    cdef readonly object dtype
    cdef readonly object description
    cdef readonly object scope
    cdef readonly object semantics
    cdef readonly uint64_t time_domain
    cdef uint64_t _counter_id
    cdef uint64_t _schema_id

cdef void _fill_counter_semantics(
    nvtxSemanticsCounter_t* dst,
    object semantics,
)
