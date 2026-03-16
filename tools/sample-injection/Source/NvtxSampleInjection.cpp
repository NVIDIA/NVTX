/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <stdio.h>
#include <string>
#include <vector>

#ifndef NVTX_NO_IMPL
#define NVTX_NO_IMPL
#endif
#include <nvtx3/nvToolsExtPayload.h>

#include "NvtxPayloadInjectionAdapter.h"

#ifdef _WIN32
#include <process.h>
#include <processthreadsapi.h>

#define EXPORT_SYMBOL __declspec(dllexport)
#define getpid _getpid
#define gettid GetCurrentThreadId
#else
#include <unistd.h>

#define EXPORT_SYMBOL __attribute__((visibility("default")))
#ifdef __APPLE__
static inline int gettid(void)
{
    uint64_t tid;
    pthread_threadid_np(nullptr, &tid);
    return static_cast<int>(tid);
}
#endif
#endif

// The `nvtxDomainRegistration_st`s content is implementation-defined. For NVTX, it is just a
// forward declaration [1], likewise for `nvtxStringRegistration_st` [2]. See the
// `DomainCreateA()/DomainDestroy()` callbacks below for how its lifetime is managed by NVTX.
//
// [1] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvToolsExt.h#L377
// [2] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvToolsExt.h#L391
struct nvtxDomainRegistration_st
{
    std::string name;
    std::vector<nvtxStringHandle_t> registeredStrings;
};

struct nvtxStringRegistration_st
{
    std::string value;
};

