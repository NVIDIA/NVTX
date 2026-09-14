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
 * Recording NVTXW backend used by the nvtx.writer test suite.
 *
 * The backend implements every interface entry point used by the Python
 * bindings. Calls are deep-copied into an in-memory queue and drained through
 * read_event(), so tests inspect values while caller-owned storage is valid.
 * Session, domain, and stream ownership is tracked explicitly to catch handles
 * being passed through the wrong object graph.
 */

#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <string>

/* NVTXW reuses the NVTX payload/counter types but not their implementations. */
#define NVTX_NO_IMPL
#include <nvtx3/nvToolsExtSemanticsCounters.h>
#include <nvtx3/nvToolsExtSemanticsTime.h>
#include <nvtxw3/nvtxw3.h>

#ifdef _WIN32
#include <Python.h>
#endif

#ifdef _WIN32
#define EXPORT_SYMBOL __declspec(dllexport)
#else
#define EXPORT_SYMBOL __attribute__((visibility("default")))
#endif

namespace
{
constexpr size_t EventBufferSize = 256;
constexpr size_t SchemaEntryBufferCount = 32;
constexpr size_t SemanticsBufferCount = 4;
constexpr size_t TimestampBufferCount = 32;

enum class WriterEventKind : uint32_t
{
    SessionBegin = 1,
    SessionEnd = 2,
    DomainRegister = 3,
    StreamOpen = 4,
    StreamClose = 5,
    SchemaRegister = 6,
    EventWrite = 7,
    StringRegister = 8,
    ScopeRegister = 9,
    CategoryRegister = 10,
    EventBatchWrite = 11,
    CounterRegister = 12,
    CounterWrite = 13,
    CounterNoValueWrite = 14,
    CounterBatchWrite = 15,
};

struct SemanticsRecord
{
    uint64_t count{0};
    uint16_t ids[SemanticsBufferCount]{};
    uint16_t versions[SemanticsBufferCount]{};
    uint32_t struct_sizes[SemanticsBufferCount]{};

    uint8_t has_counter{0};
    uint8_t has_time{0};
    uint64_t time_domain_id{0};

    uint64_t counter_flags{0};
    char counter_unit[EventBufferSize]{};
    uint64_t counter_unit_scale_numerator{0};
    uint64_t counter_unit_scale_denominator{0};
    int64_t counter_limit_type{0};
    int64_t counter_min_i64{0};
    uint64_t counter_min_u64{0};
    double counter_min_f64{0.0};
    int64_t counter_max_i64{0};
    uint64_t counter_max_u64{0};
    double counter_max_f64{0.0};
};

struct SchemaEntryRecord
{
    uint64_t flags{0};
    uint64_t type{0};
    char name[EventBufferSize]{};
    char description[EventBufferSize]{};
    uint64_t array_or_union_detail{0};
    uint64_t offset{0};
    SemanticsRecord semantics{};
    uint8_t has_reserved{0};
};

/* Layout must match _WriterEventBuffer in the writer test conftest. */
struct WriterEventRecord
{
    uint32_t kind{0};

    /* Common ownership handles. */
    uint64_t session_id{0};
    uint64_t domain_id{0};
    uint64_t stream_id{0};

    /* SessionBegin / DomainRegister / StreamOpen. */
    char session_name[EventBufferSize]{};
    char session_config[EventBufferSize]{};
    char domain_name[EventBufferSize]{};
    char stream_name[EventBufferSize]{};
    uint64_t stream_domain_id{0};
    uint64_t scope_id{0};
    uint64_t time_domain_id{0};
    int16_t order_interleaving{0};
    int16_t ordering_type{0};
    int32_t ordering_skid{0};
    int64_t ordering_skid_amount{0};

    /* CategoryRegister. */
    uint32_t category_id{0};
    char category_name[EventBufferSize]{};

    /* StringRegister. */
    char string_value[EventBufferSize]{};
    uint64_t string_handle{0};

