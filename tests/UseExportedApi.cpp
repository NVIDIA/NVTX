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

#include "PathHelper.h"

#include <nvtx3/nvToolsExt.h>
#include <nvtx3/nvToolsExtCuda.h>
#include <nvtx3/nvToolsExtCudaRt.h>
#include <nvtx3/nvToolsExtOpenCL.h>
#include <nvtx3/nvToolsExtSync.h>

#include <iostream>
#include <string>

// Use an X-macro to allow doing arbitrary operations to all exported API functions.
// An easy way to generate this list is to use Linux and dump the exports from libexport-api.so.
// I recommend having a bash script to do this, e.g. "exports":
//
//    #!/bin/bash
//    nm -D "$@" | perl -ne 'print if s/^\S+ T //'
//
// Then typing "exports libexport-api.so" will dump a plain list of the exported symbols.
// That can be piped into perl or sed again to add the X-macro stuff, e.g.:
//
//    exports libexport-api.so | perl -ne 'chomp; print "    func($_) \\\n"'
//
// Running that command would produce the exact text you can use for the implementation of this
// macro.  Don't forget to leave at least one blank line after the macro so the backslash on the
// last line doesn't connect the macro to the next line of code afterwards.
//
// Double-check when generating the list of exports from libexport-api.so that it does in fact
// contain the expected number of exported functions!!!  If you automate generating this macro
// as part of the build, then failure to export some symbols would result in failure to include
// them in this list of symbols to test!
//
#define FOR_EACH_EXPORT(func) \
    func(nvtxDomainCreateA) \
    func(nvtxDomainCreateW) \
    func(nvtxDomainDestroy) \
    func(nvtxDomainMarkEx) \
    func(nvtxDomainNameCategoryA) \
    func(nvtxDomainNameCategoryW) \
    func(nvtxDomainRangeEnd) \
    func(nvtxDomainRangePop) \
    func(nvtxDomainRangePushEx) \
    func(nvtxDomainRangeStartEx) \
    func(nvtxDomainRegisterStringA) \
    func(nvtxDomainRegisterStringW) \
    func(nvtxDomainResourceCreate) \
    func(nvtxDomainResourceDestroy) \
    func(nvtxDomainSyncUserAcquireFailed) \
    func(nvtxDomainSyncUserAcquireStart) \
    func(nvtxDomainSyncUserAcquireSuccess) \
    func(nvtxDomainSyncUserCreate) \
    func(nvtxDomainSyncUserDestroy) \
    func(nvtxDomainSyncUserReleasing) \
    func(nvtxInitialize) \
    func(nvtxMarkA) \
    func(nvtxMarkEx) \
    func(nvtxMarkW) \
    func(nvtxNameCategoryA) \
    func(nvtxNameCategoryW) \
    func(nvtxNameClCommandQueueA) \
    func(nvtxNameClCommandQueueW) \
    func(nvtxNameClContextA) \
    func(nvtxNameClContextW) \
    func(nvtxNameClDeviceA) \
    func(nvtxNameClDeviceW) \
    func(nvtxNameClEventA) \
    func(nvtxNameClEventW) \
    func(nvtxNameClMemObjectA) \
    func(nvtxNameClMemObjectW) \
    func(nvtxNameClProgramA) \
    func(nvtxNameClProgramW) \
    func(nvtxNameClSamplerA) \
    func(nvtxNameClSamplerW) \
    func(nvtxNameCuContextA) \
    func(nvtxNameCuContextW) \
    func(nvtxNameCuDeviceA) \
    func(nvtxNameCuDeviceW) \
    func(nvtxNameCuEventA) \
    func(nvtxNameCuEventW) \
    func(nvtxNameCuStreamA) \
    func(nvtxNameCuStreamW) \
    func(nvtxNameCudaDeviceA) \
    func(nvtxNameCudaDeviceW) \
    func(nvtxNameCudaEventA) \
    func(nvtxNameCudaEventW) \
    func(nvtxNameCudaStreamA) \
    func(nvtxNameCudaStreamW) \
    func(nvtxNameOsThreadA) \
    func(nvtxNameOsThreadW) \
    func(nvtxRangeEnd) \
    func(nvtxRangePop) \
    func(nvtxRangePushA) \
    func(nvtxRangePushEx) \
    func(nvtxRangePushW) \
    func(nvtxRangeStartA) \
    func(nvtxRangeStartEx) \
    func(nvtxRangeStartW) \

// ^ Above line must be left blank, since last line of macro ends with a backslash

template <typename FnPtr>
FnPtr GetExport(
    DLL_HANDLE hDll,
    const char* fnName,
    std::vector<const char*>& found,
    std::vector<const char*>& missing)
{
    FnPtr pfn = (FnPtr)GET_DLL_FUNC(hDll, fnName);
    if (pfn)
    {
        found.push_back(fnName);
    }
    else
    {
        missing.push_back(fnName);
    }
    return pfn;
}

#define DEFINE_AND_GET_FN_PTR_FOR_EXPORT(fn) \
    auto pfn_##fn = GetExport<decltype(&fn)>(hDll, #fn, foundFuncs, missingFuncs);

extern "C" NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    bool verbose = false;
    const std::string verboseArg = "-v";
    for (; *argv; ++argv)
    {
        if (*argv == verboseArg) verbose = true;
    }

    if (verbose) std::cout << "-------------------------------------\n";

    // Construct abs path to export-api library
    std::string exportApiLib = AbsolutePathToLibraryInCurrentProcessPath("export-api");

    // Load export-api library
    DLL_HANDLE hDll = DLL_OPEN(exportApiLib.c_str());
    if (!hDll) return 201;

    std::vector<const char*> foundFuncs, missingFuncs;

    // For each export, try to GET_DLL_FUNC for it
    //     - Don't early-out, print list of all failed exports

    //auto pfn_nvtxMarkA = GetExport<decltype(&nvtxMarkA)>(hDll, "nvtxMarkA", foundFuncs, missingFuncs);
    //auto pfn_nvtxDomainCreateA = GetExport<decltype(&nvtxDomainCreateA)>(hDll, "nvtxDomainCreateA", foundFuncs, missingFuncs);
    // ...
    FOR_EACH_EXPORT(DEFINE_AND_GET_FN_PTR_FOR_EXPORT)

    if (verbose) std::cout << " - Got non-zero pointers for " << foundFuncs.size() << " NVTX functions.\n";

    if (verbose) std::cout << " - Trying to call some NVTX functions through the exports...\n";

    // For a few simple functions, try calling them through function pointers with
    // harmless args.  If the calling conventions are wrong, these calls will crash.
    // If they are working, the NVTX injection should load and print something.
    if (pfn_nvtxMarkA)
    {
        pfn_nvtxMarkA("Testing nvtxMarkA");
    }

    if (pfn_nvtxDomainCreateA)
    {
        auto hDomain = pfn_nvtxDomainCreateA("Testing nvtxDomainCreateA");
        (void)hDomain;
    }

    if (verbose) std::cout << " - Survived calling NVTX functions.\n";

    if (!missingFuncs.empty())
    {
        if (verbose)
        {
            std::cout << "Missing exports:\n";
            for (auto fnName : missingFuncs)
            {
                std::cout << "    " << fnName << "\n";
            }
        }
        return 202;
    }

    if (verbose) std::cout << "-------------------------------------\n";

    return 0;
}
