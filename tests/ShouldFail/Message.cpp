/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <string>

struct test_message_domain
{
    static constexpr char const* name{"TestMessageDomain"};
};

using registered_string = nvtx3::registered_string_in<test_message_domain>;
using scoped_range = nvtx3::scoped_range_in<test_message_domain>;

extern void dummy();
void dummy()
{
    // Just make sure the basics work
    {
        std::string msg = "Test Message";
        nvtx3::message m{msg};
        nvtx3::event_attributes attr{m};
        scoped_range r1{attr};
        (void)nvtx3::start_range_in<test_message_domain>(attr);
    }
    {
        std::wstring msg = L"Test Message";
        nvtx3::message m{msg};
        nvtx3::event_attributes attr{m};
        scoped_range r1{attr};
        (void)nvtx3::start_range_in<test_message_domain>(attr);
    }

    // Test cases that should fail to compile
    {
#ifdef EVENT_ATTRIBUTES_FROM_MESSAGE_FROM_TEMPORARY_STRING
        nvtx3::event_attributes attr{nvtx3::message{std::string{"foo"}}};
#endif
    }
    {
#ifdef START_RANGE_IN_FROM_MESSAGE_FROM_TEMPORARY_STRING
        nvtx3::event_attributes attr{nvtx3::message{std::string{"foo"}}};
        (void)nvtx3::start_range_in<test_message_domain>(attr);
#endif
    }
    {
#ifdef EVENT_ATTRIBUTES_FROM_MESSAGE_FROM_TEMPORARY_WSTRING
        nvtx3::event_attributes attr{nvtx3::message{std::wstring{L"foo"}}};
#endif
    }
    {
#ifdef START_RANGE_IN_FROM_MESSAGE_FROM_TEMPORARY_WSTRING
        nvtx3::event_attributes attr{nvtx3::message{std::wstring{L"foo"}}};
        (void)nvtx3::start_range_in<test_message_domain>(attr);
#endif
    }
}
