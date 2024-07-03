/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 * See LICENSE.txt for license information.
 */

#include "SelfInjection.h"
#include "DllHelper.h"
#include <stdio.h>

#if defined(NVTX_INJECTION_TEST_QUIET)
#define LOG_ERROR(...)
#else
#define LOG_ERROR(...) do { fprintf(stderr, "  [inj] ERROR: " __VA_ARGS__); } while (0)
#endif

Callbacks g_callbacks;

namespace {

/* NVTX_CB_MODULE_CORE */
void          NVTX_API HandleMarkEx       (const nvtxEventAttributes_t* eventAttrib) {        g_callbacks.MarkEx       (eventAttrib); }
void          NVTX_API HandleMarkA        (const char* str                         ) {        g_callbacks.MarkA        (str        ); }
void          NVTX_API HandleMarkW        (const wchar_t* str                      ) {        g_callbacks.MarkW        (str        ); }
nvtxRangeId_t NVTX_API HandleRangeStartEx (const nvtxEventAttributes_t* eventAttrib) { return g_callbacks.RangeStartEx (eventAttrib); }
nvtxRangeId_t NVTX_API HandleRangeStartA  (const char* str                         ) { return g_callbacks.RangeStartA  (str        ); }
nvtxRangeId_t NVTX_API HandleRangeStartW  (const wchar_t* str                      ) { return g_callbacks.RangeStartW  (str        ); }
void          NVTX_API HandleRangeEnd     (nvtxRangeId_t id                        ) {        g_callbacks.RangeEnd     (id         ); }
int           NVTX_API HandleRangePushEx  (const nvtxEventAttributes_t* eventAttrib) { return g_callbacks.RangePushEx  (eventAttrib); }
int           NVTX_API HandleRangePushA   (const char* str                         ) { return g_callbacks.RangePushA   (str        ); }
int           NVTX_API HandleRangePushW   (const wchar_t* str                      ) { return g_callbacks.RangePushW   (str        ); }
int           NVTX_API HandleRangePop     (                                        ) { return g_callbacks.RangePop     (           ); }
void          NVTX_API HandleNameCategoryA(uint32_t id, const char* str            ) {        g_callbacks.NameCategoryA(id, str    ); }
void          NVTX_API HandleNameCategoryW(uint32_t id, const wchar_t* str         ) {        g_callbacks.NameCategoryW(id, str    ); }
void          NVTX_API HandleNameOsThreadA(uint32_t id, const char* str            ) {        g_callbacks.NameOsThreadA(id, str    ); }
void          NVTX_API HandleNameOsThreadW(uint32_t id, const wchar_t* str         ) {        g_callbacks.NameOsThreadW(id, str    ); }

/* NVTX_CB_MODULE_CORE2 */
void                 NVTX_API HandleDomainMarkEx         (nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib) {        g_callbacks.DomainMarkEx         (domain, eventAttrib); }
nvtxRangeId_t        NVTX_API HandleDomainRangeStartEx   (nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib) { return g_callbacks.DomainRangeStartEx   (domain, eventAttrib); }
void                 NVTX_API HandleDomainRangeEnd       (nvtxDomainHandle_t domain, nvtxRangeId_t id                        ) {        g_callbacks.DomainRangeEnd       (domain, id         ); }
int                  NVTX_API HandleDomainRangePushEx    (nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib) { return g_callbacks.DomainRangePushEx    (domain, eventAttrib); }
int                  NVTX_API HandleDomainRangePop       (nvtxDomainHandle_t domain                                          ) { return g_callbacks.DomainRangePop       (domain             ); }
nvtxResourceHandle_t NVTX_API HandleDomainResourceCreate (nvtxDomainHandle_t domain, nvtxResourceAttributes_t* attr          ) { return g_callbacks.DomainResourceCreate (domain, attr       ); }
void                 NVTX_API HandleDomainResourceDestroy(nvtxResourceHandle_t attr                                          ) {        g_callbacks.DomainResourceDestroy(attr               ); }
void                 NVTX_API HandleDomainNameCategoryA  (nvtxDomainHandle_t domain, uint32_t id, const char* str            ) {        g_callbacks.DomainNameCategoryA  (domain, id, str    ); }
void                 NVTX_API HandleDomainNameCategoryW  (nvtxDomainHandle_t domain, uint32_t id, const wchar_t* str         ) {        g_callbacks.DomainNameCategoryW  (domain, id, str    ); }
nvtxStringHandle_t   NVTX_API HandleDomainRegisterStringA(nvtxDomainHandle_t domain, const char* str                         ) { return g_callbacks.DomainRegisterStringA(domain, str        ); }
nvtxStringHandle_t   NVTX_API HandleDomainRegisterStringW(nvtxDomainHandle_t domain, const wchar_t* str                      ) { return g_callbacks.DomainRegisterStringW(domain, str        ); }
nvtxDomainHandle_t   NVTX_API HandleDomainCreateA        (const char* name                                                   ) { return g_callbacks.DomainCreateA        (name               ); }
nvtxDomainHandle_t   NVTX_API HandleDomainCreateW        (const wchar_t* name                                                ) { return g_callbacks.DomainCreateW        (name               ); }
void                 NVTX_API HandleDomainDestroy        (nvtxDomainHandle_t domain                                          ) {        g_callbacks.DomainDestroy        (domain             ); }
void                 NVTX_API HandleInitialize           (const void* reserved                                               ) {        g_callbacks.Initialize           (reserved           ); }

}

