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
struct char_test              { static constexpr const char*    name{"Test char"}; };
struct wchar_test             { static constexpr const wchar_t* name{L"Test wchar_t"}; };
struct error_name_missing     { static constexpr const char*    not_name{"Test name is missing"}; };
struct error_name_is_bad_type { static constexpr const int      name{5}; };

extern "C" NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv);
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    (void)argc;
    (void)argv;

    using namespace nvtx3;

    {
        std::cout << std::boolalpha;
        std::cout << "is_c_string<const char *>     = " << detail::is_c_string<const char*>::value << '\n';
        std::cout << "is_c_string<const wchar_t *>  = " << detail::is_c_string<const wchar_t*>::value << '\n';
        std::cout << "is_c_string<const char *&>    = " << detail::is_c_string<const char*&>::value << '\n';
        std::cout << "is_c_string<const wchar_t *&> = " << detail::is_c_string<const wchar_t*&>::value << '\n';
        std::cout << "is_c_string<const char *c&>   = " << detail::is_c_string<const char* const&>::value << '\n';
        std::cout << "is_c_string<const wchar_t *c&>= " << detail::is_c_string<const wchar_t* const&>::value << '\n';
        std::cout << "is_c_string<char *>     = " << detail::is_c_string<char*>::value << '\n';
        std::cout << "is_c_string<wchar_t *>  = " << detail::is_c_string<wchar_t*>::value << '\n';
        std::cout << "is_c_string<char *&>    = " << detail::is_c_string<char*&>::value << '\n';
        std::cout << "is_c_string<wchar_t *&> = " << detail::is_c_string<wchar_t*&>::value << '\n';
        std::cout << "is_c_string<char *c&>   = " << detail::is_c_string<char* const&>::value << '\n';
        std::cout << "is_c_string<wchar_t *c&>= " << detail::is_c_string<wchar_t* const&>::value << '\n';

        std::cout << "is_c_string<int>       = " << detail::is_c_string<int>::value << '\n';
        std::cout << "is_c_string<const int*>= " << detail::is_c_string<const int*>::value << '\n';
        std::cout << "is_c_string<void*>     = " << detail::is_c_string<void*>::value << '\n';

        std::cout << "-------------\n";
    }

    std::cout << "- Global domain (mark alias):\n";
    mark("Mark in global domain (implicit)");

    std::cout << "- Global domain implicit: ";
    auto& gi = domain::get<>();
    std::cout << gi << "\n";
    mark_in<>("Mark in global domain (implicit)");

    std::cout << "- Global domain explicit:";
    auto& ge = domain::get<domain::global>();
    std::cout << ge << "\n";
    mark_in<domain::global>("Mark in global domain (explicit)");

    std::cout << "- Test domain (char):";
    auto& d1 = domain::get<char_test>();
    std::cout << d1 << "\n";
    mark_in<char_test>("Mark in char_test domain");

    std::cout << "- Test domain (wchar_t):";
    auto& d2 = domain::get<wchar_test>();
    std::cout << d2 << "\n";
    mark_in<wchar_test>("Mark in wchar_test domain");

#if STATIC_ASSERT_TESTING

#if 1 // defined(ERROR_TEST_NAME_IS_MISSING)
    {
        std::cout << "- Error test - domain is missing name member:";
        auto& d3 = domain::get<error_name_missing>();
        std::cout << d3 << "\n";
        mark_in<error_name_missing>("Mark in error_name_missing domain");
        scoped_range_in<error_name_missing> r3("Mark in error_name_missing domain");
    }
#endif

#if 1 // defined(ERROR_TEST_NAME_IS_BAD_TYPE)
    {
        std::cout << "- Error test - domain name member isn't narrow or wide char array:";
        auto& d4 = domain::get<error_name_is_bad_type>();
        std::cout << d4 << "\n";
        mark_in<error_name_is_bad_type>("Mark in error_name_is_bad_type domain");
        scoped_range_in<error_name_is_bad_type> r4("Mark in error_name_is_bad_type domain");
    }
#endif

#endif // STATIC_ASSERT_TESTING

    return 0;
}
