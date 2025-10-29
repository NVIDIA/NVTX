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

#include <nvtx3/nvtx3.hpp>

#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <string>
#include <vector>

#include "SelfInjection.h"
#include "PrettyPrintersNvtxC.h"

class CallbackTester
{
    Callbacks stored;
    std::vector<Call> calls;
public:

public:
    void Record(Call const& call) { calls.push_back(call); }

    CallbackTester() : stored(g_callbacks)
    {
        g_callbacks.Default = [&](Call const& call) { Record(call); };
    }
    ~CallbackTester() { g_callbacks = stored; }

    bool CallsMatch(std::vector<Call> expCalls, bool verbose = false) const
    {
        auto cmp = [&](Call const& lhs, Call const& rhs)
        {
            return Same(lhs, rhs, true, verbose, "NVTX call");
        };

        bool match = std::equal(calls.begin(), calls.end(), expCalls.begin(), cmp);
        if (verbose && !match)
        {
            auto printCall = [](Call const& c) { std::cout << "    " << *c << "\n"; };
            std::cout << "Did not get expected NVTX C API call sequence!  Expected:\n";
            for (auto& c : expCalls) printCall(c);
            std::cout << "Recorded:\n";
            for (auto& c : calls) printCall(c);
        }

        return match;
    }
};

template <int N> struct a_lib { static constexpr const char* name = "LibA"; };
template <int N> struct b_lib { static constexpr const char* name = "LibB"; };
template <int N> struct c_lib { static constexpr const char* name = "LibC"; };

template <int N> struct cat1 { static constexpr const char* name = "Cat1"; static constexpr const uint32_t id = 1; };
template <int N> struct cat2 { static constexpr const char* name = "Cat2"; static constexpr const uint32_t id = 2; };
template <int N> struct cat3 { static constexpr const char* name = "Cat3"; static constexpr const uint32_t id = 3; };

template <int N> struct reg1 { static constexpr const char* message = "Reg1"; };
template <int N> struct reg2 { static constexpr const char* message = "Reg2"; };
template <int N> struct reg3 { static constexpr const char* message = "Reg3"; };

