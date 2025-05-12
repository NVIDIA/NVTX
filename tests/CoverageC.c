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

#include <nvtx3/nvToolsExt.h>

static void TestCore(void)
{
    nvtxEventAttributes_t attributes;
    nvtxRangeId_t rangeId;

    attributes.version = NVTX_VERSION;
    attributes.size = sizeof(attributes);
    attributes.category = 0;
    attributes.colorType = NVTX_COLOR_ARGB;
    attributes.color = 0xFF1133FF;
    attributes.payloadType = NVTX_PAYLOAD_UNKNOWN;
    attributes.payload.llValue = 0;
    attributes.messageType = NVTX_MESSAGE_TYPE_ASCII;
    attributes.message.ascii = "Test message";

    nvtxMarkEx(&attributes);
    nvtxMarkA("MarkA");
    nvtxMarkW(L"MarkW");
    rangeId = nvtxRangeStartEx(&attributes);
    nvtxRangeEnd(rangeId);
    rangeId = nvtxRangeStartA("RangeStartA");
    nvtxRangeEnd(rangeId);
    rangeId = nvtxRangeStartW(L"RangeStartW");
    nvtxRangeEnd(rangeId);
    nvtxRangePushEx(&attributes);
    nvtxRangePop();
    nvtxRangePushA("RangePushA");
    nvtxRangePop();
    nvtxRangePushW(L"RangePushW");
    nvtxRangePop();
}

static void TestCore2(void)
{
    nvtxEventAttributes_t attributes;
    nvtxRangeId_t rangeId;
    nvtxDomainHandle_t domain, domainW;

    attributes.version = NVTX_VERSION;
    attributes.size = sizeof(attributes);
    attributes.category = 0;
    attributes.colorType = NVTX_COLOR_ARGB;
    attributes.color = 0xFF1133FF;
    attributes.payloadType = NVTX_PAYLOAD_UNKNOWN;
    attributes.payload.llValue = 0;
    attributes.messageType = NVTX_MESSAGE_TYPE_ASCII;
    attributes.message.ascii = "Test message";

    domain = nvtxDomainCreateA("DomainA");
    domainW = nvtxDomainCreateW(L"DomainW");

    nvtxDomainMarkEx(domain, &attributes);
    rangeId = nvtxDomainRangeStartEx(domain, &attributes);
    nvtxDomainRangeEnd(domain, rangeId);
    nvtxDomainRangePushEx(domain, &attributes);
    nvtxDomainRangePop(domain);

    nvtxDomainMarkEx(domainW, &attributes);
    rangeId = nvtxDomainRangeStartEx(domainW, &attributes);
    nvtxDomainRangeEnd(domainW, rangeId);
    nvtxDomainRangePushEx(domainW, &attributes);
    nvtxDomainRangePop(domainW);
}

NVTX_DYNAMIC_EXPORT
extern int RunTest(int argc, const char** argv);
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    (void)argc;
    (void)argv;

    TestCore();
    TestCore2();

    return 0;
}
