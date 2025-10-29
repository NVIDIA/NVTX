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

#include <type_traits>
#include <iostream>
#include <memory>
#include <string>
#include <wchar.h>
#include <string.h>

//-----------------------------------------------------------------------------------------------
// Implementations of "Same" function for various types
//   Provides better comparison capabilities than operator==
//   - Option for shallow or deep comparision (i.e. pointers vs. what they point at)
//   - Option for verbose mode, with a custom ostream to write to
//   - Option to specify name string for what is being compared
//   - Option for indent depth, so nested comparisons can print unwinding mismatch messages
//-----------------------------------------------------------------------------------------------

// C++11-compatible SFINAE helpers to choose overloads based on whether a type is complete or not
template <typename... Ts> struct make_void { typedef void type; };
template <typename... Ts> using void_t = typename make_void<Ts...>::type;
template <bool B> using enable_if = typename std::enable_if<B, int>::type;
template <typename, typename = void> struct is_complete { static constexpr bool value = false; };
template <typename T> struct is_complete<T, void_t<decltype(sizeof(T))>> { static constexpr bool value = true; };

#define SAME_COMMON_ARGS \
    bool deep = false, bool verbose = false, const char* name = "<unspecified>", std::ostream& oss = std::cout, int depth = 0

// Test if two objects are the same.  When 'deep' is true, ignore pointer values and only
// compare pointed-to contents, otherwise behave as operator==.  When 'verbose' is true,
// print information about differences to 'oss'.  The generic overload only works if there's
// an operator== and operator<< defined.
template <typename T>
inline auto Same(T const& lhs, T const& rhs, SAME_COMMON_ARGS)
    -> decltype(lhs == rhs, oss << lhs, bool())
{
    (void)!deep; /* unused */
    bool objSame = lhs == rhs;
    if (verbose && !objSame)
    {
        oss << std::string(depth, ' ') << "'" << name << "' different:  values are "
            << lhs << " and " << rhs
            // << " (type is " << typeid(lhs).name() << ")"
            << '\n';
    }
    return objSame;
}

// Generic pointer overload for complete types
template <typename T, enable_if<is_complete<T>::value> = 0>
inline bool Same(T* lhs, T* rhs, SAME_COMMON_ARGS)
{
    if (deep)
    {
        return Same(*lhs, *rhs, deep, verbose, name, oss, depth);
    }
    else
    {
        bool ptrSame = lhs == rhs;
        if (verbose && !ptrSame)
        {
            oss << std::string(depth, ' ') << "'" << name << "' different:  pointer values are 0x"
                << static_cast<const void*>(lhs) << " and 0x" << static_cast<const void*>(rhs) << '\n';
        }
        return ptrSame;
    }
}

// Generic pointer overload for incomplete types
template <typename T, enable_if<!is_complete<T>::value> = 0>
inline bool Same(T* lhs, T* rhs, SAME_COMMON_ARGS)
{
    (void)!deep; /* unused */
    // Don't know how to deep-copy incomplete types, so always compare pointers
    bool ptrSame = lhs == rhs;
    if (verbose && !ptrSame)
    {
        oss << std::string(depth, ' ') << "'" << name << "' different:  pointer values (to incomplete type) are 0x"
            << static_cast<const void*>(lhs) << " and 0x" << static_cast<const void*>(rhs) << '\n';
    }
    return ptrSame;
}

// Overloads for smart pointers -- in all cases, forward to contained raw pointer.
// In deep mode the comparison will be on the pointed-at objects, and in non-deep
// mode the comparison will be on the raw pointer values.
template <typename T>
inline bool Same(std::shared_ptr<T> const& lhs, std::shared_ptr<T> const& rhs, SAME_COMMON_ARGS)
{
    return Same(lhs.get(), rhs.get(), deep, verbose, name, oss, depth);
}

template <typename T>
inline bool Same(std::unique_ptr<T> const& lhs, std::unique_ptr<T> const& rhs, SAME_COMMON_ARGS)
{
    return Same(lhs.get(), rhs.get(), deep, verbose, name, oss, depth);
}

