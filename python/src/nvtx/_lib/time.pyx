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

from enum import Enum


class TimestampType(Enum):
    """
    Timestamp domains that can be associated with batched counter samples.

    This enum represents the ``NVTX_TIMESTAMP_TYPE_*`` macros from the
    NVTX C payload header.

    Use these values as the ``time_domain`` argument to
    :meth:`nvtx.Domain.get_counter`. For batched counters, timestamps
    passed to :meth:`Counter.batch_submit <nvtx.Counter.batch_submit>`
    should come from the same timestamp domain.

    Attributes
    ----------
    NONE
        No timestamp domain is specified.
    TOOL_PROVIDED
        The timestamp is provided by the NVTX handler (tool).
    CPU_TSC
        CPU timestamp counter (RDTSC on x86, CNTVCT on ARM).
    CPU_TSC_NONVIRTUALIZED
        Non-virtualized CPU timestamp counter (CNTPCT on ARM).
    CPU_CLOCK_GETTIME_REALTIME
        POSIX ``clock_gettime`` with ``CLOCK_REALTIME``.
    CPU_CLOCK_GETTIME_REALTIME_COARSE
        POSIX ``clock_gettime`` with ``CLOCK_REALTIME_COARSE``.
    CPU_CLOCK_GETTIME_MONOTONIC
        POSIX ``clock_gettime`` with ``CLOCK_MONOTONIC``.
    CPU_CLOCK_GETTIME_MONOTONIC_RAW
        POSIX ``clock_gettime`` with ``CLOCK_MONOTONIC_RAW``.
    CPU_CLOCK_GETTIME_MONOTONIC_COARSE
        POSIX ``clock_gettime`` with ``CLOCK_MONOTONIC_COARSE``.
    CPU_CLOCK_GETTIME_BOOTTIME
        POSIX ``clock_gettime`` with ``CLOCK_BOOTTIME``.
    CPU_CLOCK_GETTIME_PROCESS_CPUTIME_ID
        POSIX ``clock_gettime`` with ``CLOCK_PROCESS_CPUTIME_ID``.
    CPU_CLOCK_GETTIME_THREAD_CPUTIME_ID
        POSIX ``clock_gettime`` with ``CLOCK_THREAD_CPUTIME_ID``.
    WIN_QPC
        Windows ``QueryPerformanceCounter``.
    WIN_GSTAFT
        Windows ``GetSystemTimeAsFileTime``.
    WIN_GSTAFTP
        Windows ``GetSystemTimePreciseAsFileTime``.
    C_TIME
        C ``time()``.
    C_CLOCK
        C ``clock()``.
    C_TIMESPEC_GET
        C ``timespec_get()``.
    CPP_STEADY_CLOCK
        C++ ``std::chrono::steady_clock``.
    CPP_HIGH_RESOLUTION_CLOCK
        C++ ``std::chrono::high_resolution_clock``.
    CPP_SYSTEM_CLOCK
        C++ ``std::chrono::system_clock``.
    CPP_UTC_CLOCK
        C++ ``std::chrono::utc_clock``.
    CPP_TAI_CLOCK
        C++ ``std::chrono::tai_clock``.
    CPP_GPS_CLOCK
        C++ ``std::chrono::gps_clock``.
    CPP_FILE_CLOCK
        C++ ``std::chrono::file_clock``.
    GPU_GLOBALTIMER
        GPU global timer (e.g. PTIMER).
    """

    NONE = NVTX_TIMESTAMP_TYPE_NONE
    TOOL_PROVIDED = NVTX_TIMESTAMP_TYPE_TOOL_PROVIDED
    CPU_TSC = NVTX_TIMESTAMP_TYPE_CPU_TSC
    CPU_TSC_NONVIRTUALIZED = NVTX_TIMESTAMP_TYPE_CPU_TSC_NONVIRTUALIZED
    CPU_CLOCK_GETTIME_REALTIME = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_REALTIME
    CPU_CLOCK_GETTIME_REALTIME_COARSE = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_REALTIME_COARSE
    CPU_CLOCK_GETTIME_MONOTONIC = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC
    CPU_CLOCK_GETTIME_MONOTONIC_RAW = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC_RAW
    CPU_CLOCK_GETTIME_MONOTONIC_COARSE = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC_COARSE
    CPU_CLOCK_GETTIME_BOOTTIME = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_BOOTTIME
    CPU_CLOCK_GETTIME_PROCESS_CPUTIME_ID = (
        NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_PROCESS_CPUTIME_ID
    )
    CPU_CLOCK_GETTIME_THREAD_CPUTIME_ID = (
        NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_THREAD_CPUTIME_ID
    )
    WIN_QPC = NVTX_TIMESTAMP_TYPE_WIN_QPC
    WIN_GSTAFT = NVTX_TIMESTAMP_TYPE_WIN_GSTAFT
    WIN_GSTAFTP = NVTX_TIMESTAMP_TYPE_WIN_GSTAFTP
    C_TIME = NVTX_TIMESTAMP_TYPE_C_TIME
    C_CLOCK = NVTX_TIMESTAMP_TYPE_C_CLOCK
    C_TIMESPEC_GET = NVTX_TIMESTAMP_TYPE_C_TIMESPEC_GET
    CPP_STEADY_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_STEADY_CLOCK
    CPP_HIGH_RESOLUTION_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_HIGH_RESOLUTION_CLOCK
    CPP_SYSTEM_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_SYSTEM_CLOCK
    CPP_UTC_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_UTC_CLOCK
    CPP_TAI_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_TAI_CLOCK
    CPP_GPS_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_GPS_CLOCK
    CPP_FILE_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_FILE_CLOCK
    GPU_GLOBALTIMER = NVTX_TIMESTAMP_TYPE_GPU_GLOBALTIMER


cdef void _fill_time_semantics(nvtxSemanticsTime_t* dst, uint64_t time_domain):
    dst.header.structSize = sizeof(nvtxSemanticsTime_t)
    dst.header.semanticId = NVTX_SEMANTIC_ID_TIME_V1
    dst.header.version = NVTX_TIME_SEMANTIC_VERSION
    dst.header.next = NULL
    dst.timeDomainId = time_domain
