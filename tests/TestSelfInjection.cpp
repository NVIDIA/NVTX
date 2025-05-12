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

#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <string>
#include <vector>

#include "SelfInjection.h"

struct S1
{
    int i;
    float f;
};
static inline bool operator==(S1 const& lhs, S1 const& rhs)
{
    return lhs.i == rhs.i && lhs.f == rhs.f;
}
static std::ostream& operator<<(std::ostream& lhs, S1 const& rhs)
{
    return lhs << '{' << rhs.i << ',' << rhs.f << '}';
}

struct S2
{
    int i;
    float f;
    const char* s;
};

static bool Same(S2 const& lhs, S2 const& rhs, SAME_COMMON_ARGS)
{
    bool same =
        Same(lhs.i, rhs.i, deep, verbose, "i", oss, depth + 1) &&
        Same(lhs.f, rhs.f, deep, verbose, "f", oss, depth + 1) &&
        Same(lhs.s, rhs.s, deep, verbose, "s", oss, depth + 1);
    if (verbose && !same) oss << std::string(depth, ' ') << "'" << name << "' members different\n";
    return same;
}

static bool TestSame(bool verbose, bool deep)
{
    std::cout << std::boolalpha;

    std::cout << "--- Simple ints:\n";
    {
        int xL = 5, xR = 5;
        bool result = Same(xL, xR, deep, verbose, "x");
        std::cout << "> == ints: " << result << '\n';
    }
    {
        int xL = 5, xR = 6;
        bool result = Same(xL, xR, deep, verbose, "x");
        std::cout << "> != ints: " << result << '\n';
    }

    std::cout << "--- C-style strings:\n";
    {
        const char* str = "String";
        bool result = Same(str, str, deep, verbose, "str");
        std::cout << "> char string w/itself: " << result << '\n';
    }
    {
        const char* strL = "String";
        const char* strR = "String";
        bool result = Same(strL, strR, deep, verbose, "str");
        std::cout << "> == char strings: " << result << '\n';
    }
    {
        const char* strL = "StringA";
        const char* strR = "StringB";
        bool result = Same(strL, strR, deep, verbose, "str");
        std::cout << "> != char strings: " << result << '\n';
    }

    std::cout << "--- Structs with == and << operators:\n";
    {
        S1 sL{5, 3.125f};
        S1 sR{5, 3.125f};
        bool result = Same(sL, sR, deep, verbose, "S1");
        std::cout << "> == S1s: " << result << '\n';
    }
    {
        S1 sL{5, 3.125f};
        S1 sR{5, 3.14159f};
        bool result = Same(sL, sR, deep, verbose, "S1");
        std::cout << "> != S1s: " << result << '\n';
    }

    std::cout << "--- Pointers to structs with == and << operators:\n";
    {
        S1 sL{5, 3.125f};
        S1* psL = &sL;
        bool result = Same(psL, psL, deep, verbose, "S1 ptr");
        std::cout << "> same ptr to an S1: " << result << '\n';
    }
    {
        S1 sL{5, 3.125f};
        S1 sR{5, 3.125f};
        S1* psL = &sL;
        S1* psR = &sR;
        bool result = Same(psL, psR, deep, verbose, "S1 ptr");
        std::cout << "> different ptrs to == S1s: " << result << '\n';
    }
    {
        S1 sL{5, 3.125f};
        S1 sR{5, 3.14159f};
        S1* psL = &sL;
        S1* psR = &sR;
        bool result = Same(psL, psR, deep, verbose, "S1 ptr");
        std::cout << "> different ptrs to != S1s: " << result << '\n';
    }

    std::cout << "--- Structs with Same function defined:\n";
    {
        S2 sL{5, 3.125f, "An S2"};
        S2 sR{5, 3.125f, "An S2"};
        bool result = Same(sL, sR, deep, verbose, "S2");
        std::cout << "> == S2s: " << result << '\n';
    }
    {
        S2 sL{5, 3.125f, "An S2"};
        S2 sR{5, 3.14159f, "An S2"};
        bool result = Same(sL, sR, deep, verbose, "S2");
        std::cout << "> !=f in S2s: " << result << '\n';
    }
    {
        S2 sL{5, 3.125f, "An S2"};
        S2 sR{5, 3.125f, "Another S2"};
        bool result = Same(sL, sR, deep, verbose, "S2");
        std::cout << "> !=s in S2s: " << result << '\n';
    }

    std::cout << "--- NVTX handles - pointers to incomplete types:\n";
    {
        auto hL = reinterpret_cast<nvtxDomainHandle_t>(1024);
        auto hR = reinterpret_cast<nvtxDomainHandle_t>(1024);
        bool result = Same(hL, hR, deep, verbose, "nvtxDomainHandle_t");
        std::cout << "> == domain handles: " << result << '\n';
    }
    {
        auto hL = reinterpret_cast<nvtxDomainHandle_t>(1024);
        auto hR = reinterpret_cast<nvtxDomainHandle_t>(2048);
        bool result = Same(hL, hR, deep, verbose, "nvtxDomainHandle_t");
        std::cout << "> != domain handles: " << result << '\n';
    }

    std::cout << "--- NVTX event attributes - struct with tagged union:\n";
    {
        char buf1[]{"Test message"};
        char buf2[]{"Test message"};

        nvtxEventAttributes_t aL{};
        aL.version = NVTX_VERSION;
        aL.size = sizeof(nvtxEventAttributes_t);
        aL.category = 5;
        aL.colorType = NVTX_COLOR_ARGB;
        aL.color = 0xFF446688;
        aL.payloadType = NVTX_PAYLOAD_TYPE_DOUBLE;
        aL.payload.dValue = 3.125;
        aL.messageType = NVTX_MESSAGE_TYPE_ASCII;
        aL.message.ascii = buf1;
        aL.reserved0 = 1;

        auto aR = aL;

        auto* paL = &aL;
        auto* paR = &aR;

        bool result = Same(aL, aR, deep, verbose, "nvtxEventAttributes_t");
        std::cout << "> == attrs: " << result << '\n';

        aR = aL;
        aR.reserved0 = 2;
        result = Same(aL, aR, deep, verbose, "nvtxEventAttributes_t");
        std::cout << "> == attrs with different padding: " << result << '\n';

        aR = aL;
        aR.category = 6;
        result = Same(aL, aR, deep, verbose, "nvtxEventAttributes_t");
        std::cout << "> != attrs, category: " << result << '\n';

        aR = aL;
        aR.message.ascii = buf2;
        result = Same(aL, aR, deep, verbose, "nvtxEventAttributes_t");
        std::cout << "> == attrs with same message in different buffers: " << result << '\n';

        aR = aL;
        aR.message.ascii = "Different message";
        result = Same(aL, aR, deep, verbose, "nvtxEventAttributes_t");
        std::cout << "> != attrs, message: " << result << '\n';

        aR = aL;
        aR.payloadType = NVTX_PAYLOAD_TYPE_FLOAT;
        result = Same(aL, aR, deep, verbose, "nvtxEventAttributes_t");
        std::cout << "> != attrs, payloadType: " << result << '\n';

        aR = aL;
        aR.payload.dValue = -3.125;
        result = Same(aL, aR, deep, verbose, "nvtxEventAttributes_t");
        std::cout << "> != attrs, payload union value: " << result << '\n';

        aR = aL;
        result = Same(paL, paL, deep, verbose, "nvtxEventAttributes_t by pointer");
        std::cout << "> == attr pointers: " << result << '\n';

        result = Same(paL, paR, deep, verbose, "nvtxEventAttributes_t by pointer");
        std::cout << "> == attr values, different pointers: " << result << '\n';

        aR.payload.dValue = -3.125;
        result = Same(paL, paR, deep, verbose, "nvtxEventAttributes_t by pointer");
        std::cout << "> != attr values, payload union value: " << result << '\n';
    }

    return true;
}

extern "C" NVTX_DYNAMIC_EXPORT
int RunTest(int /*argc*/, const char** /*argv*/);
NVTX_DYNAMIC_EXPORT
int RunTest(int /*argc*/, const char** /*argv*/)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    // Always verbose -- tests both verbose and non-verbose modes

    {
        std::cout << "\n------- Non-verbose, non-deep:\n";
        bool success = TestSame(false, false);
        if (!success) { std::cout << "TestSame returned false!\n"; return 1; }
    }
    {
        std::cout << "\n------- Non-verbose, deep:\n";
        bool success = TestSame(false, true);
        if (!success) { std::cout << "TestSame returned false!\n"; return 1; }
    }

    {
        std::cout << "\n------- Verbose, non-deep:\n";
        bool success = TestSame(true, false);
        if (!success) { std::cout << "TestSame returned false!\n"; return 1; }
    }
    {
        std::cout << "\n------- Verbose, deep:\n";
        bool success = TestSame(true, true);
        if (!success) { std::cout << "TestSame returned false!\n"; return 1; }
    }

    std::cout << "\n--------- Success!\n";
    return 0;
}
