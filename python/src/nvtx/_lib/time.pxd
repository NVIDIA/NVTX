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

from libc.stdint cimport int64_t, uint64_t
from nvtx._lib.lib cimport nvtxSemanticsHeader_t

cdef extern from "nvtx3/nvToolsExtPayload.h" nogil:
    cdef int NVTX_TIMESTAMP_TYPE_NONE
    cdef int NVTX_TIMESTAMP_TYPE_TOOL_PROVIDED
    cdef int NVTX_TIMESTAMP_TYPE_CPU_TSC
    cdef int NVTX_TIMESTAMP_TYPE_CPU_TSC_NONVIRTUALIZED
    cdef int NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_REALTIME
    cdef int NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_REALTIME_COARSE
    cdef int NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC
    cdef int NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC_RAW
    cdef int NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC_COARSE
    cdef int NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_BOOTTIME
    cdef int NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_PROCESS_CPUTIME_ID
    cdef int NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_THREAD_CPUTIME_ID
    cdef int NVTX_TIMESTAMP_TYPE_WIN_QPC
    cdef int NVTX_TIMESTAMP_TYPE_WIN_GSTAFT
    cdef int NVTX_TIMESTAMP_TYPE_WIN_GSTAFTP
    cdef int NVTX_TIMESTAMP_TYPE_C_TIME
    cdef int NVTX_TIMESTAMP_TYPE_C_CLOCK
    cdef int NVTX_TIMESTAMP_TYPE_C_TIMESPEC_GET
    cdef int NVTX_TIMESTAMP_TYPE_CPP_STEADY_CLOCK
    cdef int NVTX_TIMESTAMP_TYPE_CPP_HIGH_RESOLUTION_CLOCK
    cdef int NVTX_TIMESTAMP_TYPE_CPP_SYSTEM_CLOCK
    cdef int NVTX_TIMESTAMP_TYPE_CPP_UTC_CLOCK
    cdef int NVTX_TIMESTAMP_TYPE_CPP_TAI_CLOCK
    cdef int NVTX_TIMESTAMP_TYPE_CPP_GPS_CLOCK
    cdef int NVTX_TIMESTAMP_TYPE_CPP_FILE_CLOCK
    cdef int NVTX_TIMESTAMP_TYPE_GPU_GLOBALTIMER

    cdef int64_t nvtxTimestampGet()

cdef extern from "nvtx3/nvToolsExtSemanticsTime.h" nogil:
    cdef int NVTX_SEMANTIC_ID_TIME_V1
    cdef int NVTX_TIME_SEMANTIC_VERSION

    ctypedef struct nvtxSemanticsTime_t:
        nvtxSemanticsHeader_t header
        uint64_t timeDomainId

cdef void _fill_time_semantics(nvtxSemanticsTime_t* dst, uint64_t time_domain)
