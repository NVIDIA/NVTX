/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <deque>
#include <nvtx3/nvToolsExt.h>
#include <nvtx3/nvToolsExtPayload.h>
#include <nvtx3/nvToolsExtCounters.h>
#include <nvtx3/nvToolsExtSemanticsCounters.h>
#include <nvtx3/nvToolsExtSemanticsTime.h>
#include <Python.h>

#ifdef _WIN32
#define EXPORT_SYMBOL   __declspec(dllexport)
#else
#define EXPORT_SYMBOL __attribute__((visibility("default")))
#endif

namespace
{
constexpr char DefaultDomain[] = "Default";
constexpr size_t EventBufferSize = 256;
constexpr size_t SchemaEntryBufferCount = 16;
constexpr size_t TimestampBufferCount = 16;

enum class EventKind : uint32_t
{
    DomainCreate = 1,
    DomainRegisterString = 2,
    DomainNameCategory = 3,
    Mark = 4,
    RangePush = 5,
    RangePop = 6,
    RangeStart = 7,
    RangeEnd = 8,
    PayloadSchemaRegister = 9,
    CounterRegister = 10,
    CounterSample = 11,
    CounterSampleInt64 = 12,
    CounterSampleFloat64 = 13,
    CounterSampleNoValue = 14,
    CounterBatchSubmit = 15,
    TimestampGet = 16,
    ScopeRegister = 17,
};

struct AttributeRecord
{
    int32_t message_type{NVTX_MESSAGE_UNKNOWN};
    const char* message{nullptr};
    uint32_t category{0};
    uint64_t range_id{0};
    uint32_t color{0};
};

struct PayloadRecord
{
    int32_t type{0};
    int64_t i64{0};
    double f64{0.0};
    uint64_t ext_schema_id{0};
    uint64_t ext_size{0};
    uint64_t schema_type{0};
    uint64_t schema_flags{0};
    uint64_t schema_num_entries{0};
    uint64_t schema_static_size{0};
    uint64_t schema_entry_types[SchemaEntryBufferCount]{};
    uint8_t ext_data[EventBufferSize]{};
};

struct CounterSemanticsRecord
{
    uint8_t has_semantics{0};
    uint8_t has_time_semantics{0};
    uint64_t time_domain_id{0};
    uint64_t flags{0};
    char unit[EventBufferSize]{};
    uint64_t unit_scale_numerator{0};
    uint64_t unit_scale_denominator{0};
    int64_t limit_type{0};
    int64_t min_i64{0};
    uint64_t min_u64{0};
    double min_f64{0.0};
    int64_t max_i64{0};
    uint64_t max_u64{0};
    double max_f64{0.0};
};

struct CounterRegistrationRecord
{
    uint64_t schema_id{0};
    char name[EventBufferSize]{};
    char description[EventBufferSize]{};
    uint64_t scope_id{0};
    CounterSemanticsRecord semantics;
};

struct CounterSampleRecord
{
    int64_t i64{0};
    double f64{0.0};
    uint8_t no_value_reason{0};
    uint64_t data_size{0};
    uint8_t data[EventBufferSize]{};
    uint64_t timestamps_size{0};
    int64_t timestamps[TimestampBufferCount]{};
};

struct CounterRecord
{
    uint64_t id{0};
    CounterRegistrationRecord registration;
    CounterSampleRecord sample;
};

struct TimestampRecord
{
    int64_t value{0};
};

struct ScopeRecord
{
    uint64_t id{0};
    char path[EventBufferSize]{};
};

struct EventRecord
{
    uint32_t kind{0};
    const char* domain{nullptr};
    AttributeRecord attributes;
    PayloadRecord payload;
    CounterRecord counter;
    TimestampRecord timestamp;
    ScopeRecord scope;
};

// Queue of the recorded events. Consumed by `read_event()` (called by Python tests).
std::deque<EventRecord> g_events;

// Atomic counter for the range IDs.
std::atomic<uint64_t> g_rangeId{1};

// Registered domains. nvtxDomainHandle_t is the index in the deque
// (1-based because 0 is reserved for the default domain)
std::deque<std::string> g_registeredDomains;

// Registered strings.
std::deque<std::string> g_registeredStrings;

// Atomic counter for assigning unique schema IDs (starting at 1).
std::atomic<uint64_t> g_schemaId{1};

// Atomic counter for assigning unique counter IDs (starting in the dynamic ID range).
std::atomic<uint64_t> g_counterId{NVTX_COUNTER_ID_DYNAMIC_START};

// Atomic counter for assigning unique scope IDs (starting in the dynamic ID range).
std::atomic<uint64_t> g_scopeId{NVTX_SCOPE_ID_DYNAMIC_START};

// Monotonic counter returned by intercepted nvtxTimestampGet calls.
std::atomic<int64_t> g_nextTimestamp{1000000};

bool CopyCString(char (&dst)[EventBufferSize], const char* src, std::string fieldName)
{
    size_t length = std::strlen(src);
    if (length >= EventBufferSize)
    {
        std::string error_message = fieldName +
            " exceeds fixed test buffer size " + std::to_string(EventBufferSize);
        PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
        return false;
    }
    std::memcpy(dst, src, length + 1);
    return true;
}

bool ValidateCounterSemanticsFlags(const nvtxSemanticsCounter_t* semantics)
{
    constexpr uint64_t CounterValueTypeMask = NVTX_COUNTER_FLAG_VALUETYPE_DELTA_SINCE_START;
    constexpr uint64_t CounterInterpolationMask =
        NVTX_COUNTER_FLAG_INTERPOLATION_POINT |
        NVTX_COUNTER_FLAG_INTERPOLATION_SINCE_LAST |
        NVTX_COUNTER_FLAG_INTERPOLATION_UNTIL_NEXT |
        NVTX_COUNTER_FLAG_INTERPOLATION_LINEAR;
    constexpr uint64_t SupportedCounterFlags =
        NVTX_COUNTER_FLAG_NORMALIZE |
        NVTX_COUNTER_FLAG_LIMITS |
        CounterValueTypeMask |
        CounterInterpolationMask;

    uint64_t flags = semantics->flags;
    if ((flags & ~SupportedCounterFlags) != 0)
    {
        std::string error_message = "Unsupported counter semantics flags: " +
            std::to_string(flags);
        PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
        return false;
    }

    uint64_t valueType = flags & CounterValueTypeMask;
    switch (valueType)
    {
        case NVTX_COUNTER_FLAG_VALUETYPE_ABSOLUTE:
        case NVTX_COUNTER_FLAG_VALUETYPE_DELTA:
        case NVTX_COUNTER_FLAG_VALUETYPE_DELTA_SINCE_START:
            break;
        default:
        {
            std::string error_message = "Invalid counter value type flags: " +
                std::to_string(valueType);
            PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
            return false;
        }
    }

    uint64_t interpolation = flags & CounterInterpolationMask;
    switch (interpolation)
    {
        case NVTX_COUNTER_FLAG_INTERPOLATION_POINT:
        case NVTX_COUNTER_FLAG_INTERPOLATION_SINCE_LAST:
        case NVTX_COUNTER_FLAG_INTERPOLATION_UNTIL_NEXT:
        case NVTX_COUNTER_FLAG_INTERPOLATION_LINEAR:
            break;
        default:
        {
            std::string error_message = "Invalid counter interpolation flags: " +
                std::to_string(interpolation);
            PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
            return false;
        }
    }

    if ((flags & NVTX_COUNTER_FLAG_LIMITS) != 0)
    {
        switch (semantics->limitType)
        {
            case NVTX_COUNTER_LIMIT_I64:
            case NVTX_COUNTER_LIMIT_U64:
            case NVTX_COUNTER_LIMIT_F64:
                break;
            default:
            {
                std::string error_message = "Counter limit flags require a valid limit type: " +
                    std::to_string(semantics->limitType);
                PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
                return false;
            }
        }
    }
    return true;
}

uint8_t DomainIsEnabled(nvtxDomainHandle_t /*domain*/)
{
    return 1;
}

const char* ResolveDomain(nvtxDomainHandle_t domain)
{
    if (domain == nullptr)
    {
        return DefaultDomain;
    }

    // nvtxDomainHandle_t is a 1-based index, because 0 is reserved for the default domain
    size_t domainIndex = reinterpret_cast<size_t>(domain) - 1;
    if (g_registeredDomains.size() <= domainIndex)
    {
        std::string error_message = "Domain index " + std::to_string(domainIndex) +
            " is out of bounds. g_registered_domains.size(): " +
            std::to_string(g_registeredDomains.size());
        PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
        return nullptr;
    }
    return g_registeredDomains[domainIndex].c_str();
}

uint64_t PayloadSchemaRegister(nvtxDomainHandle_t domain,
    const nvtxPayloadSchemaAttr_t* attr)
{
    uint64_t schemaId = g_schemaId.fetch_add(1, std::memory_order_relaxed);
    if (attr == nullptr)
    {
        return schemaId;
    }
    EventRecord record{};
    record.kind = static_cast<uint32_t>(EventKind::PayloadSchemaRegister);
    record.domain = ResolveDomain(domain);
    record.payload.ext_schema_id = schemaId;
    record.payload.schema_type =
        (attr->fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE) ? attr->type : 0;
    record.payload.schema_flags =
        (attr->fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_FLAGS) ? attr->flags : 0;
    record.payload.schema_num_entries =
        (attr->fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES) ? attr->numEntries : 0;
    record.payload.schema_static_size =
        (attr->fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE)
            ? attr->payloadStaticSize
            : 0;
    if (attr->fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES)
    {
        if (attr->entries == nullptr && attr->numEntries > 0)
        {
            PyErr_SetString(PyExc_RuntimeError, "Invalid payload schema entries");
            return 0;
        }
        if (attr->numEntries > SchemaEntryBufferCount)
        {
            std::string error_message = "Payload schema has " +
                std::to_string(attr->numEntries) +
                " entries, but the test event buffer only stores " +
                std::to_string(SchemaEntryBufferCount) + ".";
            PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
            return 0;
        }
        for (size_t i = 0; i < attr->numEntries; ++i)
        {
            record.payload.schema_entry_types[i] = attr->entries[i].type;
        }
    }
    g_events.push_back(record);
    return schemaId;
}

bool RecordCounterSemantics(
    CounterSemanticsRecord& record, const nvtxSemanticsCounter_t* semantics)
{
    record.has_semantics = 1;
    if (!ValidateCounterSemanticsFlags(semantics))
    {
        return false;
    }
    record.flags = semantics->flags;
    if (semantics->unit != nullptr &&
        !CopyCString(record.unit, semantics->unit, "Counter semantics unit"))
    {
        return false;
    }
    record.unit_scale_numerator = semantics->unitScaleNumerator;
    record.unit_scale_denominator = semantics->unitScaleDenominator;
    record.limit_type = semantics->limitType;

    switch (semantics->limitType)
    {
        case NVTX_COUNTER_LIMIT_I64:
            record.min_i64 = semantics->min.i64;
            record.max_i64 = semantics->max.i64;
            break;
        case NVTX_COUNTER_LIMIT_U64:
            record.min_u64 = semantics->min.u64;
            record.max_u64 = semantics->max.u64;
            break;
        case NVTX_COUNTER_LIMIT_F64:
            record.min_f64 = semantics->min.f64;
            record.max_f64 = semantics->max.f64;
            break;
        default:
            break;
    }
    return true;
}

bool RecordSemanticsChain(
    CounterRegistrationRecord& record, const nvtxSemanticsHeader_t* semantics)
{
    for (auto* header = semantics; header != nullptr; header = header->next)
    {
        if (header->semanticId == NVTX_SEMANTIC_ID_COUNTERS_V1)
        {
            if (!RecordCounterSemantics(
                    record.semantics, reinterpret_cast<const nvtxSemanticsCounter_t*>(header)))
            {
                return false;
            }
        }
        else if (header->semanticId == NVTX_SEMANTIC_ID_TIME_V1)
        {
            auto* timeSemantics = reinterpret_cast<const nvtxSemanticsTime_t*>(header);
            record.semantics.has_time_semantics = 1;
            record.semantics.time_domain_id = timeSemantics->timeDomainId;
        }
        else
        {
            std::string error_message = "Unsupported counter semantics ID: " +
                std::to_string(header->semanticId);
            PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
            return false;
        }
    }
    return true;
}

uint64_t ScopeRegister(nvtxDomainHandle_t domain, const nvtxScopeAttr_t* attr)
{
    if (attr == nullptr || attr->path == nullptr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid scope attributes");
        return NVTX_SCOPE_NONE;
    }

    EventRecord record{};
    record.kind = static_cast<uint32_t>(EventKind::ScopeRegister);
    record.domain = ResolveDomain(domain);
    if (!CopyCString(record.scope.path, attr->path, "Scope path"))
    {
        return NVTX_SCOPE_NONE;
    }
    uint64_t scopeId = g_scopeId.fetch_add(1, std::memory_order_relaxed);
    record.scope.id = scopeId;
    g_events.push_back(record);
    return scopeId;
}

int64_t TimestampGet()
{
    int64_t value = g_nextTimestamp.fetch_add(1, std::memory_order_relaxed);
    EventRecord record{};
    record.kind = static_cast<uint32_t>(EventKind::TimestampGet);
    record.domain = DefaultDomain;
    record.timestamp.value = value;
    g_events.push_back(record);
    return value;
}

uint64_t CounterRegister(nvtxDomainHandle_t domain, const nvtxCounterAttr_t* attr)
{
    if (attr == nullptr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid counter attributes");
        return NVTX_COUNTER_ID_NONE;
    }

    if (attr->name == nullptr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid counter name");
        return NVTX_COUNTER_ID_NONE;
    }

    EventRecord record{};
    record.kind = static_cast<uint32_t>(EventKind::CounterRegister);
    record.domain = ResolveDomain(domain);
    record.counter.registration.schema_id = attr->schemaId;
    if (!CopyCString(record.counter.registration.name, attr->name, "Counter name"))
    {
        return NVTX_COUNTER_ID_NONE;
    }
    if (attr->description != nullptr &&
        !CopyCString(
            record.counter.registration.description, attr->description, "Counter description"))
    {
        return NVTX_COUNTER_ID_NONE;
    }
    uint64_t counterId = g_counterId.fetch_add(1, std::memory_order_relaxed);
    record.counter.id = counterId;
    record.counter.registration.scope_id = attr->scopeId;
    if (!RecordSemanticsChain(record.counter.registration, attr->semantics))
    {
        return NVTX_COUNTER_ID_NONE;
    }
    g_events.push_back(record);
    return counterId;
}

void CounterSampleInt64(nvtxDomainHandle_t domain, uint64_t counterId, int64_t value)
{
    EventRecord record{};
    record.kind = static_cast<uint32_t>(EventKind::CounterSampleInt64);
    record.domain = ResolveDomain(domain);
    record.counter.id = counterId;
    record.counter.sample.i64 = value;
    g_events.push_back(record);
}

void CounterSampleFloat64(nvtxDomainHandle_t domain, uint64_t counterId, double value)
{
    EventRecord record{};
    record.kind = static_cast<uint32_t>(EventKind::CounterSampleFloat64);
    record.domain = ResolveDomain(domain);
    record.counter.id = counterId;
    record.counter.sample.f64 = value;
    g_events.push_back(record);
}

void CounterSample(nvtxDomainHandle_t domain, uint64_t counterId,
                   const void* value, size_t size)
{
    EventRecord record{};
    record.kind = static_cast<uint32_t>(EventKind::CounterSample);
    record.domain = ResolveDomain(domain);
    record.counter.id = counterId;
    if (size > sizeof(record.counter.sample.data))
    {
        PyErr_SetString(PyExc_RuntimeError, "Counter sample exceeds fixed test buffer.");
        return;
    }
    record.counter.sample.data_size = size;
    if (size != 0 && value != nullptr)
    {
        memcpy(record.counter.sample.data, value, size);
    }
    g_events.push_back(record);
}

void CounterSampleNoValue(nvtxDomainHandle_t domain, uint64_t counterId, uint8_t reason)
{
    EventRecord record{};
    record.kind = static_cast<uint32_t>(EventKind::CounterSampleNoValue);
    record.domain = ResolveDomain(domain);
    record.counter.id = counterId;
    record.counter.sample.no_value_reason = reason;
    g_events.push_back(record);
}

void CounterBatchSubmit(nvtxDomainHandle_t domain, const nvtxCounterBatch_t* counterData)
{
    if (counterData == nullptr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid counter batch");
        return;
    }

    EventRecord record{};
    record.kind = static_cast<uint32_t>(EventKind::CounterBatchSubmit);
    record.domain = ResolveDomain(domain);
    record.counter.id = counterData->counterId;
    if (counterData->countersSize > sizeof(record.counter.sample.data))
    {
        PyErr_SetString(PyExc_RuntimeError, "Counter batch exceeds fixed test buffer.");
        return;
    }
    record.counter.sample.data_size = counterData->countersSize;
    if (counterData->countersSize != 0 && counterData->counters != nullptr)
    {
        memcpy(record.counter.sample.data, counterData->counters, counterData->countersSize);
    }
    if (counterData->timestampsSize > sizeof(record.counter.sample.timestamps))
    {
        PyErr_SetString(PyExc_RuntimeError, "Counter timestamps exceed fixed test buffer.");
        return;
    }
    record.counter.sample.timestamps_size = counterData->timestampsSize;
    if (counterData->timestampsSize != 0 && counterData->timestamps != nullptr)
    {
        memcpy(record.counter.sample.timestamps, counterData->timestamps,
               counterData->timestampsSize);
    }
    g_events.push_back(record);
}

const char* ResolveString(nvtxStringHandle_t string)
{
    size_t stringIndex = reinterpret_cast<size_t>(string);
    if (g_registeredStrings.size() <= stringIndex)
    {
        std::string error_message = "String index " + std::to_string(stringIndex) +
            " is out of bounds. g_registered_strings.size(): " +
            std::to_string(g_registeredStrings.size());
        PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
        return nullptr;
    }
    return g_registeredStrings[stringIndex].c_str();
}

const char* ResolveMessage(const nvtxEventAttributes_t* eventAttrib)
{
    if (eventAttrib == nullptr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid event attributes");
        return nullptr;
    }
    if (eventAttrib->messageType == NVTX_MESSAGE_TYPE_REGISTERED)
    {
        return ResolveString(eventAttrib->message.registered);
    }
    return nullptr;
}

void ValidateEventAttributes(const nvtxEventAttributes_t* attrib)
{
    if (attrib == nullptr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid event attributes");
        return;
    }

    if (attrib->version != NVTX_VERSION)
    {
        std::string error_message = "Invalid version: " + std::to_string(attrib->version);
        PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
        return;
    }
    if (attrib->size != NVTX_EVENT_ATTRIB_STRUCT_SIZE)
    {
        std::string error_message = "Invalid size: " + std::to_string(attrib->size);
        PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
        return;
    }
    if (attrib->colorType != NVTX_COLOR_ARGB)
    {
        std::string error_message = "Invalid color type: " + std::to_string(attrib->colorType);
        PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
        return;
    }

    switch (attrib->messageType) {
        case NVTX_MESSAGE_UNKNOWN:
        case NVTX_MESSAGE_TYPE_REGISTERED:
            break;
        default:
        {
            std::string error_message = "Invalid message type: " + std::to_string(attrib->messageType);
            PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
            return;
        }
    }

    switch (attrib->payloadType) {
        case NVTX_PAYLOAD_UNKNOWN:
        case NVTX_PAYLOAD_TYPE_INT64:
        case NVTX_PAYLOAD_TYPE_DOUBLE:
        case NVTX_PAYLOAD_TYPE_EXT:
            break;
        default:
        {
            std::string error_message = "Invalid payload type: " + std::to_string(attrib->payloadType);
            PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
            return;
        }
    }
}

void RecordSimpleEvent(EventKind kind, const char* domain, const char* message = nullptr,
    uint32_t category = 0, uint64_t rangeId = 0)
{
    EventRecord record{};
    record.kind = static_cast<uint32_t>(kind);
    record.domain = domain;
    record.attributes.message_type =
        message == nullptr ? NVTX_MESSAGE_UNKNOWN : NVTX_MESSAGE_TYPE_REGISTERED;
    record.attributes.message = message;
    record.attributes.category = category;
    record.attributes.range_id = rangeId;
    g_events.push_back(record);
}

void RecordEventFromAttrib(
    EventKind kind, const char* domain, const nvtxEventAttributes_t* attrib)
{
    ValidateEventAttributes(attrib);
    EventRecord record{};
    record.kind = static_cast<uint32_t>(kind);
    record.domain = domain;
    record.attributes.message_type = attrib->messageType;
    record.attributes.message = ResolveMessage(attrib);
    record.attributes.category = attrib->category;
    record.attributes.range_id = 0;
    record.attributes.color = attrib->color;
    record.payload.type = attrib->payloadType;
    record.payload.i64 =
        attrib->payloadType == NVTX_PAYLOAD_TYPE_INT64 ? attrib->payload.llValue : 0;
    record.payload.f64 =
        attrib->payloadType == NVTX_PAYLOAD_TYPE_DOUBLE ? attrib->payload.dValue : 0.0;

    if (attrib->payloadType == NVTX_PAYLOAD_TYPE_EXT)
    {
        auto numPayloads = attrib->reserved0;
        if (numPayloads >= 1)
        {
            auto* payloadDataPtr = reinterpret_cast<const nvtxPayloadData_t*>(
                static_cast<uintptr_t>(attrib->payload.ullValue));
            record.payload.ext_schema_id = payloadDataPtr->schemaId;
            record.payload.ext_size = payloadDataPtr->size;
            size_t copySize = (std::min)(payloadDataPtr->size, sizeof(record.payload.ext_data));
            memcpy(record.payload.ext_data, payloadDataPtr->payload, copySize);
        }
    }

    g_events.push_back(record);
}

nvtxDomainHandle_t DomainCreateA(const char* name)
{
    if (name == nullptr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid Domain name");
        return nullptr;
    }
    g_registeredDomains.push_back(name);
    RecordSimpleEvent(EventKind::DomainCreate, g_registeredDomains.back().c_str());
    return reinterpret_cast<nvtxDomainHandle_t>(g_registeredDomains.size());
}

nvtxStringHandle_t DomainRegisterStringA(nvtxDomainHandle_t domain, const char* string)
{
    if (string == nullptr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid string");
        return nullptr;
    }
    g_registeredStrings.push_back(string);
    RecordSimpleEvent(
        EventKind::DomainRegisterString, ResolveDomain(domain), g_registeredStrings.back().c_str());
    return reinterpret_cast<nvtxStringHandle_t>(g_registeredStrings.size() - 1);
}

void DomainNameCategoryA(nvtxDomainHandle_t domain, uint32_t category, const char* name)
{
    if (name == nullptr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid category name");
        return;
    }
    if (category == 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "DomainNameCategoryA: Invalid category ID");
        return;
    }
    g_registeredStrings.push_back(name);
    RecordSimpleEvent(
        EventKind::DomainNameCategory, ResolveDomain(domain), g_registeredStrings.back().c_str(),
        category);
}

