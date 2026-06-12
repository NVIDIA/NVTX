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
#include <nvtx3/nvToolsExtSemanticsCorrelation.h>
#include <nvtx3/nvToolsExtSemanticsScope.h>
#include <nvtx3/nvToolsExtSemanticsTime.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

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

static void check_str(char const* label, char const* expr,
                      char const* actual, char const* expected)
{
    bool const eq = (actual == expected)
        || (actual && expected && std::strcmp(actual, expected) == 0);
    if (!eq) {
        ++g_failures;
        std::cout << "  FAIL [" << label << "] " << expr
                  << " = \"" << (actual ? actual : "(null)")
                  << "\" (expected \"" << (expected ? expected : "(null)") << "\")\n";
    }
}

#define CHECK_U64(LABEL, ACTUAL, EXPECTED) check_u64((LABEL), #ACTUAL, static_cast<uint64_t>(ACTUAL), static_cast<uint64_t>(EXPECTED))
#define CHECK_STR(LABEL, ACTUAL, EXPECTED) check_str((LABEL), #ACTUAL, (ACTUAL), (EXPECTED))

static nvtxSemanticsScope_v1 const&
as_scope(nvtx3::scope_semantic const& sem)
{
    return *reinterpret_cast<nvtxSemanticsScope_v1 const*>(sem.get());
}

static nvtxSemanticsTime_v1 const&
as_time(nvtx3::time_semantic const& sem)
{
    return *reinterpret_cast<nvtxSemanticsTime_v1 const*>(sem.get());
}

static nvtxSemanticsCorrelation_v1 const&
as_correlation(nvtx3::correlation_semantic const& sem)
{
    return *reinterpret_cast<nvtxSemanticsCorrelation_v1 const*>(sem.get());
}

struct scopes_domain
{
    static constexpr char const* name{"ScopesDomain"};
};

