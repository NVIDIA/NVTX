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

#if !defined(NVTXW_COUNTER_HELPERS_API)
#define NVTXW_COUNTER_HELPERS_API

#include <nvtxw3/nvtxw3.h>
#include <string.h>

/**
 * \file nvtxw3_counter_helpers.h
 * \brief Counter registration and write helpers for the NVTX Writer API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Register a counter with a dynamic counter ID.
 *
 * schemaId identifies the counter sample layout. It may be a predefined
 * scalar entry type, such as NVTX_PAYLOAD_ENTRY_TYPE_INT64 or
 * NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE, or a full schema ID. `semantics` may be NULL.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwCounterRegister(
    const nvtxwInterface_t* iface,
    nvtxDomainHandle_t domain,
    const char* name,
    uint64_t schemaId,
    const nvtxSemanticsHeader_t* semantics,
    uint64_t* counterIdOut)
{
    nvtxCounterAttr_t counterAttr;

    if (!iface || !iface->CounterRegister || !domain ||
        !schemaId || !counterIdOut)
        return NVTXW_RESULT_INVALID_ARGUMENT;

    *counterIdOut = NVTX_COUNTER_ID_NONE;

    memset(&counterAttr, 0, sizeof(counterAttr));
    counterAttr.structSize = sizeof(counterAttr);
    counterAttr.schemaId = schemaId;
    counterAttr.name = name;
    counterAttr.semantics = semantics;

    return iface->CounterRegister(domain, &counterAttr, counterIdOut);
}

/**
 * \brief Register a 64-bit integer counter with a dynamic counter ID.
 *
 * `semantics` may be NULL.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwCounterInt64Register(
    const nvtxwInterface_t* iface,
    nvtxDomainHandle_t domain,
    const char* name,
    const nvtxSemanticsHeader_t* semantics,
    uint64_t* counterIdOut)
{
    return nvtxwCounterRegister(
        iface,
        domain,
        name,
        NVTX_PAYLOAD_ENTRY_TYPE_INT64,
        semantics,
        counterIdOut);
}

/**
 * \brief Register a 64-bit floating point counter with a dynamic counter ID.
 *
 * `semantics` may be NULL.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwCounterFloat64Register(
    const nvtxwInterface_t* iface,
    nvtxDomainHandle_t domain,
    const char* name,
    const nvtxSemanticsHeader_t* semantics,
    uint64_t* counterIdOut)
{
    return nvtxwCounterRegister(
        iface,
        domain,
        name,
        NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE,
        semantics,
        counterIdOut);
}

/**
 * \brief Write a 64-bit integer counter sample.
 *
 * The counter should have been registered with an INT64 scalar schema.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwCounterInt64Write(
    const nvtxwInterface_t* iface,
    nvtxwStreamHandle_t stream,
    int64_t timestamp,
    uint64_t counterId,
    int64_t value)
{
    if (!iface || !iface->CounterWrite || !stream)
        return NVTXW_RESULT_INVALID_ARGUMENT;
    return iface->CounterWrite(stream, timestamp, counterId, &value, sizeof(value));
}

/**
 * \brief Write a 64-bit floating point counter sample.
 *
 * The counter should have been registered with a DOUBLE/FLOAT64 scalar schema.
 */
NVTX_INLINE_STATIC
nvtxwResultCode_t nvtxwCounterFloat64Write(
    const nvtxwInterface_t* iface,
    nvtxwStreamHandle_t stream,
    int64_t timestamp,
    uint64_t counterId,
    double value)
{
    if (!iface || !iface->CounterWrite || !stream)
        return NVTXW_RESULT_INVALID_ARGUMENT;
    return iface->CounterWrite(stream, timestamp, counterId, &value, sizeof(value));
}

#ifdef __cplusplus
}
#endif

#endif