// Overloads for C-style strings (narrow and wide)
inline bool Same(char const* lhs, char const* rhs, SAME_COMMON_ARGS)
{
    if (deep)
    {
        bool strSame = strcmp(lhs, rhs) == 0;
        if (verbose && !strSame)
        {
            oss << std::string(depth, ' ') << "'" << name << "' different:  char strings are \""
                << lhs << "\" and \"" << rhs << "\"\n";
        }
        return strSame;
    }
    else
    {
        bool ptrSame = lhs == rhs;
        if (verbose && !ptrSame)
        {
            oss << std::string(depth, ' ') << "'" << name << "' different:  pointer values are "
                << static_cast<const void*>(lhs) << " and " << static_cast<const void*>(rhs) << '\n';
        }
        return ptrSame;
    }
}

inline bool Same(wchar_t const* lhs, wchar_t const* rhs, SAME_COMMON_ARGS)
{
    if (deep)
    {
        bool strSame = wcscmp(lhs, rhs) == 0;
        if (verbose && !strSame)
        {
            oss << std::string(depth, ' ') << "'" << name << "' different:  wchar_t strings are L\""
                << "<TODO>" << "\" and L\"" << "<TODO>" << "\"\n";
        }
        return strSame;
    }
    else
    {
        bool ptrSame = lhs == rhs;
        if (verbose && !ptrSame)
        {
            oss << std::string(depth, ' ') << "'" << name << "' different:  pointer values are "
                << static_cast<const void*>(lhs) << " and " << static_cast<const void*>(rhs) << '\n';
        }
        return ptrSame;
    }
}

// Helper macros to define Same() overloads (and operators == and !=) for struct and tagged union types

#define MEMBER_SAME(member) Same(lhs.member, rhs.member, deep, verbose, #member, oss, depth + 1)
#define PVOID_MEMBER_SAME(member) Same(NVTX_REINTERPRET_CAST(intptr_t, lhs.member), NVTX_REINTERPRET_CAST(intptr_t, rhs.member), deep, verbose, #member, oss, depth + 1)
#define UNION_MEMBER_SAME(tagField, tagValue, member) (lhs.tagField == tagValue && MEMBER_SAME(member))

#define VERBOSE_PRINT() if (verbose && !same) oss << std::string(depth, ' ') << "'" << name << "' members different\n"

#define EQ_SIG(T)     inline bool operator==(T const& lhs, T const& rhs)
#define NE_FROM_EQ(T) inline bool operator!=(T const& lhs, T const& rhs) { return !(lhs == rhs); }

#define DEFINE_EQ_NE_DEEP(T)    EQ_SIG(T) { return Same(lhs, rhs, true ); } NE_FROM_EQ(T)
#define DEFINE_EQ_NE_SHALLOW(T) EQ_SIG(T) { return Same(lhs, rhs, false); } NE_FROM_EQ(T)

#define DEFINE_MEMBER_SAME_1(a)       MEMBER_SAME(a)
#define DEFINE_MEMBER_SAME_2(a, b)    MEMBER_SAME(a) && DEFINE_MEMBER_SAME_1(b)
#define DEFINE_MEMBER_SAME_3(a, b, c) MEMBER_SAME(a) && DEFINE_MEMBER_SAME_2(b, c)

#define SAME_SIG(T) inline bool Same(T const& lhs, T const& rhs, SAME_COMMON_ARGS)

#define DEFINE_SAME_0(T)          template <typename... A> inline bool Same(T const&, T const&, A&&...) {   return true; } DEFINE_EQ_NE_DEEP(T)
#define DEFINE_SAME_1(T, a)       SAME_SIG(T) { bool same = DEFINE_MEMBER_SAME_1(a);       VERBOSE_PRINT(); return same; } DEFINE_EQ_NE_DEEP(T)
#define DEFINE_SAME_2(T, a, b)    SAME_SIG(T) { bool same = DEFINE_MEMBER_SAME_2(a, b);    VERBOSE_PRINT(); return same; } DEFINE_EQ_NE_DEEP(T)
#define DEFINE_SAME_3(T, a, b, c) SAME_SIG(T) { bool same = DEFINE_MEMBER_SAME_3(a, b, c); VERBOSE_PRINT(); return same; } DEFINE_EQ_NE_DEEP(T)

#define DEFINE_PVOID_SAME_1(T, a) SAME_SIG(T) { bool same = PVOID_MEMBER_SAME(a);          VERBOSE_PRINT(); return same; } DEFINE_EQ_NE_DEEP(T)