void DomainMarkEx(nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib)
{
    RecordEventFromAttrib(EventKind::Mark, ResolveDomain(domain), eventAttrib);
}

void DomainRangePushEx(nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib)
{
    RecordEventFromAttrib(EventKind::RangePush, ResolveDomain(domain), eventAttrib);
}

void DomainRangePop(nvtxDomainHandle_t domain)
{
    RecordSimpleEvent(EventKind::RangePop, ResolveDomain(domain));
}

nvtxRangeId_t DomainRangeStartEx(nvtxDomainHandle_t domain,
                                 const nvtxEventAttributes_t* eventAttrib)
{
    uint64_t id = g_rangeId.fetch_add(1, std::memory_order_relaxed);
    RecordEventFromAttrib(EventKind::RangeStart, ResolveDomain(domain), eventAttrib);
    return id;
}

void DomainRangeEnd(nvtxDomainHandle_t domain, nvtxRangeId_t id)
{
    if (id == 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid range id");
        return;
    }
    RecordSimpleEvent(EventKind::RangeEnd, ResolveDomain(domain), nullptr, 0, id);
}

NvtxFunctionTable GetFunctionTable(
    NvtxGetExportTableFunc_t getExportTable, NvtxCallbackModule callbackModule)
{
    auto callbacks =
        reinterpret_cast<const NvtxExportTableCallbacks*>(getExportTable(NVTX_ETID_CALLBACKS));
    if (!callbacks)
    {
        return nullptr;
    }

    NvtxFunctionTable table = nullptr;
    unsigned int tableSize = 0;
    if (!callbacks->GetModuleFunctionTable(callbackModule, &table, &tableSize))
    {
        return nullptr;
    }
    return table;
}
}  // namespace

