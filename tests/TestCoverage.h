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

#include <iostream>
#include <string>

#include "PrettyPrintersNvtxCpp.h"

struct a_lib
{
    static constexpr const char* name{"Library A"};
    //static constexpr const float name{3.14f};
};

struct cat_x
{
    static constexpr const char* name{"Category X"};
    static constexpr uint32_t id{42};
};

struct cat_y
{
    static constexpr const char* name{"Category Y"};
    //static constexpr const float name{3.14f};
    static constexpr uint32_t id{43};
};

struct regstr_hello
{
    static constexpr const char* message{"Hello"};
};

static void TestFuncRange()
{
    NVTX3_FUNC_RANGE();
    nvtx3::mark("Marker in TestFuncRange");
}

static void TestFuncRangeV()
{
    NVTX3_V1_FUNC_RANGE();
    nvtx3::mark("Marker in TestFuncRangeV");
}

static void TestFuncRangeIfDyn(bool cond)
{
    NVTX3_FUNC_RANGE_IF(cond);
    nvtx3::mark("Marker in TestFuncRangeIfDyn");
}

static void TestFuncRangeIfDynV(bool cond)
{
    NVTX3_V1_FUNC_RANGE_IF(cond);
    nvtx3::mark("Marker in TestFuncRangeIfDynV");
}

static void TestFuncRangeIfStat(bool cond)
{
    NVTX3_FUNC_RANGE_IF(cond);
    nvtx3::mark("Marker in TestFuncRangeIfStat");
}

static void TestFuncRangeIfStatV(bool cond)
{
    NVTX3_V1_FUNC_RANGE_IF(cond);
    nvtx3::mark("Marker in TestFuncRangeIfStatV");
}

static void TestFuncRangeIn()
{
    NVTX3_FUNC_RANGE_IN(a_lib);
    nvtx3::mark("Marker in TestFuncRangeIn");
}

static void TestFuncRangeInV()
{
    NVTX3_V1_FUNC_RANGE_IN(a_lib);
    nvtx3::mark("Marker in TestFuncRangeInV");
}

static void TestFuncRangeIfInDyn(bool cond)
{
    NVTX3_FUNC_RANGE_IF_IN(a_lib, cond);
    nvtx3::mark("Marker in TestFuncRangeIfInDyn");
}

static void TestFuncRangeIfInDynV(bool cond)
{
    NVTX3_V1_FUNC_RANGE_IF_IN(a_lib, cond);
    nvtx3::mark("Marker in TestFuncRangeIfInDynV");
}

static void TestFuncRangeIfInStat(bool cond)
{
    NVTX3_FUNC_RANGE_IF_IN(a_lib, cond);
    nvtx3::mark("Marker in TestFuncRangeIfInStat");
}

static void TestFuncRangeIfInStatV(bool cond)
{
    NVTX3_V1_FUNC_RANGE_IF_IN(a_lib, cond);
    nvtx3::mark("Marker in TestFuncRangeIfInStatV");
}