    /* ScopeRegister. */
    char scope_path[EventBufferSize]{};
    uint64_t scope_parent{0};
    uint64_t scope_requested_id{0};
    uint64_t registered_scope_id{0};

    /* SchemaRegister. */
    uint64_t schema_id{0};
    uint64_t schema_field_mask{0};
    char schema_name[EventBufferSize]{};
    uint64_t schema_type{0};
    uint64_t schema_flags{0};
    uint64_t num_entries{0};
    uint64_t schema_static_size{0};
    uint64_t schema_pack_align{0};
    uint64_t schema_requested_id{0};
    uint8_t schema_has_extension{0};
    SchemaEntryRecord schema_entries[SchemaEntryBufferCount]{};

    /* EventWrite. */
    uint64_t payload_count{0};
    uint64_t payload0_schema_id{0};
    uint64_t payload0_size{0};
    uint8_t payload0[EventBufferSize]{};
    uint64_t payload1_schema_id{0};
    uint64_t payload1_size{0};
    uint8_t payload1[EventBufferSize]{};

    /* EventBatchWrite. */
    uint64_t event_batch_schema_id{0};
    uint64_t event_batch_size{0};
    uint8_t event_batch_data[EventBufferSize]{};
    uint64_t event_batch_scope{0};
    uint64_t event_batch_flags{0};
    uint64_t event_batch_flex_size{0};
    uint64_t event_batch_flex_offset{0};
    uint8_t event_batch_flex_data[EventBufferSize]{};

