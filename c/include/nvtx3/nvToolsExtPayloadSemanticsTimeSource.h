/*
* Copyright 2023  NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License v2.0 with LLVM Exceptions.
* See https://llvm.org/LICENSE.txt for license information.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#ifndef NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_TIME_SOURCE_V1
#define NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_TIME_SOURCE_V1 2

/*
 * Timestamp source is not known, e.g. NIC or switch. The NVTX handler can
 * assume that at least two synchronization points are triggered by the
 * NVTX instrumentation.
 */
#define NVTX_TIMESTAMP_SOURCE_UNKNOWN                       0

/**
 * \brief CPU timestamp sources.
 */
#define NVTX_TIMESTAMP_CPU_TSC                              1
#define NVTX_TIMESTAMP_CPU_TSC_NONVIRTUALIZED               2
#define NVTX_TIMESTAMP_CPU_CLOCK_GETTIME_REALTIME           3
#define NVTX_TIMESTAMP_CPU_CLOCK_GETTIME_REALTIME_COARSE    4
#define NVTX_TIMESTAMP_CPU_CLOCK_GETTIME_MONOTONIC          5
#define NVTX_TIMESTAMP_CPU_CLOCK_GETTIME_MONOTONIC_RAW      6
#define NVTX_TIMESTAMP_CPU_CLOCK_GETTIME_MONOTONIC_COARSE   7
#define NVTX_TIMESTAMP_CPU_CLOCK_GETTIME_BOOTTIME           8
#define NVTX_TIMESTAMP_CPU_CLOCK_GETTIME_PROCESS_CPUTIME_ID 9
#define NVTX_TIMESTAMP_CPU_CLOCK_GETTIME_THREAD_CPUTIME_ID  10

#define NVTX_TIMESTAMP_WIN_QPC     16
#define NVTX_TIMESTAMP_WIN_GSTAFT  17
#define NVTX_TIMESTAMP_WIN_GSTAFTP 18

#define NVTX_TIMESTAMP_C_TIME         32
#define NVTX_TIMESTAMP_C_CLOCK        33
#define NVTX_TIMESTAMP_C_TIMESPEC_GET 34

#define NVTX_TIMESTAMP_CPP_STEADY_CLOCK          64
#define NVTX_TIMESTAMP_CPP_HIGH_RESOLUTION_CLOCK 65
#define NVTX_TIMESTAMP_CPP_SYSTEM_CLOCK          66
#define NVTX_TIMESTAMP_CPP_UTC_CLOCK             67
#define NVTX_TIMESTAMP_CPP_TAI_CLOCK             68
#define NVTX_TIMESTAMP_CPP_GPS_CLOCK             69
#define NVTX_TIMESTAMP_CPP_FILE_CLOCK            70

/**
 * \brief GPU timestamp sources.
 */
#define NVTX_TIMESTAMP_GPU_GLOBALTIMER 128
#define NVTX_TIMESTAMP_GPU_SM_CLOCK    129
#define NVTX_TIMESTAMP_GPU_SM_CLOCK64  130
#define NVTX_TIMESTAMP_GPU_CUPTI       131

/**
 * The timestamp was provided by the NVTX handler’s timestamp routine.
 */
#define NVTX_TIMESTAMP_TOOL_PROVIDED 200

/* These flags specify the NVTX event type to which an entry refers. */
#define NVTX_TIMESTAMP_EVENT_UNKNOWN       0
#define NVTX_TIMESTAMP_EVENT_RANGE_BEGIN   1
#define NVTX_TIMESTAMP_EVENT_RANGE_END     2
#define NVTX_TIMESTAMP_EVENT_MARK          3
#define NVTX_TIMESTAMP_EVENT_COUNTER       4
#define NVTX_TIMESTAMP_EVENT_BATCH_BEGIN   5
#define NVTX_TIMESTAMP_EVENT_BATCH_DELTA   6

/**
 * \brief Semantic extension to specify timer source and event type of a
 * timestamp value.
 */
typedef struct nvtxPayloadEntryTimestamp_v1
{
    struct nvtxPayloadEntrySemantic_v1 header;

    /* Timer source (see `NVTX_TIMESTAMP_*`) */
    uint64_t timeSource;

    /* See `NVTX_SEMANTIC_TIMESTAMP_EVENT_*` */
    uint64_t eventType;
} nvtxPayloadEntryTimestamp_t;

#endif /* NVTX_PAYLOAD_ENTRY_SEMANTIC_ID_TIME_SOURCE_V1 */