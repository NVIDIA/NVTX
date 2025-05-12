/*
 * SPDX-FileCopyrightText: Copyright (c) 2009-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#if defined(NVTX_AS_SYSTEM_HEADER)
#if defined(__clang__)
#pragma clang system_header
#elif defined(__GNUC__) || defined(__NVCOMPILER)
#pragma GCC system_header
#elif defined(_MSC_VER)
#pragma system_header
#endif
#endif

NVTX_DECLSPEC void NVTX_API nvtxMarkEx(const nvtxEventAttributes_t* eventAttrib)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)eventAttrib;
#else /* NVTX_DISABLE */
    nvtxMarkEx_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxMarkEx_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(eventAttrib);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxMarkA(const char* message)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)message;
#else /* NVTX_DISABLE */
    nvtxMarkA_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxMarkA_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(message);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxMarkW(const wchar_t* message)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)message;
#else /* NVTX_DISABLE */
    nvtxMarkW_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxMarkW_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(message);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC nvtxRangeId_t NVTX_API nvtxRangeStartEx(const nvtxEventAttributes_t* eventAttrib)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)eventAttrib;
#else /* NVTX_DISABLE */
    nvtxRangeStartEx_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxRangeStartEx_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(eventAttrib);
    else
#endif /* NVTX_DISABLE */
        return NVTX_STATIC_CAST(nvtxRangeId_t, 0);
}

NVTX_DECLSPEC nvtxRangeId_t NVTX_API nvtxRangeStartA(const char* message)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)message;
#else /* NVTX_DISABLE */
    nvtxRangeStartA_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxRangeStartA_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(message);
    else
#endif /* NVTX_DISABLE */
        return NVTX_STATIC_CAST(nvtxRangeId_t, 0);
}

NVTX_DECLSPEC nvtxRangeId_t NVTX_API nvtxRangeStartW(const wchar_t* message)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)message;
#else /* NVTX_DISABLE */
    nvtxRangeStartW_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxRangeStartW_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(message);
    else
#endif /* NVTX_DISABLE */
        return NVTX_STATIC_CAST(nvtxRangeId_t, 0);
}

NVTX_DECLSPEC void NVTX_API nvtxRangeEnd(nvtxRangeId_t id)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)id;
#else /* NVTX_DISABLE */
    nvtxRangeEnd_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxRangeEnd_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(id);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC int NVTX_API nvtxRangePushEx(const nvtxEventAttributes_t* eventAttrib)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)eventAttrib;
#else /* NVTX_DISABLE */
    nvtxRangePushEx_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxRangePushEx_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(eventAttrib);
    else
#endif /* NVTX_DISABLE */
        return NVTX_STATIC_CAST(int, NVTX_NO_PUSH_POP_TRACKING);
}

NVTX_DECLSPEC int NVTX_API nvtxRangePushA(const char* message)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)message;
#else /* NVTX_DISABLE */
    nvtxRangePushA_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxRangePushA_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(message);
    else
#endif /* NVTX_DISABLE */
        return NVTX_STATIC_CAST(int, NVTX_NO_PUSH_POP_TRACKING);
}

NVTX_DECLSPEC int NVTX_API nvtxRangePushW(const wchar_t* message)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)message;
#else /* NVTX_DISABLE */
    nvtxRangePushW_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxRangePushW_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(message);
    else
#endif /* NVTX_DISABLE */
        return NVTX_STATIC_CAST(int, NVTX_NO_PUSH_POP_TRACKING);
}

NVTX_DECLSPEC int NVTX_API nvtxRangePop(void)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifndef NVTX_DISABLE
    nvtxRangePop_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxRangePop_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)();
    else
#endif /* NVTX_DISABLE */
        return NVTX_STATIC_CAST(int, NVTX_NO_PUSH_POP_TRACKING);
}

NVTX_DECLSPEC void NVTX_API nvtxNameCategoryA(uint32_t category, const char* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)category;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCategoryA_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCategoryA_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(category, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameCategoryW(uint32_t category, const wchar_t* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)category;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameCategoryW_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameCategoryW_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(category, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameOsThreadA(uint32_t threadId, const char* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)threadId;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameOsThreadA_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameOsThreadA_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(threadId, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxNameOsThreadW(uint32_t threadId, const wchar_t* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)threadId;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxNameOsThreadW_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxNameOsThreadW_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(threadId, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxDomainMarkEx(nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)domain;
    (void)eventAttrib;
#else /* NVTX_DISABLE */
    nvtxDomainMarkEx_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainMarkEx_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(domain, eventAttrib);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC nvtxRangeId_t NVTX_API nvtxDomainRangeStartEx(nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)domain;
    (void)eventAttrib;
#else /* NVTX_DISABLE */
    nvtxDomainRangeStartEx_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainRangeStartEx_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(domain, eventAttrib);
    else
#endif /* NVTX_DISABLE */
        return NVTX_STATIC_CAST(nvtxRangeId_t, 0);
}

NVTX_DECLSPEC void NVTX_API nvtxDomainRangeEnd(nvtxDomainHandle_t domain, nvtxRangeId_t id)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)domain;
    (void)id;
#else /* NVTX_DISABLE */
    nvtxDomainRangeEnd_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainRangeEnd_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(domain, id);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC int NVTX_API nvtxDomainRangePushEx(nvtxDomainHandle_t domain, const nvtxEventAttributes_t* eventAttrib)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)domain;
    (void)eventAttrib;
#else /* NVTX_DISABLE */
    nvtxDomainRangePushEx_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainRangePushEx_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(domain, eventAttrib);
    else
#endif /* NVTX_DISABLE */
        return NVTX_STATIC_CAST(int, NVTX_NO_PUSH_POP_TRACKING);
}