static int RunTestCommon(int argc, const char** argv)
{
    bool verbose = false;
    const std::string verboseArg = "-v";
    for (; *argv; ++argv)
    {
        if (*argv == verboseArg) verbose = true;
    }

    using namespace nvtx3;

    {
        std::cout << "Default attributes:\n";
        event_attributes attr;
        if (verbose) std::cout << attr << '\n';
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a message (ascii), payload, color, and category:\n";
        event_attributes attr{
            message{"Hello"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}};
        if (verbose) std::cout << attr << '\n';
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a message with different string types:\n";

        event_attributes a{message{"Hello"}};
        if (verbose) std::cout << a << '\n';

        event_attributes wa{message{L"Hello"}};
        if (verbose) std::cout << wa << '\n';

        std::string hello{"Hello"};
        event_attributes b{message{hello}};
        if (verbose) std::cout << b << '\n';

        std::wstring whello{L"Hello"};
        event_attributes wb{message{whello}};
        if (verbose) std::cout << wb << '\n';

        // Important!  Neither of following will compile:
        //
        //   event_attributes c{message{std::string{"foo"}}};
        //   std::cout << c;
        //
        //   std::string foo{"foo"};
        //   event_attributes d{message{hello + "bar"}};
        //   std::cout << d;
        //
        // Both of those usages fail with:
        // "error C2280: 'message::message(std::string &&)':
        //  attempting to reference a deleted function"
        //
        // message is a "view" class, not an owning class.
        // It cannot take ownership of a temporary string and
        // destroy it when it goes out of scope.  Similarly,
        // event_attributes is not an owning class, so it cannot take
        // ownership of an message either.
        //
        // TODO:  Could we add implicit support for this?
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a message (registered):\n";
        auto hTacobell = reinterpret_cast<nvtxStringHandle_t>(0x7ac0be11);
        event_attributes attr{message{hTacobell}};
        if (verbose) std::cout << attr << '\n';
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "Convenience: Set a message without the helper type:\n";

        event_attributes a{"Hello"};
        if (verbose) std::cout << a << '\n';

        std::string hello{"Hello"};
        event_attributes b{hello};
        if (verbose) std::cout << b << '\n';
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a payload twice (first should win):\n";
        event_attributes attr{"test", payload{1.0f}, payload{2}};
        if (verbose) std::cout << attr << '\n';
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a color twice (first should win):\n";
        event_attributes attr{"test", argb{127,0,0,255}, rgb{0,255,0}};
        if (verbose) std::cout << attr << '\n';
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a message twice (first should win):\n";
        event_attributes attr{L"wide", "narrow"};
        if (verbose) std::cout << attr << '\n';
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a category twice (first should win):\n";
        event_attributes attr{"test", category{1}, category{2}};
        if (verbose) std::cout << attr << '\n';
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "Markers\n";

        // Global domain
        event_attributes attr{
            message{"Hello1"},
            category{11},
            payload{5.0f},
            rgb{1,2,3}};
        mark(attr);

        mark(event_attributes{
            message{"Hello2"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}});

        mark(
            message{"Hello3"},
            category{11},
            payload{5.0f},
            rgb{0,255,0});

        // a_lib domain
        event_attributes a_attr{
            message{"a: Hello1"},
            category{11},
            payload{5.0f},
            rgb{1,2,3}};
        mark_in<a_lib>(attr);

        mark_in<a_lib>(event_attributes{
            message{"a: Hello2"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}});

        mark_in<a_lib>(
            message{"a: Hello3"},
            category{11},
            payload{5.0f},
            rgb{0,255,0});
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "Range start/end and range_handle\n";

        // Global domain
        event_attributes attr{
            message{"Hello1"},
            category{11},
            payload{5.0f},
            rgb{1,2,3}};
        auto h1 = start_range(attr);

        auto h2 = start_range(event_attributes{
            message{"Hello2"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}});

        auto h3 = start_range(
            message{"Hello3"},
            category{11},
            payload{5.0f},
            rgb{0,255,0});

        // a_lib domain
        event_attributes a_attr{
            message{"a: Hello1"},
            category{11},
            payload{5.0f},
            rgb{1,2,3}};
        auto h4 = start_range_in<a_lib>(attr);

        auto h5 = start_range_in<a_lib>(event_attributes{
            message{"a: Hello2"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}});

        auto h6 = start_range_in<a_lib>(
            message{"a: Hello3"},
            category{11},
            payload{5.0f},
            rgb{0,255,0});

        // range_handle operator ==, !=, and cast overloads
        bool testEq = h1 == h2;
        bool testNe = h3 != h4;
        bool testCast = bool(h5);
        if (verbose) std::cout << std::boolalpha
            << testEq << "\n"
            << testNe << "\n"
            << testCast << "\n";

        end_range(h1);
        end_range(h2);
        end_range(h3);

        end_range_in<a_lib>(h4);
        end_range_in<a_lib>(h5);
        end_range_in<a_lib>(h6);
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "unique_range\n";

        // Global domain
        event_attributes attr{
            message{"Hello1"},
            category{11},
            payload{5.0f},
            rgb{1,2,3}};
        unique_range u1{attr};

        unique_range u2{event_attributes{
            message{"Hello2"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}}};

        unique_range u3{
            message{"Hello3"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}};

        // a_lib domain
        event_attributes a_attr{
            message{"a: Hello1"},
            category{11},
            payload{5.0f},
            rgb{1,2,3}};
        unique_range_in<a_lib> u4{attr};

        unique_range_in<a_lib> u5{event_attributes{
            message{"a: Hello2"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}}};

        unique_range_in<a_lib> u6{
            message{"a: Hello3"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}};

        // movability
        auto move_in_out_global = [](unique_range           u) { return u; };
        auto move_in_out_domain = [](unique_range_in<a_lib> u) { return u; };

        auto u1moved = move_in_out_global(std::move(u1));
        auto u4moved = move_in_out_domain(std::move(u4));
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "scoped_range\n";

        // Global domain
        event_attributes attr{
            message{"Hello1"},
            category{11},
            payload{5.0f},
            rgb{1,2,3}};
        scoped_range s1{attr};

        scoped_range s2{event_attributes{
            message{"Hello2"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}}};

        scoped_range s3{
            message{"Hello3"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}};

        // a_lib domain
        event_attributes a_attr{
            message{"a: Hello1"},
            category{11},
            payload{5.0f},
            rgb{1,2,3}};
        scoped_range_in<a_lib> s4{attr};

        scoped_range_in<a_lib> s5{event_attributes{
            message{"a: Hello2"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}}};

        scoped_range_in<a_lib> s6{
            message{"a: Hello3"},
            category{11},
            payload{5.0f},
            rgb{0,255,0}};
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "named_category\n";

        // Global domain
        mark                   ("Cat", named_category::get                   <cat_x>());
        mark_in<>              ("Cat", named_category_in<>::get              <cat_x>());
        mark_in<domain::global>("Cat", named_category_in<domain::global>::get<cat_x>());

        // a_lib domain
        mark_in<a_lib>("Cat", named_category_in<a_lib>::get<cat_y>());
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "registered_string\n";

        // Global domain
        mark                   ("RegStr", registered_string::get                   <regstr_hello>());
        mark_in<>              ("RegStr", registered_string_in<>::get              <regstr_hello>());
        mark_in<domain::global>("RegStr", registered_string_in<domain::global>::get<regstr_hello>());

        // a_lib domain
        mark_in<a_lib>("RegStr", registered_string_in<a_lib>::get<regstr_hello>());
    }
    if (verbose) std::cout << "-------------------------------------\n";

    {
        std::cout << "Macros:\n";
        TestFuncRange();
        TestFuncRangeV();
        TestFuncRangeIfDyn(argc == 1001);
        TestFuncRangeIfDyn(argc != 1001);
        TestFuncRangeIfDynV(argc == 1002);
        TestFuncRangeIfDynV(argc != 1002);
        TestFuncRangeIfStat(true);
        TestFuncRangeIfStat(false);
        TestFuncRangeIfStatV(true);
        TestFuncRangeIfStatV(false);

        TestFuncRangeIn();
        TestFuncRangeInV();
        TestFuncRangeIfInDyn(argc == 1003);
        TestFuncRangeIfInDyn(argc != 1003);
        TestFuncRangeIfInDynV(argc == 1004);
        TestFuncRangeIfInDynV(argc != 1004);
        TestFuncRangeIfInStat(true);
        TestFuncRangeIfInStat(false);
        TestFuncRangeIfInStatV(true);
        TestFuncRangeIfInStatV(false);
    }
    if (verbose) std::cout << "-------------------------------------\n";

    return 0;
}
