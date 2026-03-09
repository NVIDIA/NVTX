/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/*
 * Best-practice tests for NVTX extended payloads using the C++ API.
 *
 * Demonstrates:
 *   - Domain definition as a struct with a static name
 *   - Schema registration via NVTX3_DEFINE_SCHEMA_GET
 *   - nvtx3::payload_data for wrapping payloads
 *   - nvtx3::mark_in, nvtx3::scoped_range_in, nvtx3::start_range_in /
 *     nvtx3::end_range_in for attaching payloads to events
 *   - Multiple payloads per event
 *   - Nested schemas with the C++ API
 */

#include <array>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include <nvtx3/nvtx3.hpp>

struct TestDomain
{
    static constexpr const char* name = "ExtendedPayloadCppDomain";
};

struct SensorPayload
{
    int32_t sensorId;
    float temperature;
    uint8_t channelId;
};
NVTX3_DEFINE_SCHEMA_GET(TestDomain, SensorPayload, "SensorPayload",
                        NVTX_PAYLOAD_ENTRIES((sensorId, TYPE_INT32, "SensorId"),
                                             (temperature, TYPE_FLOAT, "Temperature"),
                                             (channelId, TYPE_UINT8, "ChannelId")))

struct Boundaries
{
    float low;
    double high;
};
NVTX3_DEFINE_SCHEMA_GET(TestDomain, Boundaries, "Boundaries",
                        NVTX_PAYLOAD_ENTRIES((low, TYPE_FLOAT, "Low"), (high, TYPE_DOUBLE, "High")))

struct OuterRecord
{
    int32_t id;
    Boundaries stats;
};

template <>
inline nvtx3::schema const& nvtx3::schema::get<OuterRecord>() noexcept
{
    static const nvtxPayloadSchemaEntry_t entries[] = {
        {0, NVTX_PAYLOAD_ENTRY_TYPE_INT32, "id"},
        {0, nvtx3::schema::get<Boundaries>().get_handle(), "stats"}};

    static const nvtxPayloadSchemaAttr_t attr{
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
            NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
            NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES |
            NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE,
        "OuterRecord",
        NVTX_PAYLOAD_SCHEMA_TYPE_STATIC,
        0,
        entries,
        std::extent_v<decltype(entries)>,
        sizeof(OuterRecord)};

    static const schema s{nvtxPayloadSchemaRegister(nvtx3::domain::get<TestDomain>(), &attr)};
    return s;
}

namespace {

void TestMarkWithPayload()
{
    SensorPayload pl{42, 3.14f, 1};
    nvtx3::mark_in<TestDomain>(nvtx3::payload_data{pl});
}

void TestScopedRangeWithPayload()
{
    SensorPayload pl{7, 98.6f, 3};
    {
        nvtx3::scoped_range_in<TestDomain> range{"ScopedRangeTest", nvtx3::payload_data{pl}};
    }
}

void TestStartEndRangeWithPayload()
{
    SensorPayload pl{99, 2.718f, 2};
    nvtx3::range_handle h =
        nvtx3::start_range_in<TestDomain>("StartEndTest", nvtx3::payload_data{pl});
    nvtx3::end_range_in<TestDomain>(h);
}

void TestMultiplePayloads()
{
    SensorPayload sensor{5, 72.5f, 1};
    Boundaries bounds{0.5f, 100.0};
    std::array<nvtx3::payload_data, 2> payloads{
        {nvtx3::payload_data{sensor}, nvtx3::payload_data{bounds}}};
    nvtx3::mark_in<TestDomain>(payloads);
}

void TestNestedSchema()
{
    OuterRecord rec{1, {1.25f, 9.5}};
    nvtx3::mark_in<TestDomain>(nvtx3::payload_data{rec});
}

} // namespace

int main()
{
#define RUN_TEST(fn)                                                                               \
    do                                                                                             \
    {                                                                                              \
        fflush(stdout);                                                                            \
        printf("\n--- " #fn " ---\n");                                                             \
        fflush(stdout);                                                                            \
        fn();                                                                                      \
    } while (0)

    RUN_TEST(TestMarkWithPayload);
    RUN_TEST(TestScopedRangeWithPayload);
    RUN_TEST(TestStartEndRangeWithPayload);
    RUN_TEST(TestMultiplePayloads);
    RUN_TEST(TestNestedSchema);

#undef RUN_TEST

    return 0;
}
