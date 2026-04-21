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

#include <nvtx3/nvtx3.hpp>
#include <nvtx3/nvToolsExtSemanticsScope.h>

#include <cstdint>
#include <cstring>
#include <iostream>

static int g_failures = 0;

static void check_u64(char const* label, char const* expr,
                      uint64_t actual, uint64_t expected)
{
    if (actual != expected) {
        ++g_failures;
        std::cout << "  FAIL [" << label << "] " << expr
                  << " = 0x" << std::hex << actual
                  << " (expected 0x" << expected << ")" << std::dec << "\n";
    }
}

#define CHECK_U64(LABEL, ACTUAL, EXPECTED) check_u64((LABEL), #ACTUAL, static_cast<uint64_t>(ACTUAL), static_cast<uint64_t>(EXPECTED))

static nvtxSemanticsScope_v1 const&
as_scope(nvtx3::scope_semantic const& sem)
{
    return *reinterpret_cast<nvtxSemanticsScope_v1 const*>(sem.get());
}

extern "C" NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv);
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    g_failures = 0;

    (void)argc;
    (void)argv;

    {
        std::cout << "scope_semantic: default-constructed\n";
        nvtx3::scope_semantic sem;
        auto const& s = as_scope(sem);
        CHECK_U64("scope_default", s.header.semanticId, NVTX_SEMANTIC_ID_SCOPE_V1);
        CHECK_U64("scope_default", s.header.version,    NVTX_SCOPE_SEMANTIC_VERSION);
        CHECK_U64("scope_default", s.header.structSize, sizeof(nvtxSemanticsScope_v1));
        CHECK_U64("scope_default", s.header.next != nullptr, 0);
        CHECK_U64("scope_default", s.scopeId, NVTX_SCOPE_NONE);
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "scope_semantic: .scope(uint64_t)\n";
        nvtx3::scope_semantic sem;
        sem.scope(uint64_t{42});
        auto const& s = as_scope(sem);
        CHECK_U64("scope_raw", s.scopeId, 42);
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "scope_semantic: .scope(nvtx3::scope)\n";
        nvtx3::scope_semantic sem;
        sem.scope(nvtx3::scope::current_sw_thread());
        auto const& s = as_scope(sem);
        CHECK_U64("scope_typed", s.scopeId, NVTX_SCOPE_CURRENT_SW_THREAD);
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "scope_semantic: chained via raw header pointer\n";
        nvtx3::scope_semantic outer;
        outer.scope(nvtx3::scope::current_sw_process());
        nvtx3::scope_semantic inner{outer.get()};
        inner.scope(nvtx3::scope::current_sw_thread());
        auto const& o = as_scope(outer);
        auto const& i = as_scope(inner);
        CHECK_U64("scope_chain", o.scopeId, NVTX_SCOPE_CURRENT_SW_PROCESS);
        CHECK_U64("scope_chain", i.scopeId, NVTX_SCOPE_CURRENT_SW_THREAD);
        CHECK_U64("scope_chain", i.header.next == &o.header, 1);
    }
    std::cout << "-------------------------------------\n";

    if (g_failures != 0) {
        std::cout << "FAILED: " << g_failures << " semantic check(s) mismatched expected values\n";
    }
    return g_failures;
}