struct time_domains_domain
{
    static constexpr char const* name{"TimeDomainsDomain"};
};

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
        auto const* next = reinterpret_cast<nvtxSemanticsScope_v1 const*>(i.header.next);
        CHECK_U64("scope_chain", o.scopeId, NVTX_SCOPE_CURRENT_SW_PROCESS);
        CHECK_U64("scope_chain", i.scopeId, NVTX_SCOPE_CURRENT_SW_THREAD);
        CHECK_U64("scope_chain", next != nullptr, 1);
        CHECK_U64("scope_chain", next ? next->scopeId : 0, NVTX_SCOPE_CURRENT_SW_PROCESS);
        CHECK_U64("scope_chain", i.header.next == &o.header, 1);
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "time_semantic: default-constructed\n";
        nvtx3::time_semantic sem;
        auto const& t = as_time(sem);
        CHECK_U64("time_default", t.header.semanticId, NVTX_SEMANTIC_ID_TIME_V1);
        CHECK_U64("time_default", t.header.version,    NVTX_TIME_SEMANTIC_VERSION);
        CHECK_U64("time_default", t.header.structSize, sizeof(nvtxSemanticsTime_v1));
        CHECK_U64("time_default", t.header.next != nullptr, 0);
        CHECK_U64("time_default", t.timeDomainId, 0);
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "time_semantic: .time_domain(NVTX_TIMESTAMP_TYPE_*)\n";
        nvtx3::time_semantic sem;
        sem.time_domain(NVTX_TIMESTAMP_TYPE_CPU_TSC);
        auto const& t = as_time(sem);
        CHECK_U64("time_predefined", t.timeDomainId, NVTX_TIMESTAMP_TYPE_CPU_TSC);
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "time_semantic: chained to scope_semantic\n";
        nvtx3::scope_semantic scope_sem;
        scope_sem.scope(nvtx3::scope::current_sw_thread());
        nvtx3::time_semantic time_sem{scope_sem};
        time_sem.time_domain(NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC);
        auto const& t = as_time(time_sem);
        auto const* next = reinterpret_cast<nvtxSemanticsScope_v1 const*>(t.header.next);
        CHECK_U64("time_chain", t.timeDomainId, NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC);
        CHECK_U64("time_chain", next != nullptr, 1);
        CHECK_U64("time_chain", next ? next->scopeId : 0, NVTX_SCOPE_CURRENT_SW_THREAD);
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "time_semantic: chained from temporary scope_semantic\n";
        nvtx3::time_semantic time_sem{
            nvtx3::scope_semantic{}.scope(nvtx3::scope::current_sw_thread())};
        time_sem.time_domain(NVTX_TIMESTAMP_TYPE_CPU_TSC);
        auto const& t = as_time(time_sem);
        auto const* next = reinterpret_cast<nvtxSemanticsScope_v1 const*>(t.header.next);
        CHECK_U64("time_temp_chain", t.timeDomainId, NVTX_TIMESTAMP_TYPE_CPU_TSC);
        CHECK_U64("time_temp_chain", next != nullptr, 1);
        CHECK_U64("time_temp_chain", next ? next->scopeId : 0, NVTX_SCOPE_CURRENT_SW_THREAD);
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "NVTX3_SEMANTIC: chained temporary semantics\n";
        auto const* sem = NVTX3_SEMANTIC(
            nvtx3::time_semantic{
                nvtx3::scope_semantic{}.scope(nvtx3::scope::current_sw_process())}
                .time_domain(NVTX_TIMESTAMP_TYPE_CPU_TSC));
        auto const* t = reinterpret_cast<nvtxSemanticsTime_v1 const*>(sem);
        auto const* next = reinterpret_cast<nvtxSemanticsScope_v1 const*>(t->header.next);
        CHECK_U64("semantic_macro_chain", t->timeDomainId, NVTX_TIMESTAMP_TYPE_CPU_TSC);
        CHECK_U64("semantic_macro_chain", next != nullptr, 1);
        CHECK_U64("semantic_macro_chain", next ? next->scopeId : 0, NVTX_SCOPE_CURRENT_SW_PROCESS);
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "correlation_semantic: default-constructed\n";
        nvtx3::correlation_semantic sem;
        auto const& c = as_correlation(sem);
        CHECK_U64("corr_default", c.header.semanticId, NVTX_SEMANTIC_ID_CORRELATION_V1);
        CHECK_U64("corr_default", c.header.version,    NVTX_CORRELATION_SEMANTIC_VERSION);
        CHECK_U64("corr_default", c.header.structSize, sizeof(nvtxSemanticsCorrelation_v1));
        CHECK_U64("corr_default", c.header.next != nullptr, 0);
        CHECK_U64("corr_default", c.role, NVTX_CORRELATION_ROLE_NONE);
        CHECK_STR("corr_default", c.displayName, nullptr);
        for (int i = 0; i < 16; ++i) {
            CHECK_U64("corr_default_uuid", c.correlationDomainUuid[i], 0);
        }
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "correlation_semantic: .domain_uuid / .display_name / .role\n";
        static constexpr unsigned char example_uuid[16] = {
            0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
            0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
        nvtx3::correlation_semantic sem;
        std::string display_name{"ExampleCorrelationDomain"};
        sem.domain_uuid(example_uuid)
            .display_name(display_name)
            .role(uint64_t{42});
        auto const& c = as_correlation(sem);
        CHECK_U64("corr_full", c.role, 42);
        CHECK_STR("corr_full", c.displayName, "ExampleCorrelationDomain");
        for (int i = 0; i < 16; ++i) {
            CHECK_U64("corr_full_uuid", c.correlationDomainUuid[i], example_uuid[i]);
        }
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "correlation_semantic: chained correlation -> time -> scope\n";
        nvtx3::scope_semantic scope_sem;
        scope_sem.scope(nvtx3::scope::current_sw_process());
        nvtx3::time_semantic time_sem{scope_sem};
        time_sem.time_domain(NVTX_TIMESTAMP_TYPE_CPU_TSC);
        nvtx3::correlation_semantic corr_sem{time_sem};
        corr_sem.role(uint64_t{7});
        auto const& t = as_time(time_sem);
        auto const& c = as_correlation(corr_sem);
        auto const* next_time = reinterpret_cast<nvtxSemanticsTime_v1 const*>(c.header.next);
        auto const* next_scope = next_time
            ? reinterpret_cast<nvtxSemanticsScope_v1 const*>(next_time->header.next)
            : nullptr;
        CHECK_U64("corr_chain", c.role, 7);
        CHECK_U64("corr_chain", c.header.next != nullptr, 1);
        CHECK_U64("corr_chain", c.header.next == &t.header, 0);
        CHECK_U64("corr_chain", next_time ? next_time->timeDomainId : 0, NVTX_TIMESTAMP_TYPE_CPU_TSC);
        CHECK_U64("corr_chain", next_scope != nullptr, 1);
        CHECK_U64("corr_chain", next_scope ? next_scope->scopeId : 0, NVTX_SCOPE_CURRENT_SW_PROCESS);
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "scope_in: register a dynamic scope\n";
        nvtx3::scope_in<scopes_domain> gpu0{"GPU[CUDAID:0]"};
        std::cout << "  scope id: " << gpu0.id() << "\n";

        nvtx3::scope s = gpu0;
        CHECK_U64("scope_in_conv", s.get(), gpu0.id());

        nvtx3::scope_semantic sem;
        sem.scope(gpu0);
        auto const& c = *reinterpret_cast<nvtxSemanticsScope_v1 const*>(sem.get());
        CHECK_U64("scope_in_sem", c.scopeId, gpu0.id());
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "scope_in: register a dynamic scope from std::string\n";
        std::string path{"GPU[CUDAID:0]/stream[copy]"};
        nvtx3::scope_in<scopes_domain> stream{path};
        std::cout << "  scope id: " << stream.id() << "\n";

        nvtx3::scope s = stream;
        CHECK_U64("scope_in_string_conv", s.get(), stream.id());
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "scope_in: register a static scope with an explicit ID\n";
        uint64_t const static_id = NVTX_SCOPE_ID_STATIC_START + 7;
        nvtx3::scope_in<scopes_domain> leaf{
            "stream[compute]", nvtx3::scope::none(), static_id};
        std::cout << "  requested=" << static_id << " returned=" << leaf.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "scope_in: child scope under a parent scope\n";
        nvtx3::scope_in<scopes_domain> parent{"GPU[CUDAID:1]"};
        nvtx3::scope_in<scopes_domain> child{"stream[copy]", parent};
        std::cout << "  parent id: " << parent.id() << "\n";
        std::cout << "  child  id: " << child.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "timestamp(): read NVTX timestamp\n";
        int64_t t1 = nvtx3::timestamp();
        int64_t t2 = nvtx3::timestamp();
        std::cout << "  t1 = " << t1 << "\n";
        std::cout << "  t2 = " << t2 << "\n";
        CHECK_U64("timestamp_nondecreasing", t2 >= t1, 1);
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "time_domain_in: register and describe a timer source\n";
        nvtx3::time_domain_in<time_domains_domain> td{
            NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC,
            nvtx3::scope::current_sw_process(),
            NVTX_TIMER_FLAG_CLOCK_MONOTONIC,
            int64_t{1000000000},
            NVTX_TIMER_START_SYSTEM_BOOT};
        std::cout << "  time domain id: " << td.id() << "\n";

        // nvtxTimestampGet uses NVTX_API (__stdcall on 32-bit Windows), but
        // nvtxTimerSource expects a default-ABI callback.
        auto provider = +[]() -> int64_t { return nvtx3::timestamp(); };
        td.set_timer_source(NVTX_TIMER_FLAG_NONE, provider);

        static int64_t stub_counter = 0;
        auto with_data = +[](void* data) -> int64_t {
            return ++(*static_cast<int64_t*>(data));
        };
        td.set_timer_source(NVTX_TIMER_FLAG_NONE, with_data, &stub_counter);

        nvtx3::time_semantic sem;
        sem.time_domain(td.id());
        auto const& c = *reinterpret_cast<nvtxSemanticsTime_v1 const*>(sem.get());
        CHECK_U64("time_domain_in_sem", c.timeDomainId, td.id());
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "time_domain_in: sync_point / sync_point_table / conversion_factor\n";
        nvtx3::time_domain_in<time_domains_domain> monotonic{
            NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC};
        nvtx3::time_domain_in<time_domains_domain> tsc{NVTX_TIMESTAMP_TYPE_CPU_TSC};

        monotonic.sync_point(tsc.id(),
                             int64_t{1000000000},
                             int64_t{3000000000});

        nvtxSyncPoint_t points[3] = {
            {int64_t{1}, int64_t{3}},
            {int64_t{2}, int64_t{6}},
            {int64_t{3}, int64_t{9}},
        };
        monotonic.sync_point_table(tsc.id(), points, 3);

        monotonic.conversion_factor(tsc.id(), 3.0,
                                    int64_t{1000000000},
                                    int64_t{3000000000});
        std::cout << "  called sync_point / sync_point_table / conversion_factor OK\n";
    }
    std::cout << "-------------------------------------\n";

    if (g_failures != 0) {
        std::cout << "FAILED: " << g_failures << " semantic check(s) mismatched expected values\n";
    }
    return g_failures;
}