    /* CounterRegister and counter writes. */
    uint64_t counter_id{0};
    uint64_t counter_requested_id{0};
    uint64_t counter_schema_id{0};
    char counter_name[EventBufferSize]{};
    char counter_description[EventBufferSize]{};
    uint64_t counter_scope_id{0};
    SemanticsRecord counter_semantics{};
    int64_t counter_timestamp{0};
    uint8_t counter_no_value_reason{0};
    uint64_t counter_data_size{0};
    uint8_t counter_data[EventBufferSize]{};
    uint64_t counter_batch_flags{0};
    uint64_t counter_timestamps_size{0};
    int64_t counter_timestamps[TimestampBufferCount]{};
};

struct SessionState
{
    std::map<std::string, uint64_t> domains;
    std::set<uint64_t> domain_ids;
    std::set<uint64_t> stream_ids;
};

struct StreamState
{
    uint64_t session_id{0};
    uint64_t domain_id{0};
};

std::deque<WriterEventRecord> g_events;
std::map<uint64_t, SessionState> g_sessions;
std::map<uint64_t, uint64_t> g_domain_sessions;
std::map<uint64_t, StreamState> g_streams;
nvtxwResultCode_t g_nextResult{NVTXW_RESULT_SUCCESS};
uint64_t g_finalizeCount{0};

#if UINTPTR_MAX > UINT32_MAX
constexpr uint64_t SessionHandleStart = UINT64_C(0x0000001100000000);
constexpr uint64_t StreamHandleStart = UINT64_C(0x0000002200000000);
constexpr uint64_t DomainHandleStart = UINT64_C(0x0000003300000000);
constexpr uint64_t StringHandleStart = UINT64_C(0x0000004400000000);
#else
constexpr uint64_t SessionHandleStart = UINT64_C(0x10000);
constexpr uint64_t StreamHandleStart = UINT64_C(0x20000);
constexpr uint64_t DomainHandleStart = UINT64_C(0x30000);
constexpr uint64_t StringHandleStart = UINT64_C(0x40000);
#endif
uint64_t g_sessionId{SessionHandleStart};
uint64_t g_streamId{StreamHandleStart};
uint64_t g_domainId{DomainHandleStart};
uint64_t g_schemaId{NVTX_PAYLOAD_SCHEMA_ID_DYNAMIC_START};
uint64_t g_stringHandle{StringHandleStart};
uint64_t g_scopeId{NVTX_SCOPE_ID_DYNAMIC_START};
uint64_t g_counterId{NVTX_COUNTER_ID_DYNAMIC_START};

/*
 * This test backend is intentionally single-threaded. Add synchronization if
 * multithreaded writer tests are introduced.
 */

nvtxwResultCode_t TakeNextResult()
{
    const nvtxwResultCode_t result = g_nextResult;
    g_nextResult = NVTXW_RESULT_SUCCESS;
    return result;
}

#define RETURN_IF_NEXT_RESULT_FAIL()                                    \
    do                                                                  \
    {                                                                   \
        const nvtxwResultCode_t result = TakeNextResult();              \
        if (result != NVTXW_RESULT_SUCCESS)                             \
        {                                                               \
            return result;                                              \
        }                                                               \
    } while (false)

template <size_t N>
bool CopyCString(char (&dst)[N], const char* src)
{
    if (src == nullptr)
    {
        return true;
    }
    const size_t length = std::strlen(src);
    if (length >= N)
    {
        return false;
    }
    std::memcpy(dst, src, length + 1);
    return true;
}

template <size_t N>
bool CopyBytes(uint8_t (&dst)[N], const void* src, size_t size)
{
    if (size > N || (size != 0 && src == nullptr))
    {
        return false;
    }
    if (size != 0)
    {
        std::memcpy(dst, src, size);
    }
    return true;
}

uint64_t HandleId(const void* handle)
{
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
}

bool FindDomainSession(uint64_t domain_id, uint64_t& session_id)
{
    const auto it = g_domain_sessions.find(domain_id);
    if (it == g_domain_sessions.end())
    {
        return false;
    }
    session_id = it->second;
    return true;
}

bool FillStreamOwnership(WriterEventRecord& record, uint64_t stream_id)
{
    const auto it = g_streams.find(stream_id);
    if (it == g_streams.end())
    {
        return false;
    }
    record.session_id = it->second.session_id;
    record.domain_id = it->second.domain_id;
    record.stream_id = stream_id;
    return true;
}

bool CaptureSemantics(
    SemanticsRecord& record, const nvtxSemanticsHeader_t* semantics)
{
    for (auto* header = semantics; header != nullptr; header = header->next)
    {
        if (record.count >= SemanticsBufferCount)
        {
            return false;
        }
        const size_t index = static_cast<size_t>(record.count++);
        record.ids[index] = header->semanticId;
        record.versions[index] = header->version;
        record.struct_sizes[index] = header->structSize;

        if (header->semanticId == NVTX_SEMANTIC_ID_COUNTERS_V1)
        {
            if (header->structSize < sizeof(nvtxSemanticsCounter_t))
            {
                return false;
            }
            const auto* counter =
                reinterpret_cast<const nvtxSemanticsCounter_t*>(header);
            record.has_counter = 1;
            record.counter_flags = counter->flags;
            if (!CopyCString(record.counter_unit, counter->unit))
            {
                return false;
            }
            record.counter_unit_scale_numerator =
                counter->unitScaleNumerator;
            record.counter_unit_scale_denominator =
                counter->unitScaleDenominator;
            record.counter_limit_type = counter->limitType;
            switch (counter->limitType)
            {
                case NVTX_COUNTER_LIMIT_I64:
                    record.counter_min_i64 = counter->min.i64;
                    record.counter_max_i64 = counter->max.i64;
                    break;
                case NVTX_COUNTER_LIMIT_U64:
                    record.counter_min_u64 = counter->min.u64;
                    record.counter_max_u64 = counter->max.u64;
                    break;
                case NVTX_COUNTER_LIMIT_F64:
                    record.counter_min_f64 = counter->min.f64;
                    record.counter_max_f64 = counter->max.f64;
                    break;
                default:
                    break;
            }
        }
        else if (header->semanticId == NVTX_SEMANTIC_ID_TIME_V1)
        {
            if (header->structSize < sizeof(nvtxSemanticsTime_t))
            {
                return false;
            }
            const auto* time =
                reinterpret_cast<const nvtxSemanticsTime_t*>(header);
            record.has_time = 1;
            record.time_domain_id = time->timeDomainId;
        }
    }
    return true;
}

bool CaptureSchema(
    WriterEventRecord& record, const nvtxPayloadSchemaAttr_t* attr)
{
    constexpr uint64_t RequiredFields =
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
        NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES;
    if (attr == nullptr ||
        (attr->fieldMask & RequiredFields) != RequiredFields)
    {
        return false;
    }
    record.schema_field_mask = attr->fieldMask;
    record.schema_type = attr->type;
    record.num_entries = attr->numEntries;
    if (record.num_entries > SchemaEntryBufferCount ||
        (record.num_entries != 0 && attr->entries == nullptr))
    {
        return false;
    }

    if ((attr->fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME) != 0 &&
        !CopyCString(record.schema_name, attr->name))
    {
        return false;
    }
    if ((attr->fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_FLAGS) != 0)
    {
        record.schema_flags = attr->flags;
    }
    if ((attr->fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE) != 0)
    {
        record.schema_static_size = attr->payloadStaticSize;
    }
    if ((attr->fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ALIGNMENT) != 0)
    {
        record.schema_pack_align = attr->packAlign;
    }
    if ((attr->fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_SCHEMA_ID) != 0)
    {
        record.schema_requested_id = attr->schemaId;
    }
    if ((attr->fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_EXTENSION) != 0)
    {
        record.schema_has_extension = attr->extension != nullptr;
    }

    for (size_t i = 0; i < record.num_entries; ++i)
    {
        const auto& src = attr->entries[i];
        auto& dst = record.schema_entries[i];
        dst.flags = src.flags;
        dst.type = src.type;
        if (!CopyCString(dst.name, src.name) ||
            !CopyCString(dst.description, src.description) ||
            !CaptureSemantics(dst.semantics, src.semantics))
        {
            return false;
        }
        dst.array_or_union_detail = src.arrayOrUnionDetail;
        dst.offset = src.offset;
        dst.has_reserved = src.reserved != nullptr;
    }
    return true;
}

nvtxwResultCode_t SessionBegin(
    const nvtxwSessionAttributes_t* attr, nvtxwSessionHandle_t* sessionOut)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    if (attr == nullptr || sessionOut == nullptr ||
        attr->structSize < sizeof(nvtxwSessionAttributes_t))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::SessionBegin);
    if (!CopyCString(record.session_name, attr->name) ||
        !CopyCString(record.session_config, attr->configString))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }

    const uint64_t id = g_sessionId++;
    record.session_id = id;
    g_sessions.emplace(id, SessionState{});
    g_events.push_back(record);
    *sessionOut = reinterpret_cast<nvtxwSessionHandle_t>(id);
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t SessionEnd(nvtxwSessionHandle_t session)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    const uint64_t id = HandleId(session);
    const auto session_it = g_sessions.find(id);
    if (session_it == g_sessions.end())
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }

    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::SessionEnd);
    record.session_id = id;
    g_events.push_back(record);

    for (uint64_t domain_id : session_it->second.domain_ids)
    {
        g_domain_sessions.erase(domain_id);
    }
    for (uint64_t stream_id : session_it->second.stream_ids)
    {
        g_streams.erase(stream_id);
    }
    g_sessions.erase(session_it);
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t DomainRegister(
    nvtxwSessionHandle_t session,
    const nvtxwDomainAttributes_t* attr,
    nvtxDomainHandle_t* domainOut)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    if (attr == nullptr || domainOut == nullptr ||
        attr->structSize < sizeof(nvtxwDomainAttributes_t))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    const uint64_t session_id = HandleId(session);
    const char* name = attr->name != nullptr ? attr->name : "";

    const auto session_it = g_sessions.find(session_id);
    if (session_it == g_sessions.end())
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }

    uint64_t domain_id = 0;
    const auto domain_it = session_it->second.domains.find(name);
    if (domain_it != session_it->second.domains.end())
    {
        domain_id = domain_it->second;
    }
    else
    {
        domain_id = g_domainId++;
        session_it->second.domains.emplace(name, domain_id);
        session_it->second.domain_ids.insert(domain_id);
        g_domain_sessions.emplace(domain_id, session_id);
    }

    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::DomainRegister);
    record.session_id = session_id;
    record.domain_id = domain_id;
    if (!CopyCString(record.domain_name, name))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    g_events.push_back(record);
    *domainOut = reinterpret_cast<nvtxDomainHandle_t>(domain_id);
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t CategoryRegister(
    nvtxDomainHandle_t domain, uint32_t category, const char* name)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    const uint64_t domain_id = HandleId(domain);
    uint64_t session_id = 0;
    if (name == nullptr || !FindDomainSession(domain_id, session_id))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::CategoryRegister);
    record.session_id = session_id;
    record.domain_id = domain_id;
    record.category_id = category;
    if (!CopyCString(record.category_name, name))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    g_events.push_back(record);
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t StringRegister(
    nvtxDomainHandle_t domain,
    const char* string,
    nvtxStringHandle_t* stringHandleOut)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    const uint64_t domain_id = HandleId(domain);
    uint64_t session_id = 0;
    if (string == nullptr || stringHandleOut == nullptr ||
        !FindDomainSession(domain_id, session_id))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::StringRegister);
    record.session_id = session_id;
    record.domain_id = domain_id;
    if (!CopyCString(record.string_value, string))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    const uint64_t handle = g_stringHandle++;
    record.string_handle = handle;
    g_events.push_back(record);
    *stringHandleOut = reinterpret_cast<nvtxStringHandle_t>(handle);
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t ScopeRegister(
    nvtxDomainHandle_t domain,
    const nvtxScopeAttr_t* attr,
    uint64_t* scopeIdOut)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    const uint64_t domain_id = HandleId(domain);
    uint64_t session_id = 0;
    if (attr == nullptr || attr->structSize < sizeof(nvtxScopeAttr_t) ||
        !FindDomainSession(domain_id, session_id))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    if (attr->scopeId == NVTX_SCOPE_NONE && scopeIdOut == nullptr)
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    const uint64_t id =
        attr->scopeId != NVTX_SCOPE_NONE ? attr->scopeId : g_scopeId++;
    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::ScopeRegister);
    record.session_id = session_id;
    record.domain_id = domain_id;
    if (!CopyCString(record.scope_path, attr->path))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    record.scope_parent = attr->parentScope;
    record.scope_requested_id = attr->scopeId;
    record.registered_scope_id = id;
    g_events.push_back(record);
    if (scopeIdOut != nullptr)
    {
        *scopeIdOut = id;
    }
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t SchemaRegister(
    nvtxDomainHandle_t domain,
    const nvtxPayloadSchemaAttr_t* attr,
    uint64_t* schemaIdOut)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    const uint64_t domain_id = HandleId(domain);
    uint64_t session_id = 0;
    if (schemaIdOut == nullptr || !FindDomainSession(domain_id, session_id))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::SchemaRegister);
    record.session_id = session_id;
    record.domain_id = domain_id;
    if (!CaptureSchema(record, attr))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    const uint64_t id = record.schema_requested_id != 0
        ? record.schema_requested_id
        : g_schemaId++;
    record.schema_id = id;
    g_events.push_back(record);
    *schemaIdOut = id;
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t CounterRegister(
    nvtxDomainHandle_t domain,
    const nvtxCounterAttr_t* attr,
    uint64_t* counterIdOut)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    const uint64_t domain_id = HandleId(domain);
    uint64_t session_id = 0;
    if (attr == nullptr || attr->structSize < sizeof(nvtxCounterAttr_t) ||
        counterIdOut == nullptr || attr->name == nullptr ||
        !FindDomainSession(domain_id, session_id))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }

    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::CounterRegister);
    record.session_id = session_id;
    record.domain_id = domain_id;
    record.counter_schema_id = attr->schemaId;
    record.counter_scope_id = attr->scopeId;
    record.counter_requested_id = attr->counterId;
    if (!CopyCString(record.counter_name, attr->name) ||
        !CopyCString(record.counter_description, attr->description) ||
        !CaptureSemantics(record.counter_semantics, attr->semantics))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    const uint64_t id =
        attr->counterId != 0 ? attr->counterId : g_counterId++;
    record.counter_id = id;
    g_events.push_back(record);
    *counterIdOut = id;
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t StreamOpen(
    nvtxwSessionHandle_t session,
    const nvtxwStreamAttributes_t* attr,
    nvtxwStreamHandle_t* streamOut)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    if (attr == nullptr || streamOut == nullptr ||
        attr->structSize < sizeof(nvtxwStreamAttributes_t))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    const uint64_t session_id = HandleId(session);
    const uint64_t domain_id = HandleId(attr->domain);
    const auto session_it = g_sessions.find(session_id);
    uint64_t owner_session_id = 0;
    if (session_it == g_sessions.end() ||
        (domain_id != 0 &&
         (!FindDomainSession(domain_id, owner_session_id) ||
          owner_session_id != session_id)))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }

    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::StreamOpen);
    record.session_id = session_id;
    record.domain_id = domain_id;
    record.stream_domain_id = domain_id;
    if (!CopyCString(record.stream_name, attr->name))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    record.scope_id = attr->scopeId;
    record.time_domain_id = attr->timeDomainId;
    record.order_interleaving = attr->orderInterleaving;
    record.ordering_type = attr->orderingType;
    record.ordering_skid = attr->orderingSkid;
    record.ordering_skid_amount = attr->orderingSkidAmount;

    const uint64_t stream_id = g_streamId++;
    record.stream_id = stream_id;
    g_streams.emplace(stream_id, StreamState{session_id, domain_id});
    session_it->second.stream_ids.insert(stream_id);
    g_events.push_back(record);
    *streamOut = reinterpret_cast<nvtxwStreamHandle_t>(stream_id);
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t StreamClose(nvtxwStreamHandle_t stream)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    const uint64_t stream_id = HandleId(stream);
    const auto stream_it = g_streams.find(stream_id);
    if (stream_it == g_streams.end())
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::StreamClose);
    record.session_id = stream_it->second.session_id;
    record.domain_id = stream_it->second.domain_id;
    record.stream_id = stream_id;
    const auto session_it = g_sessions.find(stream_it->second.session_id);
    if (session_it != g_sessions.end())
    {
        session_it->second.stream_ids.erase(stream_id);
    }
    g_streams.erase(stream_it);
    g_events.push_back(record);
    return NVTXW_RESULT_SUCCESS;
}

