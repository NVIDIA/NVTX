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

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdio.h>

#include <nvtx3/nvToolsExt.h>

#ifdef _WIN32
#include <process.h>
#include <processthreadsapi.h>

#define EXPORT_SYMBOL   __declspec(dllexport)
#define getpid _getpid
#define gettid GetCurrentThreadId
#else
#include <unistd.h>

#define EXPORT_SYMBOL   __attribute__((visibility("default")))
#ifdef __APPLE__
static inline int gettid(void)
{
    uint64_t tid;
    pthread_threadid_np(nullptr, &tid);
    return static_cast<int>(tid);
}
#endif
#endif

// The `nvtxDomainRegistration_st`s content is implementation-defined. For NVTX, it is just a forward
// declaration [1], likewise for `nvtxStringRegistration_st` [2].
// See the `DomainCreateA()/DomainDestroy()` callbacks below for how its lifetime is managed by NVTX.
//
// [1] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvToolsExt.h#L377
// [2] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvToolsExt.h#L391
struct nvtxDomainRegistration_st {
    const char* name;
};

namespace {

std::mutex g_mutex;
std::atomic<bool> g_isTornDown{false};
struct TearDownDetector
{
    ~TearDownDetector() {
        g_isTornDown = true;
    }
} g_tearDownDetector;

// Returns a callback table ([1]) specified by the `module` argument or `nullptr` in case of error.
// See `module`'s  valid values in the `NvtxCallbackModule` enum [2].
//
// [1] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvtxDetail/nvtxTypes.h#L277
// [2] https://github.com/NVIDIA/NVTX/blob/v3.1.1/c/include/nvtx3/nvtxDetail/nvtxTypes.h#L138
NvtxFunctionTable GetFunctionTable(NvtxGetExportTableFunc_t getExportTable, NvtxCallbackModule module) {
    auto callbacks = reinterpret_cast<const NvtxExportTableCallbacks*>(getExportTable(NVTX_ETID_CALLBACKS));
    if (!callbacks) {
        fprintf(stderr, "[NVTX] Could not get NVTX_ETID_CALLBACKS.\n");
        return nullptr;
    }

    NvtxFunctionTable table = nullptr;
    unsigned int tableSize = 0;
    if (!callbacks->GetModuleFunctionTable(module, &table, &tableSize)) {
        fprintf(stderr, "[NVTX] Could not get function table of module %d.\n", module);
        return nullptr;
    }

    return table;
}

static long long GetCurrentTimeMs() {
    auto nowSinceEpoch = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(nowSinceEpoch).count();
}

namespace impl {

int RangePushA(const char* message)
{
    // The injection may need to perform some finalizing steps (like flushing collected
    // data to the disk), after which it stops accepting calls (that may come from other
    // threads) to guarantee consistency. Checking an atomic flag (`g_isTornDown`) is one
    // of the possible ways to do it.
    if (g_isTornDown) return NVTX_FAIL;

    // Collecting data from multiple threads often requires shared memory, which needs protection.
    // Here, the output of `printf` calls is being protected from interleaving. While mutexes are
    // a simple solution, lock-free data structures should be considered if performance is a concern.
    std::lock_guard<std::mutex> guard(g_mutex);

    printf("[NVTX][%d][%lld] PUSH %s\n", gettid(), GetCurrentTimeMs(), message);
    return NVTX_NO_PUSH_POP_TRACKING;
}
int RangePop()
{
    if (g_isTornDown) return NVTX_FAIL;
    std::lock_guard<std::mutex> guard(g_mutex);

    printf("[NVTX][%d][%lld] POP\n", gettid(), GetCurrentTimeMs());
    return NVTX_NO_PUSH_POP_TRACKING;
}
nvtxDomainHandle_t DomainCreateA(const char* name)
{
    if (g_isTornDown) return nullptr;
    std::lock_guard<std::mutex> guard(g_mutex);

    printf("[NVTX][%d][%lld] DOMAIN CREATE %s \n", gettid(), GetCurrentTimeMs(), name);
    return new nvtxDomainRegistration_st({name});
}
void DomainDestroy(nvtxDomainHandle_t domain)
{
    if (g_isTornDown) return;
    std::lock_guard<std::mutex> guard(g_mutex);

    // TODO: Remove calling `nvtxDomainDestroy()` with NULL in python's
    // `DomainHandle::__dealloc__()` (NVTX/python/nvtx/_lib/lib.pyx) and
    // remove this check.
    if (!domain) {
        return;
    }
    printf("[NVTX][%d][%lld] DOMAIN DESTROY %s\n", gettid(), GetCurrentTimeMs(), domain->name);
    delete domain;
}
void MarkA(const char* message)
{
    if (g_isTornDown) return;
    std::lock_guard<std::mutex> guard(g_mutex);

    printf("[NVTX][%d][%lld] MARK %s\n", gettid(), GetCurrentTimeMs(), message);
}
void DomainMarkEx(nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib)
{
    if (g_isTornDown) return;
    std::lock_guard<std::mutex> guard(g_mutex);

    const char* markName = eventAttrib ? eventAttrib->message.ascii : "No name";
    const char* domainName = domain ? domain->name : "Default domain";
    printf("[NVTX][%d][%lld] MARK %s@%s\n", gettid(), GetCurrentTimeMs(), markName, domainName);
}
void DomainRangePushEx(nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib)
{
    if (g_isTornDown) return;
    std::lock_guard<std::mutex> guard(g_mutex);

    const char* markName = eventAttrib ? eventAttrib->message.ascii : "No name";
    const char* domainName = domain ? domain->name : "Default domain";
    printf("[NVTX][%d][%lld] PUSH %s@%s\n", gettid(), GetCurrentTimeMs(), markName, domainName);
}
void DomainRangePop(nvtxDomainHandle_t domain)
{
    if (g_isTornDown) return;
    std::lock_guard<std::mutex> guard(g_mutex);

    const char* domainName = domain ? domain->name : "Default domain";
    printf("[NVTX][%d][%lld] POP @%s\n", gettid(), GetCurrentTimeMs(), domainName);
}
}  // namespace impl

}  // namespace

