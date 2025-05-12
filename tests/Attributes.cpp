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
// Include again to catch bad guards
#include <nvtx3/nvtx3.hpp>

#include <iostream>
#include <string>

#include "PrettyPrintersNvtxCpp.h"

struct a_lib
{
    static constexpr const char* name{"Library A"};
};

extern "C" NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv);
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    (void)argc;
    (void)argv;

    {
        std::cout << "Default attributes:\n";
        nvtx3::event_attributes attr;
        std::cout << attr;
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a payload:\n";
        nvtx3::event_attributes attr{nvtx3::payload{5.0f}};
        std::cout << attr;
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a color with RGB hex code 0xFF7F00:\n";
        nvtx3::event_attributes attr{nvtx3::color{0xFFFF7F00}};
        std::cout << attr;
    }
    std::cout << "-------------------------------------\n";


    {
        std::cout << "Set a color with RGB=255,127,0:\n";
        nvtx3::event_attributes attr{nvtx3::rgb{255,127,0}};
        std::cout << attr;
    }
    std::cout << "-------------------------------------\n";


    {
        std::cout << "Set a color & payload:\n";
        nvtx3::event_attributes attr{nvtx3::rgb{255,127,0}, nvtx3::payload{5.0f}};
        std::cout << attr;
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a color (red), payload, color again (green)... first color wins:\n";

        nvtx3::event_attributes attr{
            nvtx3::rgb{255,0,0},
            nvtx3::payload{5.0f},
            nvtx3::rgb{0, 255, 0}};

        std::cout << attr;
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a message (ascii), payload, color, and category:\n";

        nvtx3::event_attributes attr{
            nvtx3::message{"Hello"},
            nvtx3::category{11},
            nvtx3::payload{5.0f},
            nvtx3::rgb{0,255,0}};

        std::cout << attr;
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a message with different string types:\n";

        nvtx3::event_attributes a{nvtx3::message{"Hello"}};
        std::cout << a;

        nvtx3::event_attributes wa{nvtx3::message{L"Hello"}};
        std::cout << wa;

        std::string hello{"Hello"};
        nvtx3::event_attributes b{nvtx3::message{hello}};
        std::cout << b;

        std::wstring whello{L"Hello"};
        nvtx3::event_attributes wb{nvtx3::message{whello}};
        std::cout << wb;

        // Important!  Neither of following will compile:
        //
        //   nvtx3::event_attributes c{nvtx3::message{std::string{"foo"}}};
        //   std::cout << c;
        //
        //   std::string foo{"foo"};
        //   nvtx3::event_attributes d{nvtx3::message{hello + "bar"}};
        //   std::cout << d;
        //
        // Both of those usages fail with:
        // "error C2280: 'nvtx3::message::message(std::string &&)':
        //  attempting to reference a deleted function"
        //
        // nvtx3::message is a "view" class, not an owning class.
        // It cannot take ownership of a temporary string and
        // destroy it when it goes out of scope.  Similarly,
        // nvtx3::event_attributes is not an owning class, so it cannot take
        // ownership of an nvtx3::message either.
        //
        // TODO:  Could we add implicit support for this?
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Set a message (registered):\n";
        auto hTacobell = reinterpret_cast<nvtxStringHandle_t>(0x7ac0be11);
        nvtx3::event_attributes attr{nvtx3::message{hTacobell}};
        std::cout << attr;
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Set category/message/payload/color, with \"using\":\n";

        using namespace nvtx3;

        event_attributes a{
            category{11},
            message{"Hello"},
            payload{5.0f},
            rgb{1,2,3}};

        std::cout << a;
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Convenience: Set a message without the helper type:\n";

        nvtx3::event_attributes a{"Hello"};
        std::cout << a;

        std::string hello{"Hello"};
        nvtx3::event_attributes b{hello};
        std::cout << b;
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Examples: \"using\", skip helper type for msg, set other fields:\n";

        using namespace nvtx3;

        event_attributes a{"Hello", payload{7.0}};
        std::cout << a;

        event_attributes b{"Hello", rgb{255,255,0}};
        std::cout << b;

        event_attributes c{"Hello", category{4}};
        std::cout << c;

        // Order doesn't matter
        event_attributes d{"Hello", rgb{255,255,0}, payload{7.0}, category{4}};
        std::cout << d;

        event_attributes e{payload{7.0}, "Hello", category{4}, rgb{255,255,0}};
        std::cout << e;

        event_attributes f{category{4}, rgb{255,255,0}, payload{7.0}, "Hello"};
        std::cout << f;

        // Vertical formatting is nice too:
        event_attributes g{
            "Hello",
            category{4},
            rgb{255,255,0},
            payload{7.0}};
        std::cout << g;

        event_attributes h
        {
            "Hello",
            category{4},
            rgb{255,255,0},
            payload{7.0}
        };
        std::cout << h;
    }
    std::cout << "-------------------------------------\n";

    return 0;
}