bool CapturePayload(
    const nvtxPayloadData_t& payload,
    uint64_t& schemaIdOut,
    uint64_t& sizeOut,
    uint8_t (&bytesOut)[EventBufferSize])
{
    schemaIdOut = payload.schemaId;
    sizeOut = payload.size;
    return CopyBytes(bytesOut, payload.payload, payload.size);
}

nvtxwResultCode_t EventWrite(
    nvtxwStreamHandle_t stream,
    const nvtxPayloadData_t* payloads,
    size_t payloadCount)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    if (payloads == nullptr || payloadCount == 0 || payloadCount > 2)
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::EventWrite);
    if (!FillStreamOwnership(record, HandleId(stream)))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    record.payload_count = payloadCount;
    if (!CapturePayload(
            payloads[0], record.payload0_schema_id, record.payload0_size,
            record.payload0))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    if (payloadCount == 2 &&
        !CapturePayload(
            payloads[1], record.payload1_schema_id, record.payload1_size,
            record.payload1))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    g_events.push_back(record);
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t EventBatchWrite(
    nvtxwStreamHandle_t stream, const nvtxEventBatch_t* batch)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    if (batch == nullptr)
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::EventBatchWrite);
    if (!FillStreamOwnership(record, HandleId(stream)) ||
        !CopyBytes(record.event_batch_data, batch->events, batch->size) ||
        !CopyBytes(
            record.event_batch_flex_data, batch->flexData,
            batch->flexDataSize))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    record.event_batch_schema_id = batch->eventSchemaId;
    record.event_batch_size = batch->size;
    record.event_batch_scope = batch->scope;
    record.event_batch_flags = batch->flags;
    record.event_batch_flex_size = batch->flexDataSize;
    record.event_batch_flex_offset = batch->flexDataOffset;
    g_events.push_back(record);
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t CounterWrite(
    nvtxwStreamHandle_t stream,
    int64_t timestamp,
    uint64_t counterId,
    const void* data,
    size_t size)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::CounterWrite);
    if (!FillStreamOwnership(record, HandleId(stream)) ||
        !CopyBytes(record.counter_data, data, size))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    record.counter_timestamp = timestamp;
    record.counter_id = counterId;
    record.counter_data_size = size;
    g_events.push_back(record);
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t CounterNoValueWrite(
    nvtxwStreamHandle_t stream,
    int64_t timestamp,
    uint64_t counterId,
    uint8_t reason)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::CounterNoValueWrite);
    if (!FillStreamOwnership(record, HandleId(stream)))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    record.counter_timestamp = timestamp;
    record.counter_id = counterId;
    record.counter_no_value_reason = reason;
    g_events.push_back(record);
    return NVTXW_RESULT_SUCCESS;
}

