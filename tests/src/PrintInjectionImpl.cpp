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

#include "PrintInjectionImpl.h"

#include <stdio.h>

/* Use a compiler option to define this prefix string to a custom value */
#ifndef INJECTION_PRINT_PREFIX
#define INJECTION_PRINT_PREFIX "inj"
#endif

#if defined(NVTX_INJECTION_TEST_QUIET)
#define LOG_INFO(...)
#define LOG_ERROR(...)
#else
#define LOG_INFO(...)  fprintf(stdout, "[" INJECTION_PRINT_PREFIX "] " __VA_ARGS__)
#define LOG_ERROR(...) fprintf(stdout, "[" INJECTION_PRINT_PREFIX "] ERROR: " __VA_ARGS__)
#endif

/* Implementations of NVTX functions to attach to client */

#define NVTX_TOOL_ATTACHED_UNUSED_RANGE_ID (static_cast<nvtxRangeId_t>(-1LL))
#define NVTX_TOOL_ATTACHED_UNUSED_PUSH_POP_ID (static_cast<int>(-1))
#define NVTX_TOOL_ATTACHED_UNUSED_DOMAIN_HANDLE (reinterpret_cast<nvtxDomainHandle_t>(-1LL))
#define NVTX_TOOL_ATTACHED_UNUSED_STRING_HANDLE (reinterpret_cast<nvtxStringHandle_t>(-1LL))

/* NVTX_CB_MODULE_CORE */

static void NVTX_API HandleMarkA(const char* /*str*/)
{
    LOG_INFO("%s\n", "nvtxMarkA");
}

static int NVTX_API HandleRangePushA(const char* /*str*/)
{
    LOG_INFO("%s\n", "nvtxRangePushA");
    return NVTX_TOOL_ATTACHED_UNUSED_PUSH_POP_ID;
}

static int NVTX_API HandleRangePop()
{
    LOG_INFO("%s\n", "nvtxRangePop");
    return NVTX_TOOL_ATTACHED_UNUSED_PUSH_POP_ID;
}

/* NVTX_CB_MODULE_CORE2 */

static void NVTX_API HandleDomainMarkEx(nvtxDomainHandle_t /*domain*/, const nvtxEventAttributes_t* /*eventAttrib*/)
{
    LOG_INFO("%s\n", "nvtxDomainMarkEx");
}

static nvtxRangeId_t NVTX_API HandleDomainRangeStartEx(nvtxDomainHandle_t /*domain*/, const nvtxEventAttributes_t* /*eventAttrib*/)
{
    LOG_INFO("%s\n", "nvtxDomainRangeStartEx");
    return NVTX_TOOL_ATTACHED_UNUSED_RANGE_ID;
}

static void NVTX_API HandleDomainRangeEnd(nvtxDomainHandle_t /*domain*/, nvtxRangeId_t /*id*/)
{
    LOG_INFO("%s\n", "nvtxDomainRangeEnd");
}

static int NVTX_API HandleDomainRangePushEx(nvtxDomainHandle_t /*domain*/, const nvtxEventAttributes_t* /*eventAttrib*/)
{
    LOG_INFO("%s\n", "nvtxDomainRangePushEx");
    return NVTX_TOOL_ATTACHED_UNUSED_PUSH_POP_ID;
}

static int NVTX_API HandleDomainRangePop(nvtxDomainHandle_t /*domain*/)
{
    LOG_INFO("%s\n", "nvtxDomainRangePop");
    return NVTX_TOOL_ATTACHED_UNUSED_PUSH_POP_ID;
}

static nvtxStringHandle_t NVTX_API HandleDomainRegisterStringA(nvtxDomainHandle_t /*domain*/, const char* /*string*/)
{
    LOG_INFO("%s\n", "nvtxDomainRegisterStringA");
    return NVTX_TOOL_ATTACHED_UNUSED_STRING_HANDLE;
}

static nvtxDomainHandle_t NVTX_API HandleDomainCreateA(const char* /*name*/)
{
    LOG_INFO("%s\n", "nvtxDomainCreateA");
    return NVTX_TOOL_ATTACHED_UNUSED_DOMAIN_HANDLE;
}

static void NVTX_API HandleDomainDestroy(nvtxDomainHandle_t /*domain*/)
{
    LOG_INFO("%s\n", "nvtxDomainDestroy");
}

static void NVTX_API HandleInitialize(const void* /*reserved*/)
{
    LOG_INFO("%s\n", "nvtxInitialize");
}

