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

#include <nvtx3/nvtx3.hpp>

#include <cstdint>
#include <iostream>

struct counters_domain
{
    static constexpr char const* name{"CountersDomain"};
};

struct sensor_sample
{
    float temperature;
    uint32_t channel_id;
};

NVTX3_DEFINE_SCHEMA_GET(
    counters_domain,
    sensor_sample,
    "SensorSample",
    NVTX_PAYLOAD_ENTRIES(
        (temperature, TYPE_FLOAT, "Temperature", nullptr, 0, UNUSED,
            NVTX3_SEMANTIC(nvtx3::counter_semantic{}.unit("C").limits(-40.0, 85.0))),
        (channel_id, TYPE_UINT32, "ChannelID")))

extern "C" NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv);
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    (void)argc;
    (void)argv;

    {
        std::cout << "Default-constructed primitive counter (int64):\n";
        nvtx3::counter_in<int64_t, counters_domain> iterations{"iterations"};
        iterations.sample(1);
        iterations.sample(42);
        std::cout << "  counter id: " << iterations.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Counter with description and explicit scope:\n";
        nvtx3::counter_in<int64_t, counters_domain> bytes_in{
            "bytes_in", "Bytes received", nvtx3::scope::current_sw_thread()};
        bytes_in.sample(512);
        bytes_in.sample(1024);
        std::cout << "  counter id: " << bytes_in.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Counter with full semantics (int64) and sample_no_value reasons:\n";
        nvtx3::counter_semantic sem;
        sem.unit("bytes")
            .unit_scale(1024)
            .limits(int64_t{0}, int64_t{1} << 20)
            .interpolation_linear()
            .valuetype_absolute()
            .normalize();

        nvtx3::counter_in<int64_t, counters_domain> heap_size{
            "heap_size",
            "Process heap size",
            nvtx3::scope::current_sw_process(),
            sem};
        heap_size.sample(256);
        heap_size.sample(4096);
        heap_size.sample_no_value(nvtx3::no_value_reason::zero);
        heap_size.sample_no_value(nvtx3::no_value_reason::unchanged);
        heap_size.sample_no_value(nvtx3::no_value_reason::unavailable);
        std::cout << "  counter id: " << heap_size.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Floating-point counter with percent unit and interpolation:\n";
        nvtx3::counter_semantic sem;
        sem.unit("%").limits(0.0, 100.0).interpolation_since_last();

        nvtx3::counter_in<double, counters_domain> utilization{
            "gpu_utilization",
            "GPU utilization percentage",
            nvtx3::scope::current_sw_process(),
            sem};
        utilization.sample(12.5);
        utilization.sample(87.75);
        std::cout << "  counter id: " << utilization.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Unsigned counter with separate limit_min / limit_max setters:\n";
        nvtx3::counter_semantic sem;
        sem.limit_min(uint64_t{0}).limit_max(uint64_t{1ull} << 40);

        nvtx3::counter_in<uint64_t, counters_domain> offset{
            "file_offset",
            "File offset in bytes",
            nvtx3::scope::none(),
            sem};
        offset.sample(0u);
        offset.sample(1u << 20);
        std::cout << "  counter id: " << offset.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Payload-struct counter with per-field NVTX3_SEMANTIC:\n";
        nvtx3::counter_semantic sem;
        sem.interpolation_since_last().valuetype_absolute();

        nvtx3::counter_in<sensor_sample, counters_domain> sensor{
            "sensor",
            "Example payload counter",
            nvtx3::scope::current_sw_thread(),
            sem};
        sensor.sample({21.5f, 0u});
        sensor.sample({23.0f, 1u});
        sensor.sample({19.25f, 0u});
        std::cout << "  counter id: " << sensor.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    return 0;
}
