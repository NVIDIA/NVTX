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

#include <stdint.h>
#include <string.h>

#include <nvtx3/nvToolsExtPayload.h>
#include <nvtx3/nvToolsExtPayloadHelper.h>

/*
 * Validate helper-macro generated payload schema entry initialization for
 * entry variants with 3..7 fields.
 */
NVTX_DEFINE_STRUCT_WITH_SCHEMA(testPayload, "TestSchemaEntryFields",
    NVTX_PAYLOAD_ENTRIES(
        (int32_t, v3, TYPE_INT32),
        (int32_t, v4, TYPE_INT32, "entry4"),
        (int32_t, v5, TYPE_INT32, "entry5", "desc5"),
        (int32_t, (v6, 2), TYPE_INT32, "entry6", "desc6", 2),
        (int32_t, (v7, 3), TYPE_INT32, "entry7", "desc7", 3, ARRAY_FIXED_SIZE),
        (int32_t, (v7neutral, 1), TYPE_INT32, NVTX_NULLPTR, NVTX_NULLPTR, 0, UNUSED)
    )
)

#if NVTX_HAVE_CHAR16_CHAR32
NVTX_DEFINE_STRUCT_WITH_SCHEMA(testPayloadChar16Char32, "TestSchemaEntryFieldsChar16Char32",
    NVTX_PAYLOAD_ENTRIES(
        (char32_t, vchar32, TYPE_CHAR32, "entryChar32", "descChar32"),
        (char16_t, vchar16, TYPE_CHAR16, "entryChar16", "descChar16")
    )
)
#endif

#if NVTX_HAVE_CHAR8
NVTX_DEFINE_STRUCT_WITH_SCHEMA(testPayloadChar8, "TestSchemaEntryFieldsChar8",
    NVTX_PAYLOAD_ENTRIES(
        (char8_t, vchar8, TYPE_CHAR8, "entryChar8", "descChar8")
    )
)
#endif

static int check_entry(
    const nvtxPayloadSchemaEntry_t* e,
    uint64_t flags,
    uint64_t type,
    const char* name,
    const char* desc,
    size_t array_detail)
{
    if (e->flags != flags) return 0;
    if (e->type != type) return 0;
    if (((name == NVTX_NULLPTR) != (e->name == NVTX_NULLPTR)) ||
        (name != NVTX_NULLPTR && strcmp(name, e->name) != 0)) return 0;
    if (((desc == NVTX_NULLPTR) != (e->description == NVTX_NULLPTR)) ||
        (desc != NVTX_NULLPTR && strcmp(desc, e->description) != 0)) return 0;
    if (e->arrayOrUnionDetail != array_detail) return 0;
    if (e->semantics != NVTX_NULLPTR) return 0;
    if (e->reserved != NVTX_NULLPTR) return 0;
    return 1;
}

#ifdef __cplusplus
extern "C" {
#endif
NVTX_DYNAMIC_EXPORT
extern int RunTest(int argc, const char** argv);
#ifdef __cplusplus
}
#endif
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    (void)argc;
    (void)argv;

    if (!check_entry(&testPayloadSchema[0], 0, NVTX_PAYLOAD_ENTRY_TYPE_INT32, NVTX_NULLPTR, NVTX_NULLPTR, 0)) return 1;
    if (!check_entry(&testPayloadSchema[1], 0, NVTX_PAYLOAD_ENTRY_TYPE_INT32, "entry4", NVTX_NULLPTR, 0)) return 2;
    if (!check_entry(&testPayloadSchema[2], 0, NVTX_PAYLOAD_ENTRY_TYPE_INT32, "entry5", "desc5", 0)) return 3;
    if (!check_entry(&testPayloadSchema[3], 0, NVTX_PAYLOAD_ENTRY_TYPE_INT32, "entry6", "desc6", 2)) return 4;
    if (!check_entry(
            &testPayloadSchema[4],
            NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE,
            NVTX_PAYLOAD_ENTRY_TYPE_INT32,
            "entry7",
            "desc7",
            3))
        return 5;
    if (!check_entry(
            &testPayloadSchema[5],
            NVTX_PAYLOAD_ENTRY_FLAG_UNUSED,
            NVTX_PAYLOAD_ENTRY_TYPE_INT32,
            NVTX_NULLPTR,
            NVTX_NULLPTR,
            0))
        return 6;

#if NVTX_HAVE_CHAR16_CHAR32
    if (!check_entry(
            &testPayloadChar16Char32Schema[0],
            0,
            NVTX_PAYLOAD_ENTRY_TYPE_CHAR32,
            "entryChar32",
            "descChar32",
            0))
        return 7;
    if (!check_entry(
            &testPayloadChar16Char32Schema[1],
            0,
            NVTX_PAYLOAD_ENTRY_TYPE_CHAR16,
            "entryChar16",
            "descChar16",
            0))
        return 8;
#endif

#if NVTX_HAVE_CHAR8
    if (!check_entry(
            &testPayloadChar8Schema[0],
            0,
            NVTX_PAYLOAD_ENTRY_TYPE_CHAR8,
            "entryChar8",
            "descChar8",
            0))
        return 9;
#endif

    return 0;
}
