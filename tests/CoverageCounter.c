/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 * See LICENSE.txt for license information.
 */

#include <nvtx3/nvToolsExtCounters.h>

void TestMem()
{
    nvtxDomainHandle_t domain;
    uint64_t counter;
    nvtxCountersAttr_t attr;
    int64_t i64 = 0;
    double f64 = 0.0;

    domain = nvtxDomainCreateA("Domain");

    counter = nvtxCountersRegister(domain, &attr);
    nvtxCountersSampleInt64(domain, counter, i64);
    nvtxCountersSampleFloat64(domain, counter, f64);
    nvtxCountersSampleNoValue(domain, counter, NVTX_COUNTERS_SAMPLE_UNCHANGED);
}

#include "DllHelper.h"

DLL_EXPORT
int RunTest(int argc, const char** argv)
{
    (void)argc;
    (void)argv;

    TestMem();

    return 0;
}
