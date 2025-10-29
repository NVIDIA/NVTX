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

#if defined(_MSC_VER) && _MSC_VER < 1914
#define STATIC_ASSERT_TESTING 0
#else
#define STATIC_ASSERT_TESTING 1
#endif

#if defined(STATIC_ASSERT_TESTING)
#include <stdio.h>
#define NVTX3_STATIC_ASSERT(c, m) do { if (!(c)) printf("static_assert would fail: %s\n", m); } while (0)
#endif

#include <nvtx3/nvtx3.hpp>

#include <iostream>

// Domain description types
struct d { static constexpr const char*    name{"Test domain"}; };

// Registered string types
struct regstr_char_test      { static constexpr const char*    message{"Reg str char"};     };
struct regstr_wchar_test     { static constexpr const wchar_t* message{L"Reg str wchar_t"}; };
struct error_msg_missing     { static constexpr const char*    x      {"Name"}; };
struct error_msg_is_bad_type { static constexpr const int      message{5}; };
struct regstr_global_domain1 { static constexpr const char*    message{"Global1"}; };
struct regstr_global_domain2 { static constexpr const char*    message{"Global2"}; };
struct regstr_global_domain3 { static constexpr const char*    message{"Global3"}; };

extern "C" NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv);
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    (void)argc;
    (void)argv;

    using namespace nvtx3;

    std::cout << "- Domain: ";
    auto& d1 = domain::get<d>();
    std::cout << d1 << "\n";

#if 1
    std::cout << "- Registered string (char): ";
    auto& r1 = registered_string_in<d>::get<regstr_char_test>();
    std::cout << r1.get_handle() << "\n";
    mark_in<d>("Mark in regstr_char_test category", registered_string_in<d>::get<regstr_char_test>());

    std::cout << "- Registered string (wchar_t): ";
    auto& r2 = registered_string_in<d>::get<regstr_wchar_test>();
    std::cout << r2.get_handle() << "\n";
    mark_in<d>("Mark in regstr_wchar_test category", registered_string_in<d>::get<regstr_wchar_test>());
#endif

#if 1
    std::cout << "- Registered string in global domain (alias): ";
    auto& rd1 = registered_string::get<regstr_global_domain1>();
    std::cout << rd1.get_handle() << "\n";

    std::cout << "- Registered string in global domain (implicit): ";
    auto& rd2 = registered_string_in<>::get<regstr_global_domain2>();
    std::cout << rd2.get_handle() << "\n";

    std::cout << "- Registered string in global domain (explicit): ";
    auto& rd3 = registered_string_in<domain::global>::get<regstr_global_domain3>();
    std::cout << rd3.get_handle() << "\n";
#endif

#if STATIC_ASSERT_TESTING

#if 1 // defined(ERROR_TEST_MSG_IS_MISSING)
    {
        std::cout << "- Error test - registered string is missing name member: ";
        auto& r3 = registered_string_in<d>::get<error_msg_missing>();
        std::cout << r3.get_handle() << "\n";
    }
#endif

#if 1 // defined(ERROR_TEST_MSG_IS_BAD_TYPE)
    {
        std::cout << "- Error test - registered string message member isn't narrow or wide char array: ";
        auto& r4 = registered_string_in<d>::get<error_msg_is_bad_type>();
        std::cout << r4.get_handle() << "\n";
    }
#endif

#endif // STATIC_ASSERT_TESTING

    return 0;
}
