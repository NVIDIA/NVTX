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

// Do NOT REMOVE even if you IDE tells you it is not used, we need this conditionally.
#include <array>

struct test_payload_domain
{
    static constexpr char const* name{"TestPayloadDomain"};
};

using registered_string = nvtx3::registered_string_in<test_payload_domain>;
using scoped_range = nvtx3::scoped_range_in<test_payload_domain>;

struct MyPayloadStruct1
{
    MyPayloadStruct1() = delete;
    MyPayloadStruct1(uint32_t u, float f) : val_uint32{u}, val_float{f} {}
    uint32_t val_uint32;
    float val_float;
};

NVTX3_DEFINE_SCHEMA_GET(
    test_payload_domain,
    MyPayloadStruct1,
    "MyPayloadStruct1_schema",
    NVTX_PAYLOAD_ENTRIES((val_uint32, TYPE_UINT32, "MyUInt32"), (val_float, TYPE_FLOAT, "MyFloat")))

struct MyPayloadStruct2
{
    MyPayloadStruct2() = delete;
    MyPayloadStruct2(int64_t i, registered_string r) : val_int64{i}, val_reg_str{r} {}
    int64_t val_int64;
    registered_string val_reg_str; // you can use registered_string in place of a handle
};

NVTX3_DEFINE_SCHEMA_GET(
    test_payload_domain,
    MyPayloadStruct2,
    "MyPayloadStruct2_schema",
    NVTX_PAYLOAD_ENTRIES(
        (val_int64, TYPE_INT64, "MyInt64"),
        (val_reg_str, TYPE_NVTX_REGISTERED_STRING_HANDLE, "MyRegisteredString")))

struct test_message
{
    static constexpr const char* message{"TestMessage"};
};

extern void dummy();
void dummy()
{
    // Just make sure the basics work
    {
        MyPayloadStruct1 pds1 = {123, 456.789f};
        nvtx3::payload_data pd{pds1};
        nvtx3::event_attributes attr{pd};
        scoped_range r1{attr};
        (void)nvtx3::start_range_in<test_payload_domain>(attr);
    }

    // Typically the bad cases consist of two statements:
    // The first statement creates an object that wraps a teporary
    // The second statement uses the first object (now an lvalue with a dangling reference)
    // We want either of the statements to not compile, because both together
    // are invalid.
    {
#ifdef EVENT_ATTRIBUTES_FROM_PAYLOAD_DATA_FROM_TEMPORARY
        nvtx3::payload_data pd{MyPayloadStruct1{123, 456.789f}};
        nvtx3::event_attributes attr{pd};
#endif
    }
    {
        MyPayloadStruct1 pds1 = {123, 456.789f};
#ifdef SCOPED_RANGE_FROM_EVENT_ATTRIBUTES_FROM_TEMPORARY_PAYLOAD_DATA
        nvtx3::event_attributes attr{nvtx3::payload_data{pds1}};
        scoped_range r1{attr};
#else
        (void)pds1;
#endif
    }
    {
        MyPayloadStruct1 pds1 = {123, 456.789f};
#ifdef START_RANGE_IN_FROM_EVENT_ATTRIBUTES_FROM_TEMPORARY_PAYLOAD_DATA
        nvtx3::event_attributes attr{nvtx3::payload_data{pds1}};
        (void)nvtx3::start_range_in<test_payload_domain>(attr);
#else
        (void)pds1;
#endif
    }
    {
        MyPayloadStruct1 pds1 = {123, 456.789f};
#ifdef START_RANGE_FROM_EVENT_ATTRIBUTES_FROM_TEMPORARY_PAYLOAD_DATA
        nvtx3::event_attributes attr{nvtx3::payload_data{pds1}};
        (void)nvtx3::start_range(attr);
#else
        (void)pds1;
#endif
    }
    {
        MyPayloadStruct1 pds1 = {123, 456.789f};
        MyPayloadStruct1 pds2 = {234, 567.890f};
#ifdef SCOPE_FRANGE_FROM_EVENT_ATTRIBUTES_FROM_TEMPORARY_ARRAY_OF_PAYLOAD_DATA
        nvtx3::event_attributes attr{
            std::array<nvtx3::payload_data, 2>{nvtx3::payload_data{pds1}, nvtx3::payload_data{pds2}}};
        scoped_range r1{attr};
#else
        (void)pds1;
        (void)pds2;
#endif
    }
    {
        MyPayloadStruct1 pds1 = {123, 456.789f};
#ifdef START_RANGE_IN_FROM_PAYLOAD_DATA_FROM_TEMPORARY
        nvtx3::payload_data pd{MyPayloadStruct1{123, 456.789f}};
        (void)nvtx3::start_range_in<test_payload_domain>(attr{pd});
#else
        (void)pds1;
#endif
    }
    {
        MyPayloadStruct1 pds1 = {123, 456.789f};
#ifdef START_RANGE_FROM_PAYLOAD_DATA_FROM_TEMPORARY
        nvtx3::payload_data pd{MyPayloadStruct1{123, 456.789f}};
        (void)nvtx3::start_range(nvtx3::event_attributes{pd});
#else
        (void)pds1;
#endif
    }
    {
        MyPayloadStruct1 pds1 = {123, 456.789f};
#ifdef EVENT_ATTRIBUTES_FROM_EVENT_ATTRIBUTES_FROM_TEMPORARY_PAYLOAD_DATA
        nvtx3::event_attributes attr{nvtx3::payload_data{pds1}};
        nvtx3::event_attributes attr_copy{attr};
#else
        (void)pds1;
#endif
    }
    {
        MyPayloadStruct1 pds1 = {123, 456.789f};
        nvtx3::event_attributes attr2;
#ifdef EVENT_ATTRIBUTES_COPY_EVENT_ATTRIBUTES_FROM_TEMPORARY_PAYLOAD_DATA
        nvtx3::event_attributes attr{nvtx3::payload_data{pds1}};
        attr2 = attr;
#else
        (void)pds1;
        (void)attr2;
#endif
    }
}
