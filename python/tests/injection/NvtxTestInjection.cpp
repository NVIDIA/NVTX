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

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <nvtx3/nvToolsExt.h>
#include <Python.h>

#ifdef _WIN32
#define EXPORT_SYMBOL   __declspec(dllexport)
#else
#define EXPORT_SYMBOL __attribute__((visibility("default")))
#endif

namespace
{
constexpr char DefaultDomain[] = "Default";

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
};

struct EventRecord
{
    uint32_t kind;
    const char* domain{nullptr};
    int32_t message_type{NVTX_MESSAGE_UNKNOWN};
    const char* message{nullptr};
    uint32_t category{0};
    uint64_t range_id{0};
    uint32_t color{0};
    int32_t payload_type{0};
    int64_t payload_i64{0};
    double payload_f64{0.0};
};

// Queue of the recorded events. Consumed by `read_event()` (called by Python tests).
std::deque<EventRecord> g_events;

// Atomic counter for the range IDs.
std::atomic<uint64_t> g_rangeId{1};

// Registered domains. nvtxDomainHandle_t is the index in the vector
// (1-based because 0 is reserved for the default domain)
std::vector<std::string> g_registeredDomains;

// Registered strings.
std::vector<std::string> g_registeredStrings;

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
    g_events.push_back(
        {
            static_cast<uint32_t>(kind),
            domain,
            message == nullptr ? NVTX_MESSAGE_UNKNOWN : NVTX_MESSAGE_TYPE_REGISTERED,
            message,
            category,
            rangeId,
        }
    );
}

void RecordEventFromAttrib(
    EventKind kind, const char* domain, const nvtxEventAttributes_t* attrib)
{
    ValidateEventAttributes(attrib);
    g_events.push_back(
        {
            static_cast<uint32_t>(kind),
            domain,
            attrib->messageType,
            ResolveMessage(attrib),
            attrib->category,
            0,  // rangeId
            attrib->color,
            attrib->payloadType,
            attrib->payloadType == NVTX_PAYLOAD_TYPE_INT64 ? attrib->payload.llValue : 0,
            attrib->payloadType == NVTX_PAYLOAD_TYPE_DOUBLE ? attrib->payload.dValue : 0.0,
        }
    );
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
