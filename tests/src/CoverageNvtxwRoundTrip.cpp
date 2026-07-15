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
 * Round-trip (semantic) coverage for the NVTXW event helpers.
 *
 * The other NVTXW tests verify the bytes the helpers hand to the backend.  This
 * test goes one step further: it feeds the helper-produced schemas and payloads
 * through the standalone NVTX payload parser from tools/sample-injection -- a
 * real consumer -- and asserts the decoded text/JSON matches what was written.
 * This proves that what the helpers emit is actually decodable end to end.
 *
 * The backend here is the parser itself: SchemaRegister forwards to the parser's
 * schema registry and EventWrite decodes the payloads into a formatted string.
 * Because the helpers use whatever schema IDs the backend returns, the IDs the
 * payloads reference always match the registered schemas.
 */

#include <nvtx3/nvToolsExtPayload.h>
#include <nvtx3/nvToolsExtCounters.h>
#include <nvtxw3/nvtxw3_helpers.h>

#include "NvtxPayloadInjectionAdapter.h"
#include "NvtxPayloadRegistry.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

NvtxPayloadRegistry* g_registry = nullptr;
std::vector<std::string>* g_decoded = nullptr;

bool Contains(const std::string& haystack, const char* needle)
{
    return haystack.find(needle) != std::string::npos;
}

} // namespace

extern "C" {

/* Backend SchemaRegister: convert + store the schema in the parser registry.
 * The NVTXW helpers always set the STATIC_SIZE field bit, even for the dynamic
 * UTF-8 message schema whose static size is 0.  The sample-injection adapter
 * rejects STATIC_SIZE with a zero size, so drop that bit for dynamic schemas. */
static nvtxwResultCode_t RtSchemaRegister(
    nvtxDomainHandle_t /*domain*/, const nvtxPayloadSchemaAttr_t* attr, uint64_t* schemaIdOut)
{
    if (!attr || !schemaIdOut)
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }

    nvtxPayloadSchemaAttr_t fixed = *attr;
    if ((fixed.fieldMask & NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE) != 0 &&
        fixed.payloadStaticSize == 0)
    {
        fixed.fieldMask &=
            ~static_cast<uint64_t>(NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE);
    }

    const uint64_t id = NvtxPayloadInjection::RegisterSchema(*g_registry, &fixed);
    if (id == 0)
    {
        return NVTXW_RESULT_FAILED;
    }
    *schemaIdOut = id;
    return NVTXW_RESULT_SUCCESS;
}

/* Backend EventWrite: decode the event's payloads and record the text form. */
static nvtxwResultCode_t RtEventWrite(
    nvtxwStreamHandle_t /*stream*/, const nvtxPayloadData_t* payloads, size_t payloadCount)
{
    if (!payloads || payloadCount == 0)
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }
    g_decoded->push_back(NvtxPayloadInjection::DescribePayloads(
        *g_registry, payloads, payloadCount, PayloadFormat::Text));
    return NVTXW_RESULT_SUCCESS;
}

NVTX_DYNAMIC_EXPORT
extern int RunTest(int argc, const char** argv);

} // extern "C"

NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    (void)argc;
    (void)argv;

    const char* expectedMsg = "roundtrip";

    std::unordered_map<nvtxStringHandle_t, std::string> registeredStrings;
    NvtxPayloadRegistry registry(registeredStrings);
    std::vector<std::string> decoded;
    g_registry = &registry;
    g_decoded = &decoded;

    /* Minimal core interface: only the entry points the event helpers use. */
    nvtxwInterface_t iface;
    memset(&iface, 0, sizeof(iface));
    iface.SchemaRegister = RtSchemaRegister;
    iface.EventWrite = RtEventWrite;

    /* The helpers only need non-NULL, never-dereferenced domain/stream handles. */
    int domainStorage = 0;
    int streamStorage = 0;
    nvtxDomainHandle_t domain = reinterpret_cast<nvtxDomainHandle_t>(&domainStorage);
    nvtxwStreamHandle_t stream = reinterpret_cast<nvtxwStreamHandle_t>(&streamStorage);

    nvtxwEventHelperSchemaIds_t schemaIds;
    if (nvtxwEventSchemasRegister(
            &iface, domain, NVTXW_EVENT_HELPER_SCHEMA_ALL, &schemaIds) !=
        NVTXW_RESULT_SUCCESS)
    {
        return 1;
    }

    nvtxwEventWriter_t writer;
    if (nvtxwEventWriterInit(&writer, &iface, stream, &schemaIds) !=
        NVTXW_RESULT_SUCCESS)
    {
        return 1;
    }

    nvtxwEventAttributesUtf8_t utf8Attr = {};
    utf8Attr.color = 0xFF00FF00u;
    utf8Attr.category = 7;
    utf8Attr.message = expectedMsg;

    /* Mark: one header payload (timestamp/color/category) + one message payload. */
    if (nvtxwMarkWriteUtf8(&writer, 100, utf8Attr) !=
        NVTXW_RESULT_SUCCESS)
    {
        return 3;
    }
    if (decoded.size() != 1)
    {
        return 4;
    }
    if (!Contains(decoded[0], "count=2"))
    {
        return 5;
    }
    if (!Contains(decoded[0], "timestamp=100"))
    {
        return 6;
    }
    if (!Contains(decoded[0], "category=7"))
    {
        return 7;
    }
    /* The zero-terminated UTF-8 message decodes as a char array, so it is
     * rendered with array brackets by the text formatter. */
    if (!Contains(decoded[0], "message=[roundtrip]"))
    {
        return 8;
    }

    /* Push/pop range: header carries both begin and end timestamps. */
    if (nvtxwRangePushPopWriteUtf8(&writer, 200, 300, utf8Attr) !=
        NVTXW_RESULT_SUCCESS)
    {
        return 9;
    }
    if (decoded.size() != 2)
    {
        return 10;
    }
    if (!Contains(decoded[1], "timestampBegin=200"))
    {
        return 11;
    }
    if (!Contains(decoded[1], "timestampEnd=300"))
    {
        return 12;
    }
    if (!Contains(decoded[1], "message=[roundtrip]"))
    {
        return 13;
    }

    /* Range pop: a single header payload with no message. */
    if (nvtxwRangePopWrite(&writer, 400) != NVTXW_RESULT_SUCCESS)
    {
        return 14;
    }
    if (decoded.size() != 3)
    {
        return 15;
    }
    if (!Contains(decoded[2], "count=1"))
    {
        return 16;
    }
    if (!Contains(decoded[2], "timestamp=400"))
    {
        return 17;
    }

    /* An explicit message length transports exactly that many bytes: sending the
     * length of "roundtrip" from a longer buffer must decode to just "roundtrip". */
    utf8Attr.message = "roundtrip-and-more";
    utf8Attr.messageLength = static_cast<uint32_t>(strlen(expectedMsg));
    if (nvtxwMarkWriteUtf8(&writer, 500, utf8Attr) !=
        NVTXW_RESULT_SUCCESS)
    {
        return 18;
    }
    utf8Attr.messageLength = 0;
    if (decoded.size() != 4)
    {
        return 19;
    }
    if (!Contains(decoded[3], "message=[roundtrip]"))
    {
        return 20;
    }
    if (Contains(decoded[3], "roundtrip-and-more"))
    {
        return 21;
    }

    /* The same payloads must also decode through the JSON formatter. */
    {
        nvtxwEventUtf8Payload_t header;
        memset(&header, 0, sizeof(header));
        header.timestamp = 600;
        header.attr.color = 0xFF00FF00u;
        header.attr.category = 7;

        nvtxPayloadData_t payloads[2];
        payloads[0].schemaId = schemaIds.markUtf8;
        payloads[0].size = sizeof(header);
        payloads[0].payload = &header;
        payloads[1].schemaId = schemaIds.eventMessageUtf8;
        payloads[1].size = strlen(expectedMsg);
        payloads[1].payload = expectedMsg;

        const std::string json = NvtxPayloadInjection::DescribePayloads(
            registry, payloads, 2, PayloadFormat::Json);
        if (!Contains(json, "roundtrip"))
        {
            return 22;
        }
        if (!Contains(json, "600"))
        {
            return 23;
        }
    }

    g_registry = nullptr;
    g_decoded = nullptr;
    return 0;
}