namespace {

std::mutex g_mutex;
std::atomic<bool> g_isTornDown{false};

std::unordered_map<nvtxStringHandle_t, std::string> g_registeredStrings;

// A single global registry is used here because our example codes do not have colliding static
// schema/enum IDs across domains. In NVTX, static IDs are only required to be unique per domain,
// so a multi-domain application with overlapping static IDs would need per-domain registries.
// Constructed in InitializePayloadExtension once typeInfo is available.
std::optional<NvtxPayloadRegistry> g_registry;
PayloadFormat g_payloadFormat = PayloadFormat::Text;

struct TearDownDetector
{
    ~TearDownDetector()
    {
        g_isTornDown = true;
    }
} g_tearDownDetector;

// Returns a callback table ([1]) specified by the `module` argument or `nullptr` in case of error.
// See `module`'s  valid values in the `NvtxCallbackModule` enum [2].
//
// [1] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvtxDetail/nvtxTypes.h#L277
// [2] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvtxDetail/nvtxTypes.h#L138
NvtxFunctionTable
GetFunctionTable(NvtxGetExportTableFunc_t getExportTable, NvtxCallbackModule callbackModule)
{
    auto callbacks =
        reinterpret_cast<const NvtxExportTableCallbacks*>(getExportTable(NVTX_ETID_CALLBACKS));
    if (!callbacks)
    {
        fprintf(stderr, "[NVTX] Could not get NVTX_ETID_CALLBACKS.\n");
        return nullptr;
    }

    NvtxFunctionTable table = nullptr;
    unsigned int tableSize = 0;
    if (!callbacks->GetModuleFunctionTable(callbackModule, &table, &tableSize))
    {
        fprintf(stderr, "[NVTX] Could not get function table of module %d.\n", callbackModule);
        return nullptr;
    }

    return table;
}

long long GetCurrentTimeMs()
{
    auto nowSinceEpoch = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(nowSinceEpoch).count();
}

void PrintPayloadEvent(
    const char* eventKind,
    nvtxDomainHandle_t domain,
    const nvtxPayloadData_t* payloadData,
    size_t count)
{
    const char* domainName = domain ? domain->name.c_str() : "<default domain>";
    std::string desc =
        NvtxPayloadInjection::DescribePayloads(*g_registry, payloadData, count, g_payloadFormat);
    printf(
        "[NVTX][%d][%lld] PAYLOAD %s @%s (%s)\n",
        gettid(),
        GetCurrentTimeMs(),
        eventKind,
        domainName,
        desc.c_str());
}

void PrintDomainEvent(
    const char* eventKind, nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib)
{
    const char* name = "No name";
    if (eventAttrib)
    {
        if (eventAttrib->messageType == NVTX_MESSAGE_TYPE_REGISTERED)
        {
            auto it = g_registeredStrings.find(eventAttrib->message.registered);
            name = (it != g_registeredStrings.end()) ? it->second.c_str() : "<unregistered>";
        }
        else if (eventAttrib->messageType == NVTX_MESSAGE_TYPE_ASCII && eventAttrib->message.ascii)
        {
            name = eventAttrib->message.ascii;
        }
    }

    const char* domainName = domain ? domain->name.c_str() : "<default domain>";

    // Core callbacks can fire before the payload extension is initialized.
    // Payload-specific callbacks are only reachable after g_registry is constructed.
    std::string payloadDescription;
    if (g_registry)
    {
        payloadDescription = NvtxPayloadInjection::DescribeEmbeddedPayload(
            *g_registry, eventAttrib, g_payloadFormat);
    }

    if (payloadDescription.empty())
    {
        printf(
            "[NVTX][%d][%lld] %s %s@%s\n",
            gettid(),
            GetCurrentTimeMs(),
            eventKind,
            name,
            domainName);
    }
    else
    {
        printf(
            "[NVTX][%d][%lld] PAYLOAD %s %s@%s (%s)\n",
            gettid(),
            GetCurrentTimeMs(),
            eventKind,
            name,
            domainName,
            payloadDescription.c_str());
    }
}
namespace impl {

int RangePushA(const char* message)
{
    // The injection may need to perform some finalizing steps (like flushing collected
    // data to the disk), after which it stops accepting calls (that may come from other
    // threads) to guarantee consistency. Checking an atomic flag (`g_isTornDown`) is one
    // of the possible ways to do it.
    if (g_isTornDown)
    {
        return NVTX_FAIL;
    }

    // Collecting data from multiple threads often requires shared memory, which needs protection.
    // Here, the output of `printf` calls is being protected from interleaving. While mutexes are
    // a simple solution, lock-free data structures should be considered if performance is a
    // concern.
    std::lock_guard<std::mutex> guard(g_mutex);

    printf("[NVTX][%d][%lld] PUSH %s\n", gettid(), GetCurrentTimeMs(), message);
    return NVTX_NO_PUSH_POP_TRACKING;
}

int RangePop()
{
    if (g_isTornDown)
    {
        return NVTX_FAIL;
    }

    std::lock_guard<std::mutex> guard(g_mutex);

    printf("[NVTX][%d][%lld] POP\n", gettid(), GetCurrentTimeMs());
    return NVTX_NO_PUSH_POP_TRACKING;
}

nvtxDomainHandle_t DomainCreateA(const char* name)
{
    if (g_isTornDown)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> guard(g_mutex);

    printf("[NVTX][%d][%lld] DOMAIN CREATE %s \n", gettid(), GetCurrentTimeMs(), name);
    return new nvtxDomainRegistration_st({name, {}});
}

void DomainDestroy(nvtxDomainHandle_t domain)
{
    if (g_isTornDown)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(g_mutex);

    // TODO: Remove calling `nvtxDomainDestroy()` with NULL in python's
    // `DomainHandle::__dealloc__()` (NVTX/python/nvtx/_lib/lib.pyx) and
    // remove this check.
    if (!domain)
    {
        return;
    }

    for (auto* stringHandle : domain->registeredStrings)
    {
        delete stringHandle;
    }

    printf(
        "[NVTX][%d][%lld] DOMAIN DESTROY %s\n", gettid(), GetCurrentTimeMs(), domain->name.c_str());
    delete domain;
}

nvtxStringHandle_t DomainRegisterStringA(nvtxDomainHandle_t domain, const char* string)
{
    if (g_isTornDown)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> guard(g_mutex);

    auto* registration = new nvtxStringRegistration_st{string ? string : ""};
    g_registeredStrings[registration] = registration->value;
    if (domain)
    {
        domain->registeredStrings.push_back(registration);
    }

    const char* domainName = domain ? domain->name.c_str() : "<default domain>";
    printf(
        "[NVTX][%d][%lld] DOMAIN REGISTER STRING %s@%s\n",
        gettid(),
        GetCurrentTimeMs(),
        registration->value.c_str(),
        domainName);
    return registration;
}

void MarkA(const char* message)
{
    if (g_isTornDown)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(g_mutex);

    printf("[NVTX][%d][%lld] MARK %s\n", gettid(), GetCurrentTimeMs(), message);
}

void DomainMarkEx(nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib)
{
    if (g_isTornDown)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(g_mutex);
    PrintDomainEvent("MARK", domain, eventAttrib);
}

void DomainRangePushEx(nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib)
{
    if (g_isTornDown)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(g_mutex);
    PrintDomainEvent("PUSH", domain, eventAttrib);
}

void DomainRangePop(nvtxDomainHandle_t domain)
{
    if (g_isTornDown)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(g_mutex);

    const char* domainName = domain ? domain->name.c_str() : "<default domain>";
    printf("[NVTX][%d][%lld] POP @%s\n", gettid(), GetCurrentTimeMs(), domainName);
}

uint64_t PayloadSchemaRegister(nvtxDomainHandle_t domain, const nvtxPayloadSchemaAttr_t* attr)
{
    if (g_isTornDown)
    {
        return 0;
    }

    std::lock_guard<std::mutex> guard(g_mutex);

    const char* domainName = domain ? domain->name.c_str() : "<default domain>";
    const bool hasName =
        attr && (attr->fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME) && attr->name;
    const char* schemaName = hasName ? attr->name : "<unnamed schema>";

    uint64_t schemaId = NvtxPayloadInjection::RegisterSchema(*g_registry, attr);
    if (schemaId == 0)
    {
        fprintf(
            stderr,
            "[NVTX] PAYLOAD SCHEMA REGISTER failed for %s@%s: invalid schema attributes.\n",
            schemaName,
            domainName);
        return 0;
    }

    printf(
        "[NVTX][%d][%lld] PAYLOAD SCHEMA REGISTER %s@%s (id=%" PRIu64 ")\n",
        gettid(),
        GetCurrentTimeMs(),
        schemaName,
        domainName,
        schemaId);

    return schemaId;
}

uint64_t PayloadEnumRegister(nvtxDomainHandle_t domain, const nvtxPayloadEnumAttr_t* attr)
{
    if (g_isTornDown)
    {
        return 0;
    }

    std::lock_guard<std::mutex> guard(g_mutex);

    const char* domainName = domain ? domain->name.c_str() : "<default domain>";
    const bool hasName =
        attr && (attr->fieldMask & NVTX_PAYLOAD_ENUM_ATTR_FIELD_NAME) && attr->name;
    const char* enumName = hasName ? attr->name : "<unnamed enum>";

    const uint64_t enumId = NvtxPayloadInjection::RegisterEnum(*g_registry, attr);
    if (enumId == 0)
    {
        fprintf(
            stderr,
            "[NVTX] PAYLOAD ENUM REGISTER failed for %s@%s: invalid enum attributes.\n",
            enumName,
            domainName);
        return 0;
    }

    printf(
        "[NVTX][%d][%lld] PAYLOAD ENUM REGISTER %s@%s (id=%" PRIu64 ")\n",
        gettid(),
        GetCurrentTimeMs(),
        enumName,
        domainName,
        enumId);

    return enumId;
}

void MarkPayload(nvtxDomainHandle_t domain, const nvtxPayloadData_t* payloadData, size_t count)
{
    if (g_isTornDown)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(g_mutex);
    PrintPayloadEvent("MARK", domain, payloadData, count);
}

int RangePushPayload(nvtxDomainHandle_t domain, const nvtxPayloadData_t* payloadData, size_t count)
{
    if (g_isTornDown)
    {
        return NVTX_FAIL;
    }

    std::lock_guard<std::mutex> guard(g_mutex);
    PrintPayloadEvent("PUSH", domain, payloadData, count);
    return NVTX_NO_PUSH_POP_TRACKING;
}

int RangePopPayload(nvtxDomainHandle_t domain, const nvtxPayloadData_t* payloadData, size_t count)
{
    if (g_isTornDown)
    {
        return NVTX_FAIL;
    }

    std::lock_guard<std::mutex> guard(g_mutex);
    PrintPayloadEvent("POP", domain, payloadData, count);
    return NVTX_NO_PUSH_POP_TRACKING;
}

} // namespace impl

int InitializePayloadExtension(nvtxExtModuleInfo_t* moduleInfo)
{
    if (moduleInfo->compatId != NVTX_EXT_PAYLOAD_COMPATID)
    {
        fprintf(
            stderr,
            "[NVTX] Payload extension compat ID %u is not supported (%u).\n",
            moduleInfo->compatId,
            NVTX_EXT_PAYLOAD_COMPATID);
        return 0;
    }

    if (!moduleInfo->extInfo)
    {
        fprintf(stderr, "[NVTX] Payload extension data type info is missing.\n");
        return 0;
    }

    if (!moduleInfo->segments || moduleInfo->segmentsCount == 0)
    {
        fprintf(stderr, "[NVTX] Payload extension did not provide module segments.\n");
        return 0;
    }

    nvtxExtModuleSegment_t* segment = &moduleInfo->segments[0];
    const size_t slotCount = segment->slotCount;
    intptr_t* functionSlots = segment->functionSlots;
    if (slotCount <= NVTX3EXT_CBID_nvtxRangePopPayload || !functionSlots)
    {
        fprintf(stderr, "[NVTX] Payload extension has no function slots.\n");
        return 0;
    }

    printf(
        "[NVTX][%d][%lld] InitializeInjectionNvtxExtension(moduleId=%u compatId=%u "
        "segmentId=%" PRIu64 " slots=%" PRIu64 ")\n",
        getpid(),
        GetCurrentTimeMs(),
        moduleInfo->moduleId,
        moduleInfo->compatId,
        static_cast<uint64_t>(segment->segmentId),
        static_cast<uint64_t>(segment->slotCount));

    g_registry.emplace(
        g_registeredStrings, (const nvtxPayloadEntryTypeInfo_t*)(moduleInfo->extInfo));

    functionSlots[NVTX3EXT_CBID_nvtxPayloadSchemaRegister] = (intptr_t)impl::PayloadSchemaRegister;
    functionSlots[NVTX3EXT_CBID_nvtxPayloadEnumRegister] = (intptr_t)impl::PayloadEnumRegister;
    functionSlots[NVTX3EXT_CBID_nvtxMarkPayload] = (intptr_t)impl::MarkPayload;
    functionSlots[NVTX3EXT_CBID_nvtxRangePushPayload] = (intptr_t)impl::RangePushPayload;
    functionSlots[NVTX3EXT_CBID_nvtxRangePopPayload] = (intptr_t)impl::RangePopPayload;

    const char* formatEnv = std::getenv("NVTX_PAYLOAD_FORMAT");
    if (formatEnv && std::strcmp(formatEnv, "json") == 0)
    {
        g_payloadFormat = PayloadFormat::Json;
    }

    return 1;
}

} // namespace

