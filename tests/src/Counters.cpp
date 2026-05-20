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
#include <nvtx3/nvToolsExtSemanticsCounters.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static int g_failures = 0;

static void check_u64(char const* label, char const* expr,
                      uint64_t actual, uint64_t expected)
{
    if (actual != expected) {
        ++g_failures;
        std::cout << "  FAIL [" << label << "] " << expr
                  << " = 0x" << std::hex << actual
                  << " (expected 0x" << expected << ")" << std::dec << "\n";
    }
}

static void check_i64(char const* label, char const* expr,
                      int64_t actual, int64_t expected)
{
    if (actual != expected) {
        ++g_failures;
        std::cout << "  FAIL [" << label << "] " << expr
                  << " = " << actual
                  << " (expected " << expected << ")\n";
    }
}

static void check_f64(char const* label, char const* expr,
                      double actual, double expected)
{
    if (actual != expected) {
        ++g_failures;
        std::cout << "  FAIL [" << label << "] " << expr
                  << " = " << actual
                  << " (expected " << expected << ")\n";
    }
}

static void check_str(char const* label, char const* expr,
                      char const* actual, char const* expected)
{
    bool const eq = (actual == expected)
        || (actual && expected && std::strcmp(actual, expected) == 0);
    if (!eq) {
        ++g_failures;
        std::cout << "  FAIL [" << label << "] " << expr
                  << " = \"" << (actual ? actual : "(null)")
                  << "\" (expected \"" << (expected ? expected : "(null)") << "\")\n";
    }
}

#define CHECK_U64(LABEL, ACTUAL, EXPECTED) check_u64((LABEL), #ACTUAL, static_cast<uint64_t>(ACTUAL), static_cast<uint64_t>(EXPECTED))
#define CHECK_I64(LABEL, ACTUAL, EXPECTED) check_i64((LABEL), #ACTUAL, static_cast<int64_t>(ACTUAL),  static_cast<int64_t>(EXPECTED))
#define CHECK_F64(LABEL, ACTUAL, EXPECTED) check_f64((LABEL), #ACTUAL, static_cast<double>(ACTUAL),   static_cast<double>(EXPECTED))
#define CHECK_STR(LABEL, ACTUAL, EXPECTED) check_str((LABEL), #ACTUAL, (ACTUAL), (EXPECTED))

static nvtxSemanticsCounter_v1 const&
as_counter(nvtx3::counter_semantic const& sem)
{
    return *reinterpret_cast<nvtxSemanticsCounter_v1 const*>(sem.get());
}

static void dump_counter_semantic(char const* label, nvtx3::counter_semantic const& sem)
{
    auto const& c = as_counter(sem);
    std::cout << "  [" << label << "]"
              << " semanticId=" << c.header.semanticId
              << " ver=" << c.header.version
              << " flags=0x" << std::hex << c.flags << std::dec
              << " limitType=" << c.limitType
              << " unit=" << (c.unit ? c.unit : "(null)")
              << " scaleNum=" << c.unitScaleNumerator
              << " scaleDen=" << c.unitScaleDenominator;
    switch (c.limitType) {
        case NVTX_COUNTER_LIMIT_I64:
            std::cout << " min.i64=" << c.min.i64 << " max.i64=" << c.max.i64;
            break;
        case NVTX_COUNTER_LIMIT_U64:
            std::cout << " min.u64=" << c.min.u64 << " max.u64=" << c.max.u64;
            break;
        case NVTX_COUNTER_LIMIT_F64:
            std::cout << " min.f64=" << c.min.f64 << " max.f64=" << c.max.f64;
            break;
        default:
            break;
    }
    std::cout << " next=" << (c.header.next ? "yes" : "no") << "\n";
}

struct counters_domain
{
    static constexpr char const* name{"CountersDomain"};
};

struct sensor_sample
{
    float temperature;
    uint32_t channel_id;
};

struct timestamped_sensor_sample
{
    int64_t timestamp;
    float temperature;
    uint32_t channel_id;
};

/*
 * Compile-verified mirror of the NVTX3_SEMANTIC docstring example.
 * If you change one, update the other.
 */
struct sensor_data
{
    float temperature;
    float pressure;
};

NVTX3_DEFINE_SCHEMA_GET(
    counters_domain,
    sensor_data,
    "SensorData",
    NVTX_PAYLOAD_ENTRIES(
        (temperature, TYPE_FLOAT, "Temperature", nullptr, 0, UNUSED,
            NVTX3_SEMANTIC(nvtx3::counter_semantic{}.unit("C").limits(-40.0, 85.0))),
        (pressure, TYPE_FLOAT, "Pressure", nullptr, 0, UNUSED,
            NVTX3_SEMANTIC(nvtx3::counter_semantic{}.unit("hPa")))))

