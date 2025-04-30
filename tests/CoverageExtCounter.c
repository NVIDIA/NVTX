/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <nvtx3/nvToolsExtCounters.h>

static void TestCounter(void)
{
    nvtxDomainHandle_t domain;
    uint64_t counter;
    nvtxCounterAttr_t attr;
    int64_t i64 = 0;
    double f64 = 0.0;

    domain = nvtxDomainCreateA("Domain");

    counter = nvtxCounterRegister(domain, &attr);
    nvtxCounterSampleInt64(domain, counter, i64);
    nvtxCounterSampleFloat64(domain, counter, f64);
    nvtxCounterSampleNoValue(domain, counter, NVTX_COUNTER_SAMPLE_UNCHANGED);
}

NVTX_DYNAMIC_EXPORT
extern int RunTest(int argc, const char** argv);
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    (void)argc;
    (void)argv;

    TestCounter();

    return 0;
}