extern "C" DLL_EXPORT
int NVTX_API InitializeInjectionNvtx2(NvtxGetExportTableFunc_t getExportTable)
{
    uint32_t version = 0;
    const NvtxExportTableVersionInfo* pVersionInfo =
        (const NvtxExportTableVersionInfo*)getExportTable(NVTX_ETID_VERSIONINFO);
    if (pVersionInfo)
    {
        if (pVersionInfo->struct_size < sizeof(*pVersionInfo))
        {
            LOG_ERROR(
                "(init v2) NvtxExportTableVersionInfo structure size is %d, expected %d!\n",
                (int)pVersionInfo->struct_size,
                (int)sizeof(*pVersionInfo));
            g_callbacks.Load(0);
            return 0;
        }

        version = pVersionInfo->version;
        if (version < 2)
        {
            LOG_ERROR(
                "(init v2) client's NVTX version is %d, expected 2+\n",
                (int)version);
            g_callbacks.Load(0);
            return 0;
        }
    }

    const NvtxExportTableCallbacks* pCallbacks =
        (const NvtxExportTableCallbacks*)getExportTable(NVTX_ETID_CALLBACKS);
    if (!pCallbacks)
    {
        LOG_ERROR("(init v2) NVTX_ETID_CALLBACKS is not supported.\n");
        g_callbacks.Load(0);
        return 0;
    }

    if (pCallbacks->struct_size < sizeof(*pCallbacks))
    {
        LOG_ERROR("(init v2) NvtxExportTableCallbacks structure size is %d, expected %d!\n",
            (int)pCallbacks->struct_size,
            (int)sizeof(*pCallbacks));
        g_callbacks.Load(0);
        return 0;
    }

    {
        NvtxFunctionTable table = 0;
        unsigned int size = 0;
        int success = pCallbacks->GetModuleFunctionTable(NVTX_CB_MODULE_CORE, &table, &size);
        if (!success || !table)
        {
            LOG_ERROR("(init v2) NVTX_CB_MODULE_CORE is not supported.\n");
            g_callbacks.Load(0);
            return 0;
        }

        /* Ensure client's table is new enough to support the function pointers we want to register */
        unsigned int highestIdUsed = NVTX_CBID_CORE_RangePop; /* Can auto-detect this in C++ */
        if (size <= highestIdUsed)
        {
            LOG_ERROR("(init v2) Client's function pointer table size is %d, and we need to assign to table[%d].\n",
                (int)size,
                (int)highestIdUsed);
            g_callbacks.Load(0);
            return 0;
        }

        *table[NVTX_CBID_CORE_MarkEx       ] = (NvtxFunctionPointer)HandleMarkEx       ;
        *table[NVTX_CBID_CORE_MarkA        ] = (NvtxFunctionPointer)HandleMarkA        ;
        *table[NVTX_CBID_CORE_MarkW        ] = (NvtxFunctionPointer)HandleMarkW        ;
        *table[NVTX_CBID_CORE_RangeStartEx ] = (NvtxFunctionPointer)HandleRangeStartEx ;
        *table[NVTX_CBID_CORE_RangeStartA  ] = (NvtxFunctionPointer)HandleRangeStartA  ;
        *table[NVTX_CBID_CORE_RangeStartW  ] = (NvtxFunctionPointer)HandleRangeStartW  ;
        *table[NVTX_CBID_CORE_RangeEnd     ] = (NvtxFunctionPointer)HandleRangeEnd     ;
        *table[NVTX_CBID_CORE_RangePushEx  ] = (NvtxFunctionPointer)HandleRangePushEx  ;
        *table[NVTX_CBID_CORE_RangePushA   ] = (NvtxFunctionPointer)HandleRangePushA   ;
        *table[NVTX_CBID_CORE_RangePushW   ] = (NvtxFunctionPointer)HandleRangePushW   ;
        *table[NVTX_CBID_CORE_RangePop     ] = (NvtxFunctionPointer)HandleRangePop     ;
        *table[NVTX_CBID_CORE_NameCategoryA] = (NvtxFunctionPointer)HandleNameCategoryA;
        *table[NVTX_CBID_CORE_NameCategoryW] = (NvtxFunctionPointer)HandleNameCategoryW;
        *table[NVTX_CBID_CORE_NameOsThreadA] = (NvtxFunctionPointer)HandleNameOsThreadA;
        *table[NVTX_CBID_CORE_NameOsThreadW] = (NvtxFunctionPointer)HandleNameOsThreadW;
    }

    {
        NvtxFunctionTable table = 0;
        unsigned int size = 0;
        int success = pCallbacks->GetModuleFunctionTable(NVTX_CB_MODULE_CORE2, &table, &size);
        if (!success || !table)
        {
            LOG_ERROR("(init v2) NVTX_CB_MODULE_CORE2 is not supported.\n");
            g_callbacks.Load(0);
            return 0;
        }

        /* Ensure client's table is new enough to support the function pointers we want to register */
        unsigned int highestIdUsed = NVTX_CBID_CORE2_Initialize; /* Can auto-detect this in C++ */
        if (size <= highestIdUsed)
        {
            LOG_ERROR("(init v2) Client's function pointer table size is %d, and we need to assign to table[%d].\n",
                (int)size,
                (int)highestIdUsed);
            g_callbacks.Load(0);
            return 0;
        }

        *table[NVTX_CBID_CORE2_DomainMarkEx         ] = (NvtxFunctionPointer)HandleDomainMarkEx         ;
        *table[NVTX_CBID_CORE2_DomainRangeStartEx   ] = (NvtxFunctionPointer)HandleDomainRangeStartEx   ;
        *table[NVTX_CBID_CORE2_DomainRangeEnd       ] = (NvtxFunctionPointer)HandleDomainRangeEnd       ;
        *table[NVTX_CBID_CORE2_DomainRangePushEx    ] = (NvtxFunctionPointer)HandleDomainRangePushEx    ;
        *table[NVTX_CBID_CORE2_DomainRangePop       ] = (NvtxFunctionPointer)HandleDomainRangePop       ;
        *table[NVTX_CBID_CORE2_DomainResourceCreate ] = (NvtxFunctionPointer)HandleDomainResourceCreate ;
        *table[NVTX_CBID_CORE2_DomainResourceDestroy] = (NvtxFunctionPointer)HandleDomainResourceDestroy;
        *table[NVTX_CBID_CORE2_DomainNameCategoryA  ] = (NvtxFunctionPointer)HandleDomainNameCategoryA  ;
        *table[NVTX_CBID_CORE2_DomainNameCategoryW  ] = (NvtxFunctionPointer)HandleDomainNameCategoryW  ;
        *table[NVTX_CBID_CORE2_DomainRegisterStringA] = (NvtxFunctionPointer)HandleDomainRegisterStringA;
        *table[NVTX_CBID_CORE2_DomainRegisterStringW] = (NvtxFunctionPointer)HandleDomainRegisterStringW;
        *table[NVTX_CBID_CORE2_DomainCreateA        ] = (NvtxFunctionPointer)HandleDomainCreateA        ;
        *table[NVTX_CBID_CORE2_DomainCreateW        ] = (NvtxFunctionPointer)HandleDomainCreateW        ;
        *table[NVTX_CBID_CORE2_DomainDestroy        ] = (NvtxFunctionPointer)HandleDomainDestroy        ;
        *table[NVTX_CBID_CORE2_Initialize           ] = (NvtxFunctionPointer)HandleInitialize           ;
    }

    g_callbacks.Load(1);
    return 1;
}