/* To simplify building this injection in various ways to test dynamic/static/preinject
*  modes, make the initialization function static so it can't be used externally.  Then
*  provide individual functions/symbols to expose it based on the #defines used. */
int NVTX_API InitializeInjectionNvtx2Internal(NvtxGetExportTableFunc_t getExportTable)
{
    uint32_t version = 0;
    const NvtxExportTableVersionInfo* pVersionInfo;
    const NvtxExportTableCallbacks* pCallbacks;
    NvtxFunctionTable table = nullptr;
    unsigned int size = 0;
    int success;
    unsigned int highestIdUsed;

    pVersionInfo = static_cast<const NvtxExportTableVersionInfo*>(getExportTable(NVTX_ETID_VERSIONINFO));
    if (pVersionInfo)
    {
        if (pVersionInfo->struct_size < sizeof(*pVersionInfo))
        {
            LOG_ERROR(
                "(init v2) NvtxExportTableVersionInfo structure size is %d, expected %d!\n",
                static_cast<int>(pVersionInfo->struct_size),
                static_cast<int>(sizeof(*pVersionInfo)));
            return 0;
        }

        version = pVersionInfo->version;
        if (version < 2)
        {
            LOG_ERROR(
                "(init v2) client's NVTX version is %d, expected 2+\n",
                static_cast<int>(version));
            return 0;
        }
    }

    LOG_INFO("---- InitializeInjectionNvtx2 called from client's NVTX v%u\n", version);

    pCallbacks = static_cast<const NvtxExportTableCallbacks*>(getExportTable(NVTX_ETID_CALLBACKS));
    if (!pCallbacks)
    {
        LOG_ERROR("(init v2) NVTX_ETID_CALLBACKS is not supported.\n");
        return 0;
    }

    if (pCallbacks->struct_size < sizeof(*pCallbacks))
    {
        LOG_ERROR("(init v2) NvtxExportTableCallbacks structure size is %d, expected %d!\n",
            static_cast<int>(pCallbacks->struct_size),
            static_cast<int>(sizeof(*pCallbacks)));
        return 0;
    }

    {
        table = nullptr;
        size = 0;
        success = pCallbacks->GetModuleFunctionTable(NVTX_CB_MODULE_CORE, &table, &size);
        if (!success || !table)
        {
            LOG_ERROR("(init v2) NVTX_CB_MODULE_CORE is not supported.\n");
            return 0;
        }

        /* Ensure client's table is new enough to support the function pointers we want to register */
        highestIdUsed = NVTX_CBID_CORE_RangePop; /* Can auto-detect this in C++ */
        if (size <= highestIdUsed)
        {
            LOG_ERROR("(init v2) Client's function pointer table size is %d, and we need to assign to table[%d].\n",
                static_cast<int>(size),
                static_cast<int>(highestIdUsed));
            return 0;
        }

        *table[NVTX_CBID_CORE_MarkA     ] = reinterpret_cast<NvtxFunctionPointer>(HandleMarkA)     ;
        *table[NVTX_CBID_CORE_RangePushA] = reinterpret_cast<NvtxFunctionPointer>(HandleRangePushA);
        *table[NVTX_CBID_CORE_RangePop  ] = reinterpret_cast<NvtxFunctionPointer>(HandleRangePop)  ;
    }

    {
        table = nullptr;
        size = 0;
        success = pCallbacks->GetModuleFunctionTable(NVTX_CB_MODULE_CORE2, &table, &size);
        if (!success || !table)
        {
            LOG_ERROR("(init v2) NVTX_CB_MODULE_CORE2 is not supported.\n");
            return 0;
        }

        /* Ensure client's table is new enough to support the function pointers we want to register */
        highestIdUsed = NVTX_CBID_CORE2_Initialize; /* Can auto-detect this in C++ */
        if (size <= highestIdUsed)
        {
            LOG_ERROR("(init v2) Client's function pointer table size is %d, and we need to assign to table[%d].\n",
                static_cast<int>(size),
                static_cast<int>(highestIdUsed));
            return 0;
        }

        *table[NVTX_CBID_CORE2_DomainMarkEx         ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainMarkEx)         ;
        *table[NVTX_CBID_CORE2_DomainRangeStartEx   ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainRangeStartEx)   ;
        *table[NVTX_CBID_CORE2_DomainRangeEnd       ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainRangeEnd)       ;
        *table[NVTX_CBID_CORE2_DomainRangePushEx    ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainRangePushEx)    ;
        *table[NVTX_CBID_CORE2_DomainRangePop       ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainRangePop)       ;
        *table[NVTX_CBID_CORE2_DomainRegisterStringA] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainRegisterStringA);
        *table[NVTX_CBID_CORE2_DomainCreateA        ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainCreateA)        ;
        *table[NVTX_CBID_CORE2_DomainDestroy        ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainDestroy)        ;
        *table[NVTX_CBID_CORE2_Initialize           ] = reinterpret_cast<NvtxFunctionPointer>(HandleInitialize)           ;
    }

    return 1;
}