nvtxwResultCode_t CounterBatchWrite(
    nvtxwStreamHandle_t stream, const nvtxCounterBatch_t* batch)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    if (batch == nullptr ||
        batch->timestampsSize > TimestampBufferCount * sizeof(int64_t) ||
        batch->timestampsSize % sizeof(int64_t) != 0)
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    WriterEventRecord record{};
    record.kind = static_cast<uint32_t>(WriterEventKind::CounterBatchWrite);
    if (!FillStreamOwnership(record, HandleId(stream)) ||
        !CopyBytes(record.counter_data, batch->counters, batch->countersSize) ||
        (batch->timestampsSize != 0 && batch->timestamps == nullptr))
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    record.counter_id = batch->counterId;
    record.counter_data_size = batch->countersSize;
    record.counter_batch_flags = batch->flags;
    record.counter_timestamps_size = batch->timestampsSize;
    if (batch->timestampsSize != 0)
    {
        std::memcpy(
            record.counter_timestamps, batch->timestamps,
            batch->timestampsSize);
    }
    g_events.push_back(record);
    return NVTXW_RESULT_SUCCESS;
}

/* Unsupported by the current Python surface, but kept non-NULL. */
nvtxwResultCode_t ResourceRegister(
    nvtxDomainHandle_t, const nvtxResourceAttributes_t*)
{
    return NVTXW_RESULT_NOT_SUPPORTED;
}