// The initializing callback implementation. This function is called by NVTX when
// it is used the first time in the annotated application code.
extern "C" EXPORT_SYMBOL int InitializeInjectionNvtx2(NvtxGetExportTableFunc_t getExportTable)
{
    if (g_isTornDown)
    {
        return 0;
    }

    std::lock_guard<std::mutex> guard(g_mutex);

    printf("[NVTX][%d][%lld] InitializeInjectionNvtx2()\n", getpid(), GetCurrentTimeMs());

    // Setting callbacks, use appropriate `NVTX_CBID_*` index constants:
    //     NVTX_CB_MODULE_CORE: NVTX_CBID_CORE_* (enum NvtxCallbackIdCore, [1])
    //     NVTX_CB_MODULE_CUDA: NVTX_CBID_CUDA_* (enum NvtxCallbackIdCuda, [2])
    //     NVTX_CB_MODULE_OPENCL: NVTX_CBID_OPENCL_* (enum NvtxCallbackIdOpenCL, [3])
    //     NVTX_CB_MODULE_CUDART: NVTX_CBID_CUDART_* (enum NvtxCallbackIdCudaRt, [4])
    //     NVTX_CB_MODULE_CORE2: NVTX_CBID_CORE2_* (enum NvtxCallbackIdCore2, [5])
    //     NVTX_CB_MODULE_SYNC: NVTX_CBID_SYNC_* (enum NvtxCallbackIdSync, [6])
    //
    // [1] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvtxDetail/nvtxTypes.h#L152
    // [2] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvtxDetail/nvtxTypes.h#L198
    // [3] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvtxDetail/nvtxTypes.h#L228
    // [4] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvtxDetail/nvtxTypes.h#L214
    // [5] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvtxDetail/nvtxTypes.h#L175
    // [6] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvtxDetail/nvtxTypes.h#L250
    NvtxFunctionTable coreTable = GetFunctionTable(getExportTable, NVTX_CB_MODULE_CORE);
    *coreTable[NVTX_CBID_CORE_RangePushA] = reinterpret_cast<NvtxFunctionPointer>(impl::RangePushA);
    *coreTable[NVTX_CBID_CORE_RangePop] =
        (NvtxFunctionPointer)impl::RangePop; // C casting is also fine
    *coreTable[NVTX_CBID_CORE_MarkA] = reinterpret_cast<NvtxFunctionPointer>(impl::MarkA);
    // Consider adding other functions as needed.

    NvtxFunctionTable core2Table = GetFunctionTable(getExportTable, NVTX_CB_MODULE_CORE2);
    *core2Table[NVTX_CBID_CORE2_DomainCreateA] =
        reinterpret_cast<NvtxFunctionPointer>(impl::DomainCreateA);
    *core2Table[NVTX_CBID_CORE2_DomainDestroy] =
        reinterpret_cast<NvtxFunctionPointer>(impl::DomainDestroy);
    *core2Table[NVTX_CBID_CORE2_DomainRegisterStringA] =
        reinterpret_cast<NvtxFunctionPointer>(impl::DomainRegisterStringA);
    *core2Table[NVTX_CBID_CORE2_DomainMarkEx] =
        reinterpret_cast<NvtxFunctionPointer>(impl::DomainMarkEx);
    *core2Table[NVTX_CBID_CORE2_DomainRangePushEx] =
        reinterpret_cast<NvtxFunctionPointer>(impl::DomainRangePushEx);
    *core2Table[NVTX_CBID_CORE2_DomainRangePop] =
        reinterpret_cast<NvtxFunctionPointer>(impl::DomainRangePop);
    // Consider adding other functions as needed.

    // Consider filling other tables as needed.

    // Report successful initialization
    return 1;
}

extern "C" EXPORT_SYMBOL int InitializeInjectionNvtxExtension(nvtxExtModuleInfo_t* moduleInfo)
{
    if (g_isTornDown)
        return 0;
    std::lock_guard<std::mutex> guard(g_mutex);

    if (!moduleInfo)
    {
        fprintf(stderr, "[NVTX] InitializeInjectionNvtxExtension got NULL module info.\n");
        return 0;
    }

    if (moduleInfo->moduleId == NVTX_EXT_PAYLOAD_MODULEID)
    {
        return InitializePayloadExtension(moduleInfo);
    }

    return 1;
}
