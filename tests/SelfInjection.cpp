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

#include "SelfInjection.h"
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

extern "C" NVTX_DYNAMIC_EXPORT
int NVTX_API InitializeInjectionNvtx2(NvtxGetExportTableFunc_t getExportTable);
NVTX_DYNAMIC_EXPORT
int NVTX_API InitializeInjectionNvtx2(NvtxGetExportTableFunc_t getExportTable)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    uint32_t version = 0;
    auto pVersionInfo = static_cast<const NvtxExportTableVersionInfo*>(getExportTable(NVTX_ETID_VERSIONINFO));
    if (pVersionInfo)
    {
        if (pVersionInfo->struct_size < sizeof(*pVersionInfo))
        {
            LOG_ERROR(
                "(init v2) NvtxExportTableVersionInfo structure size is %d, expected %d!\n",
                static_cast<int>(pVersionInfo->struct_size),
                static_cast<int>(sizeof(*pVersionInfo)));
            g_callbacks.Load(0);
            return 0;
        }

        version = pVersionInfo->version;
        if (version < 2)
        {
            LOG_ERROR(
                "(init v2) client's NVTX version is %d, expected 2+\n",
                static_cast<int>(version));
            g_callbacks.Load(0);
            return 0;
        }
    }

    auto pCallbacks = static_cast<const NvtxExportTableCallbacks*>(getExportTable(NVTX_ETID_CALLBACKS));
    if (!pCallbacks)
    {
        LOG_ERROR("(init v2) NVTX_ETID_CALLBACKS is not supported.\n");
        g_callbacks.Load(0);
        return 0;
    }

    if (pCallbacks->struct_size < sizeof(*pCallbacks))
    {
        LOG_ERROR("(init v2) NvtxExportTableCallbacks structure size is %d, expected %d!\n",
            static_cast<int>(pCallbacks->struct_size),
            static_cast<int>(sizeof(*pCallbacks)));
        g_callbacks.Load(0);
        return 0;
    }

    {
        NvtxFunctionTable table = nullptr;
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
                static_cast<int>(size),
                static_cast<int>(highestIdUsed));
            g_callbacks.Load(0);
            return 0;
        }

        *table[NVTX_CBID_CORE_MarkEx       ] = reinterpret_cast<NvtxFunctionPointer>(HandleMarkEx       );
        *table[NVTX_CBID_CORE_MarkA        ] = reinterpret_cast<NvtxFunctionPointer>(HandleMarkA        );
        *table[NVTX_CBID_CORE_MarkW        ] = reinterpret_cast<NvtxFunctionPointer>(HandleMarkW        );
        *table[NVTX_CBID_CORE_RangeStartEx ] = reinterpret_cast<NvtxFunctionPointer>(HandleRangeStartEx );
        *table[NVTX_CBID_CORE_RangeStartA  ] = reinterpret_cast<NvtxFunctionPointer>(HandleRangeStartA  );
        *table[NVTX_CBID_CORE_RangeStartW  ] = reinterpret_cast<NvtxFunctionPointer>(HandleRangeStartW  );
        *table[NVTX_CBID_CORE_RangeEnd     ] = reinterpret_cast<NvtxFunctionPointer>(HandleRangeEnd     );
        *table[NVTX_CBID_CORE_RangePushEx  ] = reinterpret_cast<NvtxFunctionPointer>(HandleRangePushEx  );
        *table[NVTX_CBID_CORE_RangePushA   ] = reinterpret_cast<NvtxFunctionPointer>(HandleRangePushA   );
        *table[NVTX_CBID_CORE_RangePushW   ] = reinterpret_cast<NvtxFunctionPointer>(HandleRangePushW   );
        *table[NVTX_CBID_CORE_RangePop     ] = reinterpret_cast<NvtxFunctionPointer>(HandleRangePop     );
        *table[NVTX_CBID_CORE_NameCategoryA] = reinterpret_cast<NvtxFunctionPointer>(HandleNameCategoryA);
        *table[NVTX_CBID_CORE_NameCategoryW] = reinterpret_cast<NvtxFunctionPointer>(HandleNameCategoryW);
        *table[NVTX_CBID_CORE_NameOsThreadA] = reinterpret_cast<NvtxFunctionPointer>(HandleNameOsThreadA);
        *table[NVTX_CBID_CORE_NameOsThreadW] = reinterpret_cast<NvtxFunctionPointer>(HandleNameOsThreadW);
    }

    {
        NvtxFunctionTable table = nullptr;
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
                static_cast<int>(size),
                static_cast<int>(highestIdUsed));
            g_callbacks.Load(0);
            return 0;
        }

        *table[NVTX_CBID_CORE2_DomainMarkEx         ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainMarkEx         );
        *table[NVTX_CBID_CORE2_DomainRangeStartEx   ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainRangeStartEx   );
        *table[NVTX_CBID_CORE2_DomainRangeEnd       ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainRangeEnd       );
        *table[NVTX_CBID_CORE2_DomainRangePushEx    ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainRangePushEx    );
        *table[NVTX_CBID_CORE2_DomainRangePop       ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainRangePop       );
        *table[NVTX_CBID_CORE2_DomainResourceCreate ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainResourceCreate );
        *table[NVTX_CBID_CORE2_DomainResourceDestroy] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainResourceDestroy);
        *table[NVTX_CBID_CORE2_DomainNameCategoryA  ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainNameCategoryA  );
        *table[NVTX_CBID_CORE2_DomainNameCategoryW  ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainNameCategoryW  );
        *table[NVTX_CBID_CORE2_DomainRegisterStringA] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainRegisterStringA);
        *table[NVTX_CBID_CORE2_DomainRegisterStringW] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainRegisterStringW);
        *table[NVTX_CBID_CORE2_DomainCreateA        ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainCreateA        );
        *table[NVTX_CBID_CORE2_DomainCreateW        ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainCreateW        );
        *table[NVTX_CBID_CORE2_DomainDestroy        ] = reinterpret_cast<NvtxFunctionPointer>(HandleDomainDestroy        );
        *table[NVTX_CBID_CORE2_Initialize           ] = reinterpret_cast<NvtxFunctionPointer>(HandleInitialize           );
    }

    g_callbacks.Load(1);
    return 1;
}