nvtxwResultCode_t EnumRegister(
    nvtxDomainHandle_t, const nvtxPayloadEnumAttr_t*, uint64_t*)
{
    return NVTXW_RESULT_NOT_SUPPORTED;
}

nvtxwResultCode_t TimeDomainRegister(
    nvtxDomainHandle_t, const nvtxTimeDomainAttr_t*, uint64_t*)
{
    return NVTXW_RESULT_NOT_SUPPORTED;
}

nvtxwResultCode_t TimeSyncPointWrite(
    nvtxwStreamHandle_t, uint64_t, uint64_t, int64_t, int64_t)
{
    return NVTXW_RESULT_NOT_SUPPORTED;
}

nvtxwResultCode_t TimeSyncPointTableWrite(
    nvtxwStreamHandle_t, uint64_t, uint64_t, const nvtxSyncPoint_t*, size_t)
{
    return NVTXW_RESULT_NOT_SUPPORTED;
}

nvtxwResultCode_t TimestampConversionFactorWrite(
    nvtxwStreamHandle_t, uint64_t, uint64_t, double, int64_t, int64_t)
{
    return NVTXW_RESULT_NOT_SUPPORTED;
}

nvtxwInterface_v2_t g_interface{};

void PopulateInterface()
{
    g_interface.SessionBegin = SessionBegin;
    g_interface.SessionEnd = SessionEnd;
    g_interface.DomainRegister = DomainRegister;
    g_interface.CategoryRegister = CategoryRegister;
    g_interface.StringRegister = StringRegister;
    g_interface.ResourceRegister = ResourceRegister;
    g_interface.ScopeRegister = ScopeRegister;
    g_interface.SchemaRegister = SchemaRegister;
    g_interface.EnumRegister = EnumRegister;
    g_interface.CounterRegister = CounterRegister;
    g_interface.TimeDomainRegister = TimeDomainRegister;
    g_interface.StreamOpen = StreamOpen;
    g_interface.StreamClose = StreamClose;
    g_interface.EventWrite = EventWrite;
    g_interface.EventBatchWrite = EventBatchWrite;
    g_interface.CounterWrite = CounterWrite;
    g_interface.CounterNoValueWrite = CounterNoValueWrite;
    g_interface.CounterBatchWrite = CounterBatchWrite;
    g_interface.TimeSyncPointWrite = TimeSyncPointWrite;
    g_interface.TimeSyncPointTableWrite = TimeSyncPointTableWrite;
    g_interface.TimestampConversionFactorWrite =
        TimestampConversionFactorWrite;
}
}  // namespace