NVTX_DECLSPEC int NVTX_API nvtxDomainRangePop(nvtxDomainHandle_t domain)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)domain;
#else /* NVTX_DISABLE */
    nvtxDomainRangePop_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainRangePop_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(domain);
    else
#endif /* NVTX_DISABLE */
        return NVTX_STATIC_CAST(int, NVTX_NO_PUSH_POP_TRACKING);
}

NVTX_DECLSPEC nvtxResourceHandle_t NVTX_API nvtxDomainResourceCreate(nvtxDomainHandle_t domain, nvtxResourceAttributes_t* attribs)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)domain;
    (void)attribs;
#else /* NVTX_DISABLE */
    nvtxDomainResourceCreate_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainResourceCreate_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(domain, attribs);
    else
#endif /* NVTX_DISABLE */
        return NVTX_NULLPTR;
}

NVTX_DECLSPEC void NVTX_API nvtxDomainResourceDestroy(nvtxResourceHandle_t resource)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)resource;
#else /* NVTX_DISABLE */
    nvtxDomainResourceDestroy_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainResourceDestroy_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(resource);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxDomainNameCategoryA(nvtxDomainHandle_t domain, uint32_t category, const char* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)domain;
    (void)category;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxDomainNameCategoryA_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainNameCategoryA_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(domain, category, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxDomainNameCategoryW(nvtxDomainHandle_t domain, uint32_t category, const wchar_t* name)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)domain;
    (void)category;
    (void)name;
#else /* NVTX_DISABLE */
    nvtxDomainNameCategoryW_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainNameCategoryW_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(domain, category, name);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC nvtxStringHandle_t NVTX_API nvtxDomainRegisterStringA(nvtxDomainHandle_t domain, const char* string)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)domain;
    (void)string;
#else /* NVTX_DISABLE */
    nvtxDomainRegisterStringA_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainRegisterStringA_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(domain, string);
    else
#endif /* NVTX_DISABLE */
        return NVTX_NULLPTR;
}

NVTX_DECLSPEC nvtxStringHandle_t NVTX_API nvtxDomainRegisterStringW(nvtxDomainHandle_t domain, const wchar_t* string)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)domain;
    (void)string;
#else /* NVTX_DISABLE */
    nvtxDomainRegisterStringW_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainRegisterStringW_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(domain, string);
    else
#endif /* NVTX_DISABLE */
        return NVTX_NULLPTR;
}

NVTX_DECLSPEC nvtxDomainHandle_t NVTX_API nvtxDomainCreateA(const char* message)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)message;
#else /* NVTX_DISABLE */
    nvtxDomainCreateA_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainCreateA_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(message);
    else
#endif /* NVTX_DISABLE */
        return NVTX_NULLPTR;
}

NVTX_DECLSPEC nvtxDomainHandle_t NVTX_API nvtxDomainCreateW(const wchar_t* message)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)message;
#else /* NVTX_DISABLE */
    nvtxDomainCreateW_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainCreateW_impl_fnptr;
    if (local != NVTX_NULLPTR)
        return (*local)(message);
    else
#endif /* NVTX_DISABLE */
        return NVTX_NULLPTR;
}

NVTX_DECLSPEC void NVTX_API nvtxDomainDestroy(nvtxDomainHandle_t domain)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)domain;
#else /* NVTX_DISABLE */
    nvtxDomainDestroy_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxDomainDestroy_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(domain);
#endif /* NVTX_DISABLE */
}

NVTX_DECLSPEC void NVTX_API nvtxInitialize(const void* reserved)
{
    NVTX_SET_NAME_MANGLING_OPTIONS
#ifdef NVTX_DISABLE
    (void)reserved;
#else /* NVTX_DISABLE */
    nvtxInitialize_impl_fntype local = NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).nvtxInitialize_impl_fnptr;
    if (local != NVTX_NULLPTR)
        (*local)(reserved);
#endif /* NVTX_DISABLE */
}