NVTX3_DEFINE_SCHEMA_GET(
    counters_domain,
    sensor_sample,
    "SensorSample",
    NVTX_PAYLOAD_ENTRIES(
        (temperature, TYPE_FLOAT, "Temperature", nullptr, 0, UNUSED,
            NVTX3_SEMANTIC(nvtx3::counter_semantic{}.unit("C").limits(-40.0, 85.0))),
        (channel_id, TYPE_UINT32, "ChannelID")))

NVTX3_DEFINE_SCHEMA_GET(
    counters_domain,
    timestamped_sensor_sample,
    "TimestampedSensorSample",
    NVTX_PAYLOAD_ENTRIES(
        (timestamp, TYPE_INT64, "Timestamp", nullptr, 0, TIMESTAMP,
            NVTX3_SEMANTIC(nvtx3::time_semantic{}.time_domain(NVTX_TIMESTAMP_TYPE_CPU_TSC))),
        (temperature, TYPE_FLOAT, "Temperature", nullptr, 0, UNUSED,
            NVTX3_SEMANTIC(nvtx3::counter_semantic{}.unit("C").limits(-40.0, 85.0))),
        (channel_id, TYPE_UINT32, "ChannelID")))

extern "C" NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv);
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    g_failures = 0;

    (void)argc;
    (void)argv;

    {
        std::cout << "Default-constructed primitive counter (int64):\n";
        nvtx3::counter_in<int64_t, counters_domain> iterations{"iterations"};
        iterations.sample(1);
        iterations.sample(42);
        int64_t batch_samples[] = {3, 5, 8};
        int64_t batch_timestamps[] = {100, 200, 300};
        iterations.submit_batch(batch_samples, 3, batch_timestamps, 3,
                                NVTX_BATCH_FLAG_TIME_SORTED_PER_SCOPE);
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
        std::cout << "Counter with std::string name:\n";
        std::string name{"string_named_counter"};
        nvtx3::counter_in<int64_t, counters_domain> string_named{name};
        string_named.sample(7);
        std::cout << "  counter id: " << string_named.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Counter with std::string name and description:\n";
        std::string name{"string_described_counter"};
        std::string description{"Counter constructed from std::string arguments"};
        nvtx3::counter_in<int64_t, counters_domain> string_described{
            name, description, nvtx3::scope::current_sw_thread()};
        string_described.sample(11);
        std::cout << "  counter id: " << string_described.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Counter with full semantics (int64) and sample_no_value reasons:\n";
        nvtx3::counter_semantic sem;
        std::string unit{"bytes"};
        sem.unit(unit)
            .unit_scale(1024)
            .limits(int64_t{0}, int64_t{1} << 20)
            .interpolation_linear()
            .valuetype_absolute()
            .normalize();

        dump_counter_semantic("heap_size", sem);
        auto const& c = as_counter(sem);
        CHECK_U64("heap_size", c.flags,
                  NVTX_COUNTER_FLAG_NORMALIZE
                | NVTX_COUNTER_FLAG_LIMITS
                | NVTX_COUNTER_FLAG_VALUETYPE_ABSOLUTE
                | NVTX_COUNTER_FLAG_INTERPOLATION_LINEAR);
        CHECK_I64("heap_size", c.limitType, NVTX_COUNTER_LIMIT_I64);
        CHECK_I64("heap_size", c.min.i64, 0);
        CHECK_I64("heap_size", c.max.i64, int64_t{1} << 20);
        CHECK_STR("heap_size", c.unit, "bytes");
        CHECK_U64("heap_size", c.unitScaleNumerator, 1024);
        CHECK_U64("heap_size", c.unitScaleDenominator, 1);
        CHECK_U64("heap_size", c.header.next != nullptr, 0);

        nvtx3::counter_in<int64_t, counters_domain> heap_size{
            std::string{"heap_size"},
            std::string{"Process heap size"},
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

        dump_counter_semantic("gpu_utilization", sem);
        auto const& c = as_counter(sem);
        CHECK_U64("gpu_utilization", c.flags,
                  NVTX_COUNTER_FLAG_LIMITS
                | NVTX_COUNTER_FLAG_INTERPOLATION_SINCE_LAST);
        CHECK_I64("gpu_utilization", c.limitType, NVTX_COUNTER_LIMIT_F64);
        CHECK_F64("gpu_utilization", c.min.f64, 0.0);
        CHECK_F64("gpu_utilization", c.max.f64, 100.0);
        CHECK_STR("gpu_utilization", c.unit, "%");
        CHECK_U64("gpu_utilization", c.unitScaleNumerator, 1);
        CHECK_U64("gpu_utilization", c.unitScaleDenominator, 1);
        CHECK_U64("gpu_utilization", c.header.next != nullptr, 0);

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

        dump_counter_semantic("file_offset", sem);
        auto const& c = as_counter(sem);
        CHECK_U64("file_offset", c.flags, NVTX_COUNTER_FLAG_LIMITS);
        CHECK_I64("file_offset", c.limitType, NVTX_COUNTER_LIMIT_U64);
        CHECK_U64("file_offset", c.min.u64, 0u);
        CHECK_U64("file_offset", c.max.u64, uint64_t{1ull} << 40);
        CHECK_STR("file_offset", c.unit, nullptr);
        CHECK_U64("file_offset", c.unitScaleNumerator, 1);
        CHECK_U64("file_offset", c.unitScaleDenominator, 1);
        CHECK_U64("file_offset", c.header.next != nullptr, 0);

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

        dump_counter_semantic("sensor", sem);
        auto const& c = as_counter(sem);
        CHECK_U64("sensor", c.flags,
                  NVTX_COUNTER_FLAG_INTERPOLATION_SINCE_LAST
                | NVTX_COUNTER_FLAG_VALUETYPE_ABSOLUTE);
        CHECK_I64("sensor", c.limitType, NVTX_COUNTER_LIMIT_UNDEFINED);
        CHECK_STR("sensor", c.unit, nullptr);
        CHECK_U64("sensor", c.unitScaleNumerator, 1);
        CHECK_U64("sensor", c.unitScaleDenominator, 1);
        CHECK_U64("sensor", c.header.next != nullptr, 0);

        nvtx3::counter_in<sensor_sample, counters_domain> sensor{
            "sensor",
            "Example payload counter",
            nvtx3::scope::current_sw_thread(),
            sem};
        sensor.sample({21.5f, 0u});
        sensor.sample({23.0f, 1u});
        sensor.sample({19.25f, 0u});
        std::array<sensor_sample, 2> batch_samples{{{24.0f, 1u}, {25.5f, 2u}}};
        std::array<int64_t, 4> batch_interval{{400, 50, 500, 50}};
        sensor.submit_batch(batch_samples, batch_interval,
                            NVTX_COUNTER_BATCH_FLAG_BEGINTIME_INTERVAL_PAIR);
        std::cout << "  counter id: " << sensor.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Payload-struct counter batch with embedded timestamps:\n";
        nvtx3::counter_in<timestamped_sensor_sample, counters_domain> sensor{
            "timestamped_sensor",
            "Example payload counter with embedded timestamps"};
        std::vector<timestamped_sensor_sample> batch_samples{
            {1000, 21.5f, 0u},
            {2000, 23.0f, 1u},
            {3000, 19.25f, 0u}};
        sensor.submit_batch(batch_samples);
        std::cout << "  counter id: " << sensor.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Conflicting enum-group setters throw std::logic_error:\n";

        {
            nvtx3::counter_semantic sem;
            sem.valuetype_absolute();
            bool threw = false;
            try {
                sem.valuetype_delta();
            } catch (std::logic_error const& e) {
                threw = true;
                std::cout << "  caught (valuetype): " << e.what() << "\n";
            }
            CHECK_U64("conflict_valuetype", threw, 1);
            auto const& c = as_counter(sem);
            CHECK_U64("conflict_valuetype",
                      c.flags & NVTX_COUNTER_FLAG_VALUETYPES,
                      NVTX_COUNTER_FLAG_VALUETYPE_ABSOLUTE);
        }

        {
            nvtx3::counter_semantic sem;
            sem.interpolation_linear();
            bool threw = false;
            try {
                sem.interpolation_point();
            } catch (std::logic_error const& e) {
                threw = true;
                std::cout << "  caught (interpolation): " << e.what() << "\n";
            }
            CHECK_U64("conflict_interpolation", threw, 1);
            auto const& c = as_counter(sem);
            CHECK_U64("conflict_interpolation",
                      c.flags & NVTX_COUNTER_FLAG_INTERPOLATIONS,
                      NVTX_COUNTER_FLAG_INTERPOLATION_LINEAR);
        }

        {
            nvtx3::counter_semantic sem;
            sem.valuetype_absolute().interpolation_linear();
            auto const& c = as_counter(sem);
            CHECK_U64("cross_group_ok", c.flags,
                      NVTX_COUNTER_FLAG_VALUETYPE_ABSOLUTE
                    | NVTX_COUNTER_FLAG_INTERPOLATION_LINEAR);
        }
    }
    std::cout << "-------------------------------------\n";

    {
        std::cout << "Doc-example schema (NVTX3_SEMANTIC with mixed builders):\n";
        nvtx3::counter_in<sensor_data, counters_domain> probe{
            "env_probe",
            "Per-field semantic mix from the NVTX3_SEMANTIC docstring"};
        probe.sample({21.5f, 1013.25f});
        probe.sample({22.0f, 1012.80f});
        std::cout << "  counter id: " << probe.id() << "\n";
    }
    std::cout << "-------------------------------------\n";

    if (g_failures != 0) {
        std::cout << "FAILED: " << g_failures << " counter_semantic check(s) mismatched expected values\n";
    }
    return g_failures;
}
