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

// Named category types
struct cat_char_test          { static constexpr const char*    name{"Cat char"};     static constexpr uint32_t id{1};  };
struct cat_wchar_test         { static constexpr const wchar_t* name{L"Cat wchar_t"}; static constexpr uint32_t id{2};  };
struct error_name_missing     { static constexpr const char*    x   {"Name"};         static constexpr uint32_t id{3};  };
struct error_name_is_bad_type { static constexpr const int      name{5};              static constexpr uint32_t id{4};  };
struct error_id_missing       { static constexpr const char*    name{"Name"};         static constexpr uint32_t y {5};  };
struct error_id_is_bad_type   { static constexpr const char*    name{"Name"};         static constexpr float    id{6};  };
struct error_both_missing     { static constexpr const char*    x   {"Name"};         static constexpr uint32_t y {7};  };
struct error_both_bad_type    { static constexpr const int      name{5};              static constexpr float    id{8};  };
struct error_no_name_bad_id   { static constexpr const char*    x   {"Name"};         static constexpr float    id{9};  };
struct error_bad_name_no_id   { static constexpr const int      name{5};              static constexpr uint32_t y {10}; };
struct cat_global_domain1     { static constexpr const char*    name{"Global1"};      static constexpr uint32_t id{11}; };
struct cat_global_domain2     { static constexpr const char*    name{"Global2"};      static constexpr uint32_t id{12}; };
struct cat_global_domain3     { static constexpr const char*    name{"Global3"};      static constexpr uint32_t id{13}; };

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
    std::cout << "- Named category (char): ";
    auto& c1 = named_category_in<d>::get<cat_char_test>();
    std::cout << c1.get_id() << "\n";
    mark_in<d>("Mark in cat_char_test category", named_category_in<d>::get<cat_char_test>());

    std::cout << "- Named category (wchar_t): ";
    auto& c2 = named_category_in<d>::get<cat_wchar_test>();
    std::cout << c2.get_id() << "\n";
    mark_in<d>("Mark in cat_wchar_test category", named_category_in<d>::get<cat_wchar_test>());
#endif

#if 1
    std::cout << "- Named category in global domain (alias): ";
    auto& cd1 = named_category::get<cat_global_domain1>();
    std::cout << cd1.get_id() << "\n";

    std::cout << "- Named category in global domain (implicit): ";
    auto& cd2 = named_category_in<>::get<cat_global_domain2>();
    std::cout << cd2.get_id() << "\n";

    std::cout << "- Named category in global domain (explicit): ";
    auto& cd3 = named_category_in<domain::global>::get<cat_global_domain3>();
    std::cout << cd3.get_id() << "\n";
#endif

#if STATIC_ASSERT_TESTING

#if 1 // defined(ERROR_TEST_NAME_IS_MISSING)
    {
        std::cout << "- Error test - category is missing name member:\n";
    }
#endif

#if 1 // defined(ERROR_TEST_NAME_IS_BAD_TYPE)
    {
        std::cout << "- Error test - category name member isn't narrow or wide char array: ";
        auto& c4 = named_category_in<d>::get<error_name_is_bad_type>();
        std::cout << c4.get_id() << "\n";
    }
#endif

#if 1 // defined(ERROR_TEST_ID_IS_MISSING)
    {
        std::cout << "- Error test - category is missing id member: ";
        auto& c5 = named_category_in<d>::get<error_id_missing>();
        std::cout << c5.get_id() << "\n";
    }
#endif

#if 1 // defined(ERROR_TEST_ID_IS_BAD_TYPE)
    {
        std::cout << "- Error test - category id member isn't uint32_t: ";
        auto& c6 = named_category_in<d>::get<error_id_is_bad_type>();
        std::cout << c6.get_id() << "\n";
    }
#endif

#if 1 // defined(ERROR_TEST_BOTH_MISSING)
    {
        std::cout << "- Error test - category is missing both members: ";
        auto& c7 = named_category_in<d>::get<error_both_missing>();
        std::cout << c7.get_id() << "\n";
    }
#endif

#if 1 // defined(ERROR_TEST_BOTH_BAD_TYPE)
    {
        std::cout << "- Error test - category members are both bad types: ";
        auto& c8 = named_category_in<d>::get<error_both_bad_type>();
        std::cout << c8.get_id() << "\n";
    }
#endif

#if 1 // defined(ERROR_TEST_NO_NAME_BAD_ID)
    {
        std::cout << "- Error test - category has no name and bad id type: ";
        auto& c9 = named_category_in<d>::get<error_no_name_bad_id>();
        std::cout << c9.get_id() << "\n";
    }
#endif

#if 1 // defined(ERROR_TEST_BAD_NAME_NO_ID)
    {
        std::cout << "- Error test - category has bad name type and no id: ";
        auto& c10 = named_category_in<d>::get<error_bad_name_no_id>();
        std::cout << c10.get_id() << "\n";
    }
#endif

#endif // STATIC_ASSERT_TESTING

    return 0;
}