extern "C"
{

EXPORT_SYMBOL nvtxwResultCode_t nvtxwGetInterface(
    nvtxwInterfaceVersion_t version, const void** ifaceOut)
{
    RETURN_IF_NEXT_RESULT_FAIL();
    if (ifaceOut == nullptr)
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    if (version != NVTXW_INTERFACE_VERSION)
    {
        return NVTXW_RESULT_INTERFACE_VERSION_NOT_SUPPORTED;
    }
    PopulateInterface();
    *ifaceOut = &g_interface;
    return NVTXW_RESULT_SUCCESS;
}

#undef RETURN_IF_NEXT_RESULT_FAIL

EXPORT_SYMBOL void nvtxwFinalize()
{
    ++g_finalizeCount;
    g_sessions.clear();
    g_domain_sessions.clear();
    g_streams.clear();
    g_nextResult = NVTXW_RESULT_SUCCESS;
    /* Preserve g_events: pytest drains records after backend teardown. */
}

EXPORT_SYMBOL void writer_set_next_result(nvtxwResultCode_t result)
{
    g_nextResult = result;
}

EXPORT_SYMBOL void writer_reset_next_result()
{
    g_nextResult = NVTXW_RESULT_SUCCESS;
}

EXPORT_SYMBOL uint64_t writer_finalize_count()
{
    return g_finalizeCount;
}

EXPORT_SYMBOL void writer_reset_finalize_count()
{
    g_finalizeCount = 0;
}

EXPORT_SYMBOL size_t writer_event_record_size()
{
    return sizeof(WriterEventRecord);
}

EXPORT_SYMBOL bool read_event(
    WriterEventRecord* out,
    size_t outSize)
{
    if (out == nullptr || outSize != sizeof(WriterEventRecord))
    {
        return false;
    }
    if (g_events.empty())
    {
        return false;
    }
    std::memcpy(out, &g_events.front(), sizeof(WriterEventRecord));
    g_events.pop_front();
    return true;
}

#ifdef _WIN32
/* setuptools builds this as a Python extension on Windows and requires
 * PyInit_* even though tests load the backend with ctypes.CDLL(). */
EXPORT_SYMBOL PyObject* PyInit_nvtxw_test_backend(void)
{
    return nullptr;
}
#endif

}  // extern "C"