extern "C"
{

EXPORT_SYMBOL int InitializeInjectionNvtx2(NvtxGetExportTableFunc_t getExportTable)
{
    NvtxFunctionTable core2 = GetFunctionTable(getExportTable, NVTX_CB_MODULE_CORE2);
    if (!core2)
    {
        return 0;
    }
    *core2[NVTX_CBID_CORE2_DomainCreateA] =
        reinterpret_cast<NvtxFunctionPointer>(DomainCreateA);
    *core2[NVTX_CBID_CORE2_DomainMarkEx] =
        reinterpret_cast<NvtxFunctionPointer>(DomainMarkEx);
    *core2[NVTX_CBID_CORE2_DomainRangePushEx] =
        reinterpret_cast<NvtxFunctionPointer>(DomainRangePushEx);
    *core2[NVTX_CBID_CORE2_DomainRangePop] =
        reinterpret_cast<NvtxFunctionPointer>(DomainRangePop);
    *core2[NVTX_CBID_CORE2_DomainRangeStartEx] =
        reinterpret_cast<NvtxFunctionPointer>(DomainRangeStartEx);
    *core2[NVTX_CBID_CORE2_DomainRangeEnd] =
        reinterpret_cast<NvtxFunctionPointer>(DomainRangeEnd);
    *core2[NVTX_CBID_CORE2_DomainRegisterStringA] =
        reinterpret_cast<NvtxFunctionPointer>(DomainRegisterStringA);
    *core2[NVTX_CBID_CORE2_DomainNameCategoryA] =
        reinterpret_cast<NvtxFunctionPointer>(DomainNameCategoryA);

    return 1;
}

EXPORT_SYMBOL int InitializeInjectionNvtxExtension(nvtxExtModuleInfo_t* moduleInfo)
{
    if (moduleInfo == nullptr || moduleInfo->segments == nullptr || moduleInfo->segmentsCount == 0)
    {
        return 0;
    }

    if (moduleInfo->moduleId == NVTX_EXT_PAYLOAD_MODULEID)
    {
        auto& seg = moduleInfo->segments[0];
        if (seg.slotCount > NVTX3EXT_CBID_nvtxPayloadSchemaRegister)
        {
            seg.functionSlots[NVTX3EXT_CBID_nvtxPayloadSchemaRegister] =
                reinterpret_cast<intptr_t>(PayloadSchemaRegister);
        }
        if (seg.slotCount > NVTX3EXT_CBID_nvtxDomainIsEnabled)
        {
            seg.functionSlots[NVTX3EXT_CBID_nvtxDomainIsEnabled] =
                reinterpret_cast<intptr_t>(DomainIsEnabled);
        }
        if (seg.slotCount > NVTX3EXT_CBID_nvtxTimestampGet)
        {
            seg.functionSlots[NVTX3EXT_CBID_nvtxTimestampGet] =
                reinterpret_cast<intptr_t>(TimestampGet);
        }
        if (seg.slotCount > NVTX3EXT_CBID_nvtxScopeRegister)
        {
            seg.functionSlots[NVTX3EXT_CBID_nvtxScopeRegister] =
                reinterpret_cast<intptr_t>(ScopeRegister);
        }
    }

    if (moduleInfo->moduleId == NVTX_EXT_COUNTERS_MODULEID)
    {
        auto& seg = moduleInfo->segments[0];
        if (seg.slotCount > NVTX3EXT_CBID_nvtxCounterRegister)
        {
            seg.functionSlots[NVTX3EXT_CBID_nvtxCounterRegister] =
                reinterpret_cast<intptr_t>(CounterRegister);
        }
        if (seg.slotCount > NVTX3EXT_CBID_nvtxCounterSampleInt64)
        {
            seg.functionSlots[NVTX3EXT_CBID_nvtxCounterSampleInt64] =
                reinterpret_cast<intptr_t>(CounterSampleInt64);
        }
        if (seg.slotCount > NVTX3EXT_CBID_nvtxCounterSampleFloat64)
        {
            seg.functionSlots[NVTX3EXT_CBID_nvtxCounterSampleFloat64] =
                reinterpret_cast<intptr_t>(CounterSampleFloat64);
        }
        if (seg.slotCount > NVTX3EXT_CBID_nvtxCounterSample)
        {
            seg.functionSlots[NVTX3EXT_CBID_nvtxCounterSample] =
                reinterpret_cast<intptr_t>(CounterSample);
        }
        if (seg.slotCount > NVTX3EXT_CBID_nvtxCounterSampleNoValue)
        {
            seg.functionSlots[NVTX3EXT_CBID_nvtxCounterSampleNoValue] =
                reinterpret_cast<intptr_t>(CounterSampleNoValue);
        }
        if (seg.slotCount > NVTX3EXT_CBID_nvtxCounterBatchSubmit)
        {
            seg.functionSlots[NVTX3EXT_CBID_nvtxCounterBatchSubmit] =
                reinterpret_cast<intptr_t>(CounterBatchSubmit);
        }
    }

    return 1;
}


// This function is called by the Python tests to read the next event from the buffer.
// See `NvtxEventsReader.__next__()` in `conftest.py`.
// Returns `true` if an event was read, `false` if the buffer is empty.
EXPORT_SYMBOL bool read_event(EventRecord* out)
{
    if (out == nullptr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid output pointer for read_event");
        // Return value is ignored since we raised a Python exception here.
        return false;
    }
    if (g_events.empty())
    {
        return false;
    }
    memcpy(out, &g_events.front(), sizeof(EventRecord));
    g_events.pop_front();
    return true;
}

EXPORT_SYMBOL void* PyInit_nvtx_test_injection()
{
    return nullptr;
}

} // extern "C"
