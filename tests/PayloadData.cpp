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

#include "PrettyPrintersNvtxCpp.h"

#include <nvtx3/nvtx3.hpp>

#include <array>
#include <iostream>
#include <vector>

struct test_payload_domain
{
  static constexpr char const* name{"TestPayloadDomain"};
};

using registered_string = nvtx3::registered_string_in<test_payload_domain>;

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

namespace application_namespace
{
  struct domain
  {
    static constexpr char const* name{"NamespacedDomain"};
  };

  struct MyPayloadStruct
  {
    uint32_t val_uint32;
  };
}

NVTX3_DEFINE_SCHEMA_GET(
    application_namespace::domain,
    application_namespace::MyPayloadStruct,
    "application_namespace::MyPayloadStruct",
    NVTX_PAYLOAD_ENTRIES((val_uint32, TYPE_UINT32, "MyUInt32")))


struct test_message
{
  static constexpr const char* message{"TestMessage"};
};

extern "C" NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv);
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
  NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

  (void)argc;
  (void)argv;

  std::cout << std::boolalpha;

  {
    std::cout << "Test with single payload_data:\n";
    MyPayloadStruct1 pds1 = {123, 456.789f};
    nvtx3::payload_data pd{pds1};
    nvtx3::event_attributes attr{pd};
    std::cout << attr;
  }
  std::cout << "-------------------------------------\n";

  {
    std::cout << "Test with single payload_data (const):\n";
    const MyPayloadStruct1 pds1 = {123, 456.789f};
    const nvtx3::payload_data pd{pds1};
    nvtx3::event_attributes attr{pd};
    std::cout << attr;
  }
  std::cout << "-------------------------------------\n";

  {
    std::cout << "Test with std::vector of payload_data (mixed types):\n";
    MyPayloadStruct1 pds1 = {111, 1.1f};
    MyPayloadStruct2 pds2 = {222LL, registered_string::get<test_message>()};

    std::vector<nvtx3::payload_data> pds_vec;
    pds_vec.emplace_back(pds1);
    pds_vec.emplace_back(pds2);

    nvtx3::event_attributes attr{pds_vec};
    std::cout << attr;
  }
  std::cout << "-------------------------------------\n";

  {
    std::cout << "Test with std::vector of payload_data (mixed types, const):\n";
    const MyPayloadStruct1 pds1 = {111, 1.1f};
    const MyPayloadStruct2 pds2 = {222LL, registered_string::get<test_message>()};

    std::vector<nvtx3::payload_data> pds_vec;
    pds_vec.emplace_back(pds1);
    pds_vec.emplace_back(pds2);

    nvtx3::event_attributes attr{pds_vec};
    std::cout << attr;
  }
  std::cout << "-------------------------------------\n";

  {
    std::cout << "Test with std::array of payload_data (mixed types):\n";
    MyPayloadStruct1 pds1 = {10, 1.0f};
    MyPayloadStruct2 pds2 = {20LL, registered_string::get<test_message>()};

    std::array<nvtx3::payload_data, 2> pds_arr = {
        {nvtx3::payload_data{pds1}, nvtx3::payload_data{pds2}}};

    nvtx3::event_attributes attr{pds_arr};
    std::cout << attr;
  }
  std::cout << "-------------------------------------\n";

  {
    std::cout << "Test with std::array of payload_data (mixed types, const):\n";
    const MyPayloadStruct1 pds1 = {10, 1.0f};
    const MyPayloadStruct2 pds2 = {20LL, registered_string::get<test_message>()};

    const std::array<nvtx3::payload_data, 2> pds_arr = {
        {nvtx3::payload_data{pds1}, nvtx3::payload_data{pds2}}};

    nvtx3::event_attributes attr{pds_arr};
    std::cout << attr;
  }
  std::cout << "-------------------------------------\n";

  {
    std::cout << "Test with single payload_data and other attributes:\n";
    MyPayloadStruct1 pds1 = {777, 88.99f};
    nvtx3::event_attributes attr{
        nvtx3::message{"Payload Test Message"},
        nvtx3::rgb{10, 20, 30},
        nvtx3::category{55},
        nvtx3::payload_data{pds1}};
    std::cout << attr;
  }
  std::cout << "-------------------------------------\n";

  {
    std::cout << "Test with empty std::vector of payload_data:\n";
    std::vector<nvtx3::payload_data> empty_pds_vec;
    nvtx3::event_attributes attr{empty_pds_vec};
    std::cout << attr;
  }
  std::cout << "-------------------------------------\n";

  {
    std::cout << "Test with C-style nvtxPayloadData_t:\n";
    MyPayloadStruct1 pds1 = {777, 88.99f};
    nvtxPayloadData_t cPayloadData = {123, sizeof(pds1), &pds1};
    nvtx3::payload_data pd{cPayloadData};
    nvtx3::event_attributes attr{pd};
    std::cout << attr;
  }

  {
    std::cout << "Test with C-style const nvtxPayloadData_t:\n";
    MyPayloadStruct1 pds1 = {777, 88.99f};
    const nvtxPayloadData_t cPayloadData = {123, sizeof(pds1), &pds1};
    nvtx3::payload_data pd{cPayloadData};
    nvtx3::event_attributes attr{pd};
    std::cout << attr;
  }

  return 0;
}