extern "C" NVTX_DYNAMIC_EXPORT
int RunTest(int /*argc*/, const char** argv);
NVTX_DYNAMIC_EXPORT
int RunTest(int /*argc*/, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    bool verbose = false;
    const std::string verboseArg = "-v";
    for (; *argv; ++argv)
    {
        if (*argv == verboseArg) verbose = true;
    }

    using namespace nvtx3;

    //---------------------------- Tests --------------------------------------

    if (verbose) std::cout << "--------- Testing injection loader\n";

    {
        CallbackTester t;

        nvtxInitialize(nullptr);
        nvtxInitialize(nullptr);

        if (!t.CallsMatch({
            CALL_LOAD(1),
            CALL(CORE2, Initialize, nullptr),
            CALL(CORE2, Initialize, nullptr)
        }, verbose)) return 1;
    }

    if (verbose) std::cout << "--------- Testing C API\n";

    {
        CallbackTester t;

        const char* teststr = "Testing 1 2 3!";
        nvtxMarkA(teststr);

        if (!t.CallsMatch({
            CALL(CORE, MarkA, teststr)
        }, verbose)) return 1;
    }

    {
        CallbackTester t;

        char teststr[] = "Testing 1 2 3!";
        nvtxMarkA(teststr);
        memcpy(teststr,  "Overwritten!!!", sizeof(teststr));

        if (!t.CallsMatch({
            CALL(CORE, MarkA, "Testing 1 2 3!")
        }, verbose)) return 1;
    }

    {
        CallbackTester t;

        wchar_t teststr[] = L"Testing 1 2 3!";
        nvtxMarkW(teststr);
        memcpy(teststr,     L"Overwritten!!!", sizeof(teststr));

        if (!t.CallsMatch({
            CALL(CORE, MarkW, L"Testing 1 2 3!")
        }, verbose)) return 1;
    }

    {
        CallbackTester t;

        nvtxEventAttributes_t attr{NVTX_VERSION, sizeof(nvtxEventAttributes_t),
            0, 0, 0, 0, 0, {0}, 0, {nullptr}};
        attr.category = 123;
        attr.colorType = NVTX_COLOR_ARGB;
        attr.color = 0xFF4466BB;
        attr.messageType = NVTX_MESSAGE_TYPE_ASCII;
        attr.message = MakeMessage("Test MarkEX");
        attr.category = 123;
        attr.payloadType = NVTX_PAYLOAD_TYPE_DOUBLE;
        attr.payload = MakePayload(3.14159);
        nvtxMarkEx(&attr);

        nvtxEventAttributes_t attr2 = attr;
        memset(&attr, 0, sizeof(attr));

        if (!t.CallsMatch({
            CALL(CORE, MarkEx, &attr2)
        }, verbose)) return 1;
    }

    if (verbose) std::cout << "--------- Testing C++ API\n";

    {
        CallbackTester t;

        mark("Testing 1 2 3!");
        mark(L"Testing 1 2 3!");

        if (!t.CallsMatch({
            CALL(CORE2, DomainMarkEx, nullptr, event_attributes{"Testing 1 2 3!"}.get()),
            CALL(CORE2, DomainMarkEx, nullptr, event_attributes{L"Testing 1 2 3!"}.get())
        }, verbose)) return 1;
    }

    {
        CallbackTester t;

        nvtxEventAttributes_t attrExpected{NVTX_VERSION, sizeof(nvtxEventAttributes_t),
            123, // category
            NVTX_COLOR_ARGB, 0xFF4466BB,
            NVTX_PAYLOAD_TYPE_DOUBLE, 0, MakePayload(3.14159),
            NVTX_MESSAGE_TYPE_ASCII, MakeMessage("Test msg")
        };

        // Same args, different order
        mark("Test msg", rgb(0x44, 0x66, 0xBB), category(123), payload(3.14159));
        mark(payload(3.14159), "Test msg", rgb(0x44, 0x66, 0xBB), category(123));
        mark(category(123), payload(3.14159), "Test msg", rgb(0x44, 0x66, 0xBB));
        mark(rgb(0x44, 0x66, 0xBB), category(123), payload(3.14159), "Test msg");

        // Same args with duplicates, test first-one-wins behavior (including union type changes)
        mark("Test msg", rgb(0x44, 0x66, 0xBB), category(123), payload(3.14159),
            "Bad msg", rgb(0x10, 0x20, 0x30), category(321), payload(3.0));
        mark("Test msg", rgb(0x44, 0x66, 0xBB), category(123), payload(3.14159),
            L"Bad message");
        mark("Test msg", rgb(0x44, 0x66, 0xBB), category(123), payload(3.14159),
            payload(3.14159f));

        if (!t.CallsMatch({
            7, CALL(CORE2, DomainMarkEx, nullptr, &attrExpected)
        }, verbose)) return 1;
    }

    {
        CallbackTester t;
        constexpr int N = 1;
        auto hA = reinterpret_cast<nvtxDomainHandle_t>(1);

        mark_in<a_lib<N>>("First call");
        mark_in<a_lib<N>>("Second call");
        mark_in<a_lib<N>>("Third call");

        if (!t.CallsMatch({
            CALL(CORE2, DomainCreateA, "LibA"),
            CALL(CORE2, DomainMarkEx,  hA, event_attributes{"First call"}.get()),
            CALL(CORE2, DomainMarkEx,  hA, event_attributes{"Second call"}.get()),
            CALL(CORE2, DomainMarkEx,  hA, event_attributes{"Third call"}.get())
        }, verbose)) return 1;
    }

    {
        CallbackTester t;
        constexpr int N = 2;
        auto hA = reinterpret_cast<nvtxDomainHandle_t>(1);
        auto hB = reinterpret_cast<nvtxDomainHandle_t>(2);

        mark_in<a_lib<N>>("First call");
        mark_in<a_lib<N>>("Second call");
        mark_in<b_lib<N>>("First call");
        mark_in<b_lib<N>>("Second call");

        if (!t.CallsMatch({
            CALL(CORE2, DomainCreateA, "LibA"),
            CALL(CORE2, DomainMarkEx,  hA, event_attributes{"First call"}.get()),
            CALL(CORE2, DomainMarkEx,  hA, event_attributes{"Second call"}.get()),
            CALL(CORE2, DomainCreateA, "LibB"),
            CALL(CORE2, DomainMarkEx,  hB, event_attributes{"First call"}.get()),
            CALL(CORE2, DomainMarkEx,  hB, event_attributes{"Second call"}.get())
        }, verbose)) return 1;
    }

    {
        CallbackTester t;
        constexpr int N = 3;
        auto hA = reinterpret_cast<nvtxDomainHandle_t>(1);
        auto hB = reinterpret_cast<nvtxDomainHandle_t>(2);

        mark_in<a_lib<N>>("DA, Cat 1, call 1", named_category_in<a_lib<N>>::get<cat1<N>>());
        mark_in<a_lib<N>>("DA, Cat 1, call 2", named_category_in<a_lib<N>>::get<cat1<N>>());
        mark_in<a_lib<N>>("DA, Cat 2, call 1", named_category_in<a_lib<N>>::get<cat2<N>>());
        mark_in<a_lib<N>>("DA, Cat 2, call 2", named_category_in<a_lib<N>>::get<cat2<N>>());
        mark_in<b_lib<N>>("DB, Cat 1, call 1", named_category_in<b_lib<N>>::get<cat1<N>>());
        mark_in<b_lib<N>>("DB, Cat 1, call 2", named_category_in<b_lib<N>>::get<cat1<N>>());
        mark_in<b_lib<N>>("DB, Cat 2, call 1", named_category_in<b_lib<N>>::get<cat2<N>>());
        mark_in<b_lib<N>>("DB, Cat 2, call 2", named_category_in<b_lib<N>>::get<cat2<N>>());

        if (!t.CallsMatch({
            CALL(CORE2, DomainCreateA,       "LibA"),
            CALL(CORE2, DomainNameCategoryA, hA, 1, "Cat1"),
            CALL(CORE2, DomainMarkEx,        hA, event_attributes{"DA, Cat 1, call 1", category(1)}.get()),
            CALL(CORE2, DomainMarkEx,        hA, event_attributes{"DA, Cat 1, call 2", category(1)}.get()),
            CALL(CORE2, DomainNameCategoryA, hA, 2, "Cat2"),
            CALL(CORE2, DomainMarkEx,        hA, event_attributes{"DA, Cat 2, call 1", category(2)}.get()),
            CALL(CORE2, DomainMarkEx,        hA, event_attributes{"DA, Cat 2, call 2", category(2)}.get()),
            CALL(CORE2, DomainCreateA,       "LibB"),
            CALL(CORE2, DomainNameCategoryA, hB, 1, "Cat1"),
            CALL(CORE2, DomainMarkEx,        hB, event_attributes{"DB, Cat 1, call 1", category(1)}.get()),
            CALL(CORE2, DomainMarkEx,        hB, event_attributes{"DB, Cat 1, call 2", category(1)}.get()),
            CALL(CORE2, DomainNameCategoryA, hB, 2, "Cat2"),
            CALL(CORE2, DomainMarkEx,        hB, event_attributes{"DB, Cat 2, call 1", category(2)}.get()),
            CALL(CORE2, DomainMarkEx,        hB, event_attributes{"DB, Cat 2, call 2", category(2)}.get()),
        }, verbose)) return 1;
    }

    {
        CallbackTester t;
        constexpr int N = 4;
        auto hA = reinterpret_cast<nvtxDomainHandle_t>(1);
        auto hB = reinterpret_cast<nvtxDomainHandle_t>(2);
        auto hReg1 = reinterpret_cast<nvtxStringHandle_t>(1);
        auto hReg2 = reinterpret_cast<nvtxStringHandle_t>(2);

        mark_in<a_lib<N>>(registered_string_in<a_lib<N>>::get<reg1<N>>());
        mark_in<a_lib<N>>(registered_string_in<a_lib<N>>::get<reg1<N>>());
        mark_in<a_lib<N>>(registered_string_in<a_lib<N>>::get<reg2<N>>());
        mark_in<a_lib<N>>(registered_string_in<a_lib<N>>::get<reg2<N>>());
        mark_in<b_lib<N>>(registered_string_in<b_lib<N>>::get<reg1<N>>());
        mark_in<b_lib<N>>(registered_string_in<b_lib<N>>::get<reg1<N>>());
        mark_in<b_lib<N>>(registered_string_in<b_lib<N>>::get<reg2<N>>());
        mark_in<b_lib<N>>(registered_string_in<b_lib<N>>::get<reg2<N>>());

        if (!t.CallsMatch({
            CALL(CORE2, DomainCreateA,         "LibA"),
            CALL(CORE2, DomainRegisterStringA, hA, "Reg1"),
            CALL(CORE2, DomainMarkEx,          hA, event_attributes{hReg1}.get()),
            CALL(CORE2, DomainMarkEx,          hA, event_attributes{hReg1}.get()),
            CALL(CORE2, DomainRegisterStringA, hA, "Reg2"),
            CALL(CORE2, DomainMarkEx,          hA, event_attributes{hReg2}.get()),
            CALL(CORE2, DomainMarkEx,          hA, event_attributes{hReg2}.get()),
            CALL(CORE2, DomainCreateA,         "LibB"),
            CALL(CORE2, DomainRegisterStringA, hB, "Reg1"),
            CALL(CORE2, DomainMarkEx,          hB, event_attributes{hReg1}.get()),
            CALL(CORE2, DomainMarkEx,          hB, event_attributes{hReg1}.get()),
            CALL(CORE2, DomainRegisterStringA, hB, "Reg2"),
            CALL(CORE2, DomainMarkEx,          hB, event_attributes{hReg2}.get()),
            CALL(CORE2, DomainMarkEx,          hB, event_attributes{hReg2}.get()),
        }, verbose)) return 1;
    }

    {
        CallbackTester t;
        constexpr int N = 5;
        auto hA = reinterpret_cast<nvtxDomainHandle_t>(1);
        auto hB = reinterpret_cast<nvtxDomainHandle_t>(2);
        auto hReg1 = reinterpret_cast<nvtxStringHandle_t>(1);
        auto hReg2 = reinterpret_cast<nvtxStringHandle_t>(2);

        auto& a_regstr1 = registered_string_in<a_lib<N>>::get<reg1<N>>();
        auto& a_regstr2 = registered_string_in<a_lib<N>>::get<reg2<N>>();
        auto& b_regstr1 = registered_string_in<b_lib<N>>::get<reg1<N>>();
        auto& b_regstr2 = registered_string_in<b_lib<N>>::get<reg2<N>>();

        auto& a_cat1 = named_category_in<a_lib<N>>::get<cat1<N>>();
        auto& a_cat2 = named_category_in<a_lib<N>>::get<cat2<N>>();
        auto& b_cat1 = named_category_in<b_lib<N>>::get<cat1<N>>();
        auto& b_cat2 = named_category_in<b_lib<N>>::get<cat2<N>>();

        mark_in<a_lib<N>>(a_cat1, a_regstr1);
        mark_in<a_lib<N>>(a_cat1, a_regstr1);
        mark_in<a_lib<N>>(a_cat2, a_regstr2);
        mark_in<a_lib<N>>(a_cat2, a_regstr2);
        mark_in<b_lib<N>>(b_cat1, b_regstr1);
        mark_in<b_lib<N>>(b_cat1, b_regstr1);
        mark_in<b_lib<N>>(b_cat2, b_regstr2);
        mark_in<b_lib<N>>(b_cat2, b_regstr2);

        if (!t.CallsMatch({
            CALL(CORE2, DomainCreateA,         "LibA"),
            CALL(CORE2, DomainRegisterStringA, hA, "Reg1"),
            CALL(CORE2, DomainRegisterStringA, hA, "Reg2"),
            CALL(CORE2, DomainCreateA,         "LibB"),
            CALL(CORE2, DomainRegisterStringA, hB, "Reg1"),
            CALL(CORE2, DomainRegisterStringA, hB, "Reg2"),
            CALL(CORE2, DomainNameCategoryA,   hA, 1, "Cat1"),
            CALL(CORE2, DomainNameCategoryA,   hA, 2, "Cat2"),
            CALL(CORE2, DomainNameCategoryA,   hB, 1, "Cat1"),
            CALL(CORE2, DomainNameCategoryA,   hB, 2, "Cat2"),
            CALL(CORE2, DomainMarkEx,          hA, event_attributes{hReg1, category(1)}.get()),
            CALL(CORE2, DomainMarkEx,          hA, event_attributes{hReg1, category(1)}.get()),
            CALL(CORE2, DomainMarkEx,          hA, event_attributes{hReg2, category(2)}.get()),
            CALL(CORE2, DomainMarkEx,          hA, event_attributes{hReg2, category(2)}.get()),
            CALL(CORE2, DomainMarkEx,          hB, event_attributes{hReg1, category(1)}.get()),
            CALL(CORE2, DomainMarkEx,          hB, event_attributes{hReg1, category(1)}.get()),
            CALL(CORE2, DomainMarkEx,          hB, event_attributes{hReg2, category(2)}.get()),
            CALL(CORE2, DomainMarkEx,          hB, event_attributes{hReg2, category(2)}.get()),
        }, verbose)) return 1;
    }

    {
        CallbackTester t;
        constexpr int N = 6;
        auto hA = reinterpret_cast<nvtxDomainHandle_t>(1);
        auto hB = reinterpret_cast<nvtxDomainHandle_t>(2);

        {
            scoped_range_in<a_lib<N>> r1("Sequential range 1");
            mark_in<a_lib<N>>("Mark in range");
        }
        {
            scoped_range_in<a_lib<N>> r2("Sequential range 2");
            mark_in<a_lib<N>>("Mark in range");
        }
        {
            scoped_range_in<a_lib<N>> r1("Nested range 1");
            scoped_range_in<a_lib<N>> r2("Nested range 2");
            mark_in<a_lib<N>>("Mark in range");
        }

        {
            scoped_range_in<b_lib<N>> r1("Sequential range 1");
            mark_in<b_lib<N>>("Mark in range");
        }
        {
            scoped_range_in<b_lib<N>> r2("Sequential range 2");
            mark_in<b_lib<N>>("Mark in range");
        }
        {
            scoped_range_in<b_lib<N>> r1("Nested range 1");
            scoped_range_in<b_lib<N>> r2("Nested range 2");
            mark_in<b_lib<N>>("Mark in range");
        }

        if (!t.CallsMatch({
            CALL(CORE2, DomainCreateA,     "LibA"),
            CALL(CORE2, DomainRangePushEx, hA, event_attributes{"Sequential range 1"}.get()),
            CALL(CORE2, DomainMarkEx,      hA, event_attributes{"Mark in range"}.get()),
            CALL(CORE2, DomainRangePop,    hA),
            CALL(CORE2, DomainRangePushEx, hA, event_attributes{"Sequential range 2"}.get()),
            CALL(CORE2, DomainMarkEx,      hA, event_attributes{"Mark in range"}.get()),
            CALL(CORE2, DomainRangePop,    hA),
            CALL(CORE2, DomainRangePushEx, hA, event_attributes{"Nested range 1"}.get()),
            CALL(CORE2, DomainRangePushEx, hA, event_attributes{"Nested range 2"}.get()),
            CALL(CORE2, DomainMarkEx,      hA, event_attributes{"Mark in range"}.get()),
            CALL(CORE2, DomainRangePop,    hA),
            CALL(CORE2, DomainRangePop,    hA),
            CALL(CORE2, DomainCreateA,     "LibB"),
            CALL(CORE2, DomainRangePushEx, hB, event_attributes{"Sequential range 1"}.get()),
            CALL(CORE2, DomainMarkEx,      hB, event_attributes{"Mark in range"}.get()),
            CALL(CORE2, DomainRangePop,    hB),
            CALL(CORE2, DomainRangePushEx, hB, event_attributes{"Sequential range 2"}.get()),
            CALL(CORE2, DomainMarkEx,      hB, event_attributes{"Mark in range"}.get()),
            CALL(CORE2, DomainRangePop,    hB),
            CALL(CORE2, DomainRangePushEx, hB, event_attributes{"Nested range 1"}.get()),
            CALL(CORE2, DomainRangePushEx, hB, event_attributes{"Nested range 2"}.get()),
            CALL(CORE2, DomainMarkEx,      hB, event_attributes{"Mark in range"}.get()),
            CALL(CORE2, DomainRangePop,    hB),
            CALL(CORE2, DomainRangePop,    hB),
        }, verbose)) return 1;
    }

    if (verbose) std::cout << "--------- Success!\n";
    return 0;
}