// The initializing callback implementation. This function is called by NVTX when
// it is used the first time in the annotated application code.
extern "C"
EXPORT_SYMBOL int InitializeInjectionNvtx2(NvtxGetExportTableFunc_t getExportTable)
{
    if (g_isTornDown) return 0;
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
    *coreTable[NVTX_CBID_CORE_RangePop] = (NvtxFunctionPointer) impl::RangePop; // C casting is also fine
    *coreTable[NVTX_CBID_CORE_MarkA] = reinterpret_cast<NvtxFunctionPointer>(impl::MarkA);
    // Consider adding other functions as needed.

    NvtxFunctionTable core2Table = GetFunctionTable(getExportTable, NVTX_CB_MODULE_CORE2);
    *core2Table[NVTX_CBID_CORE2_DomainCreateA] = reinterpret_cast<NvtxFunctionPointer>(impl::DomainCreateA);
    *core2Table[NVTX_CBID_CORE2_DomainDestroy] = reinterpret_cast<NvtxFunctionPointer>(impl::DomainDestroy);
    *core2Table[NVTX_CBID_CORE2_DomainMarkEx] = reinterpret_cast<NvtxFunctionPointer>(impl::DomainMarkEx);
    *core2Table[NVTX_CBID_CORE2_DomainRangePushEx] = reinterpret_cast<NvtxFunctionPointer>(impl::DomainRangePushEx);
    *core2Table[NVTX_CBID_CORE2_DomainRangePop] = reinterpret_cast<NvtxFunctionPointer>(impl::DomainRangePop);
    // Consider adding other functions as needed.

    // Consider filling other tables as needed.

    // Report successful initialization
    return 1;
}
