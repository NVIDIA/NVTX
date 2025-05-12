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

#define NVTX_NO_IMPL
#include "nvtx3/nvToolsExt.h"

#include "Same.h"
#include "PrettyPrintersNvtxC.h"

#include <functional>
#include <iostream>
#include <iomanip>
#include <memory>
#include <map>
#include <string>

constexpr auto NVTX_TOOL_ATTACHED_UNUSED_RANGE_ID = static_cast<nvtxRangeId_t>(-1LL);
constexpr int  NVTX_TOOL_ATTACHED_UNUSED_PUSH_POP_ID = -1;
const     auto NVTX_TOOL_ATTACHED_UNUSED_DOMAIN_HANDLE = reinterpret_cast<nvtxDomainHandle_t>(-1LL);
const     auto NVTX_TOOL_ATTACHED_UNUSED_STRING_HANDLE = reinterpret_cast<nvtxStringHandle_t>(-1LL);
const     auto NVTX_TOOL_ATTACHED_UNUSED_RESOURCE_HANDLE = reinterpret_cast<nvtxResourceHandle_t>(-1LL);

struct ArgsLoad { int success; };

struct ArgsMarkEx        { const nvtxEventAttributes_t* eventAttrib; };
struct ArgsMarkA         { const char* str                         ; };
struct ArgsMarkW         { const wchar_t* str                      ; };
struct ArgsRangeStartEx  { const nvtxEventAttributes_t* eventAttrib; };
struct ArgsRangeStartA   { const char* str                         ; };
struct ArgsRangeStartW   { const wchar_t* str                      ; };
struct ArgsRangeEnd      { nvtxRangeId_t id                        ; };
struct ArgsRangePushEx   { const nvtxEventAttributes_t* eventAttrib; };
struct ArgsRangePushA    { const char* str                         ; };
struct ArgsRangePushW    { const wchar_t* str                      ; };
struct ArgsRangePop      {                                           };
struct ArgsNameCategoryA { uint32_t id; const char* str            ; };
struct ArgsNameCategoryW { uint32_t id; const wchar_t* str         ; };
struct ArgsNameOsThreadA { uint32_t id; const char* str            ; };
struct ArgsNameOsThreadW { uint32_t id; const wchar_t* str         ; };

struct ArgsDomainMarkEx          { nvtxDomainHandle_t domain; const nvtxEventAttributes_t* eventAttrib; };
struct ArgsDomainRangeStartEx    { nvtxDomainHandle_t domain; const nvtxEventAttributes_t* eventAttrib; };
struct ArgsDomainRangeEnd        { nvtxDomainHandle_t domain; nvtxRangeId_t id                        ; };
struct ArgsDomainRangePushEx     { nvtxDomainHandle_t domain; const nvtxEventAttributes_t* eventAttrib; };
struct ArgsDomainRangePop        { nvtxDomainHandle_t domain                                          ; };
struct ArgsDomainResourceCreate  { nvtxDomainHandle_t domain; nvtxResourceAttributes_t* attr          ; };
struct ArgsDomainResourceDestroy { nvtxResourceHandle_t attr                                          ; };
struct ArgsDomainNameCategoryA   { nvtxDomainHandle_t domain; uint32_t id; const char* str            ; };
struct ArgsDomainNameCategoryW   { nvtxDomainHandle_t domain; uint32_t id; const wchar_t* str         ; };
struct ArgsDomainRegisterStringA { nvtxDomainHandle_t domain; const char* str                         ; };
struct ArgsDomainRegisterStringW { nvtxDomainHandle_t domain; const wchar_t* str                      ; };
struct ArgsDomainCreateA         { const char* name                                                   ; };
struct ArgsDomainCreateW         { const wchar_t* name                                                ; };
struct ArgsDomainDestroy         { nvtxDomainHandle_t domain                                          ; };
struct ArgsInitialize            { const void* reserved                                               ; };

struct CallId
{
    NvtxCallbackModule mod;
    int32_t cb;
};
DEFINE_SAME_2(CallId, mod, cb)

// Helper to write CALLID(CORE, MarkEx) as shorthand for CallId{NVTX_CB_MODULE_CORE, NVTX_CBID_CORE_MarkEx}
#define CALLID(m,c) CallId{NVTX_CB_MODULE_##m, static_cast<int32_t>(NVTX_CBID_##m##_##c)}

#define CALLID_LOAD() CallId{NVTX_CB_MODULE_INVALID, static_cast<int32_t>(0x7ac0be11)}

inline const char* CallName(CallId const& id)
{
    if (id == CALLID_LOAD()) return "InitializeInjectionNvtx2";
    switch (id.mod)
    {
    case NVTX_CB_MODULE_CORE:
        switch (id.cb)
        {
        case NVTX_CBID_CORE_MarkEx       : return "MarkEx";
        case NVTX_CBID_CORE_MarkA        : return "MarkA";
        case NVTX_CBID_CORE_MarkW        : return "MarkW";
        case NVTX_CBID_CORE_RangeStartEx : return "RangeStartEx";
        case NVTX_CBID_CORE_RangeStartA  : return "RangeStartA";
        case NVTX_CBID_CORE_RangeStartW  : return "RangeStartW";
        case NVTX_CBID_CORE_RangeEnd     : return "RangeEnd";
        case NVTX_CBID_CORE_RangePushEx  : return "RangePushEx";
        case NVTX_CBID_CORE_RangePushA   : return "RangePushA";
        case NVTX_CBID_CORE_RangePushW   : return "RangePushW";
        case NVTX_CBID_CORE_RangePop     : return "RangePop";
        case NVTX_CBID_CORE_NameCategoryA: return "NameCategoryA";
        case NVTX_CBID_CORE_NameCategoryW: return "NameCategoryW";
        case NVTX_CBID_CORE_NameOsThreadA: return "NameOsThreadA";
        case NVTX_CBID_CORE_NameOsThreadW: return "NameOsThreadW";
        default: return "<Unknown CORE call>";
        }
    case NVTX_CB_MODULE_CORE2:
        switch (id.cb)
        {
        case NVTX_CBID_CORE2_DomainMarkEx         : return "DomainMarkEx";
        case NVTX_CBID_CORE2_DomainRangeStartEx   : return "DomainRangeStartEx";
        case NVTX_CBID_CORE2_DomainRangeEnd       : return "DomainRangeEnd";
        case NVTX_CBID_CORE2_DomainRangePushEx    : return "DomainRangePushEx";
        case NVTX_CBID_CORE2_DomainRangePop       : return "DomainRangePop";
        case NVTX_CBID_CORE2_DomainResourceCreate : return "DomainResourceCreate";
        case NVTX_CBID_CORE2_DomainResourceDestroy: return "DomainResourceDestroy";
        case NVTX_CBID_CORE2_DomainNameCategoryA  : return "DomainNameCategoryA";
        case NVTX_CBID_CORE2_DomainNameCategoryW  : return "DomainNameCategoryW";
        case NVTX_CBID_CORE2_DomainRegisterStringA: return "DomainRegisterStringA";
        case NVTX_CBID_CORE2_DomainRegisterStringW: return "DomainRegisterStringW";
        case NVTX_CBID_CORE2_DomainCreateA        : return "DomainCreateA";
        case NVTX_CBID_CORE2_DomainCreateW        : return "DomainCreateW";
        case NVTX_CBID_CORE2_DomainDestroy        : return "DomainDestroy";
        case NVTX_CBID_CORE2_Initialize           : return "Initialize";
        default: return "<Unknown CORE2 call>";
        }
    case NVTX_CB_MODULE_SYNC:
        return "<Unknown SYNC call>";
    case NVTX_CB_MODULE_CUDA:
        return "<Unknown CUDA call>";
    case NVTX_CB_MODULE_CUDART:
        return "<Unknown CUDART call>";
    case NVTX_CB_MODULE_OPENCL:
        return "<Unknown OPENCL call>";
    case NVTX_CB_MODULE_INVALID:
    case NVTX_CB_MODULE_SIZE:
    case NVTX_CB_MODULE_FORCE_INT:
    default: return "<Unknown CB_MODULE>";
    }
}

inline std::ostream& operator<<(std::ostream& os, CallId const& id)
{
    return os << CallName(id);
}

union Args
{
    ArgsLoad          Load;

    ArgsMarkEx        MarkEx       ;
    ArgsMarkA         MarkA        ;
    ArgsMarkW         MarkW        ;
    ArgsRangeStartEx  RangeStartEx ;
    ArgsRangeStartA   RangeStartA  ;
    ArgsRangeStartW   RangeStartW  ;
    ArgsRangeEnd      RangeEnd     ;
    ArgsRangePushEx   RangePushEx  ;
    ArgsRangePushA    RangePushA   ;
    ArgsRangePushW    RangePushW   ;
    ArgsRangePop      RangePop     ;
    ArgsNameCategoryA NameCategoryA;
    ArgsNameCategoryW NameCategoryW;
    ArgsNameOsThreadA NameOsThreadA;
    ArgsNameOsThreadW NameOsThreadW;

    ArgsDomainMarkEx          DomainMarkEx         ;
    ArgsDomainRangeStartEx    DomainRangeStartEx   ;
    ArgsDomainRangeEnd        DomainRangeEnd       ;
    ArgsDomainRangePushEx     DomainRangePushEx    ;
    ArgsDomainRangePop        DomainRangePop       ;
    ArgsDomainResourceCreate  DomainResourceCreate ;
    ArgsDomainResourceDestroy DomainResourceDestroy;
    ArgsDomainNameCategoryA   DomainNameCategoryA  ;
    ArgsDomainNameCategoryW   DomainNameCategoryW  ;
    ArgsDomainRegisterStringA DomainRegisterStringA;
    ArgsDomainRegisterStringW DomainRegisterStringW;
    ArgsDomainCreateA         DomainCreateA        ;
    ArgsDomainCreateW         DomainCreateW        ;
    ArgsDomainDestroy         DomainDestroy        ;
    ArgsInitialize            Initialize           ;
};

// Free functions to emulate copy constructors and destructors for the NVTX C API types using pointers
inline void CopyCstring(const char*& lhs, const char* rhs)
{
    size_t len = strlen(rhs) + 1;
    auto* tmp = new char[len];
    std::copy(rhs, rhs + len, tmp);
    lhs = tmp;
}
inline void CopyCstring(const char*& s) { CopyCstring(s, s); }
inline void DestroyCstring(const char* s) { delete[] s; }

inline void CopyCstring(const wchar_t*& lhs, const wchar_t* rhs)
{
    size_t len = wcslen(rhs) + 1;
    auto* tmp = new wchar_t[len];
    std::copy(rhs, rhs + len, tmp);
    lhs = tmp;
}
inline void CopyCstring(const wchar_t*& s) { CopyCstring(s, s); }
inline void DestroyCstring(const wchar_t* s) { delete[] s; }

inline void CopyEventAttributes(const nvtxEventAttributes_t*& lhs, const nvtxEventAttributes_t* rhs)
{
    auto* tmp = new nvtxEventAttributes_t;
    memcpy(tmp, rhs, sizeof(*tmp));
    switch (tmp->messageType)
    {
    case NVTX_MESSAGE_TYPE_ASCII:   CopyCstring(tmp->message.ascii);   break;
    case NVTX_MESSAGE_TYPE_UNICODE: CopyCstring(tmp->message.unicode); break;
    default: break;
    }
    lhs = tmp;
}
inline void CopyEventAttributes(const nvtxEventAttributes_t*& a) { CopyEventAttributes(a, a); }
inline void DestroyEventAttributes(const nvtxEventAttributes_t* a)
{
    switch (a->messageType)
    {
    case NVTX_MESSAGE_TYPE_ASCII:   DestroyCstring(a->message.ascii);   break;
    case NVTX_MESSAGE_TYPE_UNICODE: DestroyCstring(a->message.unicode); break;
    default: break;
    }
    delete a;
}

inline void CopyResourceAttributes(nvtxResourceAttributes_t*& lhs, const nvtxResourceAttributes_t* rhs)
{
    auto* tmp = new nvtxResourceAttributes_t;
    memcpy(tmp, rhs, sizeof(*tmp));
    switch (tmp->messageType)
    {
    case NVTX_MESSAGE_TYPE_ASCII:   CopyCstring(tmp->message.ascii);   break;
    case NVTX_MESSAGE_TYPE_UNICODE: CopyCstring(tmp->message.unicode); break;
    default: break;
    }
    lhs = tmp;
}
inline void CopyResourceAttributes(nvtxResourceAttributes_t*& a) { CopyResourceAttributes(a, a); }
inline void DestroyResourceAttributes(nvtxResourceAttributes_t* a)
{
    switch (a->messageType)
    {
    case NVTX_MESSAGE_TYPE_ASCII:   DestroyCstring(a->message.ascii);   break;
    case NVTX_MESSAGE_TYPE_UNICODE: DestroyCstring(a->message.unicode); break;
    default: break;
    }
    delete a;
}

template <typename ArgsT> inline void DeepCopyAssign(ArgsT& lhs, ArgsT const& rhs) { lhs = rhs; }

template <> inline void DeepCopyAssign(ArgsMarkEx       & lhs, ArgsMarkEx        const& rhs) { lhs = rhs; CopyEventAttributes(lhs.eventAttrib); }
template <> inline void DeepCopyAssign(ArgsMarkA        & lhs, ArgsMarkA         const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsMarkW        & lhs, ArgsMarkW         const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsRangeStartEx & lhs, ArgsRangeStartEx  const& rhs) { lhs = rhs; CopyEventAttributes(lhs.eventAttrib); }
template <> inline void DeepCopyAssign(ArgsRangeStartA  & lhs, ArgsRangeStartA   const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsRangeStartW  & lhs, ArgsRangeStartW   const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsRangeEnd     & lhs, ArgsRangeEnd      const& rhs) { lhs = rhs; }
template <> inline void DeepCopyAssign(ArgsRangePushEx  & lhs, ArgsRangePushEx   const& rhs) { lhs = rhs; CopyEventAttributes(lhs.eventAttrib); }
template <> inline void DeepCopyAssign(ArgsRangePushA   & lhs, ArgsRangePushA    const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsRangePushW   & lhs, ArgsRangePushW    const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsRangePop     & lhs, ArgsRangePop      const& rhs) { lhs = rhs; }
template <> inline void DeepCopyAssign(ArgsNameCategoryA& lhs, ArgsNameCategoryA const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsNameCategoryW& lhs, ArgsNameCategoryW const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsNameOsThreadA& lhs, ArgsNameOsThreadA const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsNameOsThreadW& lhs, ArgsNameOsThreadW const& rhs) { lhs = rhs; CopyCstring(lhs.str); }

template <> inline void DeepCopyAssign(ArgsDomainMarkEx         & lhs, ArgsDomainMarkEx          const& rhs) { lhs = rhs; CopyEventAttributes(lhs.eventAttrib); }
template <> inline void DeepCopyAssign(ArgsDomainRangeStartEx   & lhs, ArgsDomainRangeStartEx    const& rhs) { lhs = rhs; CopyEventAttributes(lhs.eventAttrib); }
template <> inline void DeepCopyAssign(ArgsDomainRangeEnd       & lhs, ArgsDomainRangeEnd        const& rhs) { lhs = rhs; }
template <> inline void DeepCopyAssign(ArgsDomainRangePushEx    & lhs, ArgsDomainRangePushEx     const& rhs) { lhs = rhs; CopyEventAttributes(lhs.eventAttrib); }
template <> inline void DeepCopyAssign(ArgsDomainRangePop       & lhs, ArgsDomainRangePop        const& rhs) { lhs = rhs; }
template <> inline void DeepCopyAssign(ArgsDomainResourceCreate & lhs, ArgsDomainResourceCreate  const& rhs) { lhs = rhs; CopyResourceAttributes(lhs.attr); }
template <> inline void DeepCopyAssign(ArgsDomainResourceDestroy& lhs, ArgsDomainResourceDestroy const& rhs) { lhs = rhs; }
template <> inline void DeepCopyAssign(ArgsDomainNameCategoryA  & lhs, ArgsDomainNameCategoryA   const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsDomainNameCategoryW  & lhs, ArgsDomainNameCategoryW   const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsDomainRegisterStringA& lhs, ArgsDomainRegisterStringA const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsDomainRegisterStringW& lhs, ArgsDomainRegisterStringW const& rhs) { lhs = rhs; CopyCstring(lhs.str); }
template <> inline void DeepCopyAssign(ArgsDomainCreateA        & lhs, ArgsDomainCreateA         const& rhs) { lhs = rhs; CopyCstring(lhs.name); }
template <> inline void DeepCopyAssign(ArgsDomainCreateW        & lhs, ArgsDomainCreateW         const& rhs) { lhs = rhs; CopyCstring(lhs.name); }
template <> inline void DeepCopyAssign(ArgsDomainDestroy        & lhs, ArgsDomainDestroy         const& rhs) { lhs = rhs; }
template <> inline void DeepCopyAssign(ArgsInitialize           & lhs, ArgsInitialize            const& rhs) { lhs = rhs; }

template <typename ArgsT> inline  void DeepCopyDestroy(ArgsT&) {}

template <> inline void DeepCopyDestroy(ArgsMarkEx       & args) { DestroyEventAttributes(args.eventAttrib); }
template <> inline void DeepCopyDestroy(ArgsMarkA        & args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsMarkW        & args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsRangeStartEx & args) { DestroyEventAttributes(args.eventAttrib); }
template <> inline void DeepCopyDestroy(ArgsRangeStartA  & args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsRangeStartW  & args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsRangeEnd     & /*args*/) { }
template <> inline void DeepCopyDestroy(ArgsRangePushEx  & args) { DestroyEventAttributes(args.eventAttrib); }
template <> inline void DeepCopyDestroy(ArgsRangePushA   & args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsRangePushW   & args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsRangePop     & /*args*/) { }
template <> inline void DeepCopyDestroy(ArgsNameCategoryA& args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsNameCategoryW& args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsNameOsThreadA& args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsNameOsThreadW& args) { DestroyCstring(args.str); }

template <> inline void DeepCopyDestroy(ArgsDomainMarkEx         & args) { DestroyEventAttributes(args.eventAttrib); }
template <> inline void DeepCopyDestroy(ArgsDomainRangeStartEx   & args) { DestroyEventAttributes(args.eventAttrib); }
template <> inline void DeepCopyDestroy(ArgsDomainRangeEnd       & /*args*/) { }
template <> inline void DeepCopyDestroy(ArgsDomainRangePushEx    & args) { DestroyEventAttributes(args.eventAttrib); }
template <> inline void DeepCopyDestroy(ArgsDomainRangePop       & /*args*/) { }
template <> inline void DeepCopyDestroy(ArgsDomainResourceCreate & args) { DestroyResourceAttributes(args.attr); }
template <> inline void DeepCopyDestroy(ArgsDomainResourceDestroy& /*args*/) { }
template <> inline void DeepCopyDestroy(ArgsDomainNameCategoryA  & args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsDomainNameCategoryW  & args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsDomainRegisterStringA& args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsDomainRegisterStringW& args) { DestroyCstring(args.str); }
template <> inline void DeepCopyDestroy(ArgsDomainCreateA        & args) { DestroyCstring(args.name); }
template <> inline void DeepCopyDestroy(ArgsDomainCreateW        & args) { DestroyCstring(args.name); }
template <> inline void DeepCopyDestroy(ArgsDomainDestroy        & /*args*/) { }
template <> inline void DeepCopyDestroy(ArgsInitialize           & /*args*/) { }

struct CallData
{
    CallId id{NVTX_CB_MODULE_INVALID, 0};
    Args args;

    ~CallData()
    {
        switch (id.mod)
        {
        case NVTX_CB_MODULE_CORE:
            switch (id.cb)
            {
            case NVTX_CBID_CORE_MarkEx       : DeepCopyDestroy(args.MarkEx       ); break;
            case NVTX_CBID_CORE_MarkA        : DeepCopyDestroy(args.MarkA        ); break;
            case NVTX_CBID_CORE_MarkW        : DeepCopyDestroy(args.MarkW        ); break;
            case NVTX_CBID_CORE_RangeStartEx : DeepCopyDestroy(args.RangeStartEx ); break;
            case NVTX_CBID_CORE_RangeStartA  : DeepCopyDestroy(args.RangeStartA  ); break;
            case NVTX_CBID_CORE_RangeStartW  : DeepCopyDestroy(args.RangeStartW  ); break;
            case NVTX_CBID_CORE_RangeEnd     : DeepCopyDestroy(args.RangeEnd     ); break;
            case NVTX_CBID_CORE_RangePushEx  : DeepCopyDestroy(args.RangePushEx  ); break;
            case NVTX_CBID_CORE_RangePushA   : DeepCopyDestroy(args.RangePushA   ); break;
            case NVTX_CBID_CORE_RangePushW   : DeepCopyDestroy(args.RangePushW   ); break;
            case NVTX_CBID_CORE_RangePop     : DeepCopyDestroy(args.RangePop     ); break;
            case NVTX_CBID_CORE_NameCategoryA: DeepCopyDestroy(args.NameCategoryA); break;
            case NVTX_CBID_CORE_NameCategoryW: DeepCopyDestroy(args.NameCategoryW); break;
            case NVTX_CBID_CORE_NameOsThreadA: DeepCopyDestroy(args.NameOsThreadA); break;
            case NVTX_CBID_CORE_NameOsThreadW: DeepCopyDestroy(args.NameOsThreadW); break;
            default: break;
            }
            break;
        case NVTX_CB_MODULE_CORE2:
            switch (id.cb)
            {
            case NVTX_CBID_CORE2_DomainMarkEx         : DeepCopyDestroy(args.DomainMarkEx         ); break;
            case NVTX_CBID_CORE2_DomainRangeStartEx   : DeepCopyDestroy(args.DomainRangeStartEx   ); break;
            case NVTX_CBID_CORE2_DomainRangeEnd       : DeepCopyDestroy(args.DomainRangeEnd       ); break;
            case NVTX_CBID_CORE2_DomainRangePushEx    : DeepCopyDestroy(args.DomainRangePushEx    ); break;
            case NVTX_CBID_CORE2_DomainRangePop       : DeepCopyDestroy(args.DomainRangePop       ); break;
            case NVTX_CBID_CORE2_DomainResourceCreate : DeepCopyDestroy(args.DomainResourceCreate ); break;
            case NVTX_CBID_CORE2_DomainResourceDestroy: DeepCopyDestroy(args.DomainResourceDestroy); break;
            case NVTX_CBID_CORE2_DomainNameCategoryA  : DeepCopyDestroy(args.DomainNameCategoryA  ); break;
            case NVTX_CBID_CORE2_DomainNameCategoryW  : DeepCopyDestroy(args.DomainNameCategoryW  ); break;
            case NVTX_CBID_CORE2_DomainRegisterStringA: DeepCopyDestroy(args.DomainRegisterStringA); break;
            case NVTX_CBID_CORE2_DomainRegisterStringW: DeepCopyDestroy(args.DomainRegisterStringW); break;
            case NVTX_CBID_CORE2_DomainCreateA        : DeepCopyDestroy(args.DomainCreateA        ); break;
            case NVTX_CBID_CORE2_DomainCreateW        : DeepCopyDestroy(args.DomainCreateW        ); break;
            case NVTX_CBID_CORE2_DomainDestroy        : DeepCopyDestroy(args.DomainDestroy        ); break;
            case NVTX_CBID_CORE2_Initialize           : DeepCopyDestroy(args.Initialize           ); break;
            default: break;
            }
            break;
        case NVTX_CB_MODULE_SYNC:
        case NVTX_CB_MODULE_CUDA:
        case NVTX_CB_MODULE_CUDART:
        case NVTX_CB_MODULE_OPENCL:
        case NVTX_CB_MODULE_INVALID:
        case NVTX_CB_MODULE_SIZE:
        case NVTX_CB_MODULE_FORCE_INT:
        default: break;
        }
    }
};

inline std::ostream& operator<<(std::ostream& os, CallData const& data)
{
    if (data.id == CALLID_LOAD())
    {
        return os << CallName(data.id) << " returned " << data.args.Load.success;
    }

    os << "[" << data.id.mod << "," << std::setw(2) << data.id.cb << "] ";
    os << CallName(data.id) << '(';
    switch (data.id.mod)
    {
    case NVTX_CB_MODULE_CORE:
        switch (data.id.cb)
        {
        case NVTX_CBID_CORE_MarkEx       : {auto& a = data.args.MarkEx       ; os << *a.eventAttrib;                 } break;
        case NVTX_CBID_CORE_MarkA        : {auto& a = data.args.MarkA        ; os << '"' << a.str << '"';            } break;
        case NVTX_CBID_CORE_MarkW        : {/*auto& a = data.args.MarkW      ;*/ os << "WIDE";                       } break;
        case NVTX_CBID_CORE_RangeStartEx : {auto& a = data.args.RangeStartEx ; os << *a.eventAttrib;                 } break;
        case NVTX_CBID_CORE_RangeStartA  : {auto& a = data.args.RangeStartA  ; os << '"' << a.str << '"';            } break;
        case NVTX_CBID_CORE_RangeStartW  : {/*auto& a = data.args.RangeStartW;*/ os << "WIDE";                       } break;
        case NVTX_CBID_CORE_RangeEnd     : {auto& a = data.args.RangeEnd     ; os << a.id;                           } break;
        case NVTX_CBID_CORE_RangePushEx  : {auto& a = data.args.RangePushEx  ; os << *a.eventAttrib;                 } break;
        case NVTX_CBID_CORE_RangePushA   : {auto& a = data.args.RangePushA   ; os << '"' << a.str << '"';            } break;
        case NVTX_CBID_CORE_RangePushW   : {/*auto& a = data.args.RangePushW ;*/ os << "WIDE";                       } break;
        case NVTX_CBID_CORE_RangePop     : {/*auto& a = data.args.RangePop   ;*/                                     } break;
        case NVTX_CBID_CORE_NameCategoryA: {auto& a = data.args.NameCategoryA; os << a.id << ", \"" << a.str << '"'; } break;
        case NVTX_CBID_CORE_NameCategoryW: {auto& a = data.args.NameCategoryW; os << a.id << ", " << "WIDE";         } break;
        case NVTX_CBID_CORE_NameOsThreadA: {auto& a = data.args.NameOsThreadA; os << a.id << ", \"" << a.str << '"'; } break;
        case NVTX_CBID_CORE_NameOsThreadW: {auto& a = data.args.NameOsThreadW; os << a.id << ", " << "WIDE";         } break;
        default: break;
        }
        break;
    case NVTX_CB_MODULE_CORE2:
        switch (data.id.cb)
        {
        case NVTX_CBID_CORE2_DomainMarkEx         : {auto& a = data.args.DomainMarkEx         ; os << a.domain << ", " << *a.eventAttrib;                } break;
        case NVTX_CBID_CORE2_DomainRangeStartEx   : {auto& a = data.args.DomainRangeStartEx   ; os << a.domain << ", " << *a.eventAttrib;                } break;
        case NVTX_CBID_CORE2_DomainRangeEnd       : {auto& a = data.args.DomainRangeEnd       ; os << a.domain << ", " << a.id;                          } break;
        case NVTX_CBID_CORE2_DomainRangePushEx    : {auto& a = data.args.DomainRangePushEx    ; os << a.domain << ", " << *a.eventAttrib;                } break;
        case NVTX_CBID_CORE2_DomainRangePop       : {auto& a = data.args.DomainRangePop       ; os << a.domain;                                          } break;
        case NVTX_CBID_CORE2_DomainResourceCreate : {auto& a = data.args.DomainResourceCreate ; os << a.domain << ", " << a.attr;                        } break; // TODO
        case NVTX_CBID_CORE2_DomainResourceDestroy: {auto& a = data.args.DomainResourceDestroy; os << a.attr;                                            } break;
        case NVTX_CBID_CORE2_DomainNameCategoryA  : {auto& a = data.args.DomainNameCategoryA  ; os << a.domain << ", " << a.id << ", \"" << a.str << '"';} break;
        case NVTX_CBID_CORE2_DomainNameCategoryW  : {auto& a = data.args.DomainNameCategoryW  ; os << a.domain << ", " << a.id << ", " << "WIDE";        } break;
        case NVTX_CBID_CORE2_DomainRegisterStringA: {auto& a = data.args.DomainRegisterStringA; os << a.domain << ", \"" << a.str << '"';                } break;
        case NVTX_CBID_CORE2_DomainRegisterStringW: {auto& a = data.args.DomainRegisterStringW; os << a.domain << ", " << "WIDE";                        } break;
        case NVTX_CBID_CORE2_DomainCreateA        : {auto& a = data.args.DomainCreateA        ; os << '"' << a.name << '"';                              } break;
        case NVTX_CBID_CORE2_DomainCreateW        : {/*auto& a = data.args.DomainCreateW      ;*/ os << "WIDE";                                          } break;
        case NVTX_CBID_CORE2_DomainDestroy        : {auto& a = data.args.DomainDestroy        ; os << a.domain;                                          } break;
        case NVTX_CBID_CORE2_Initialize           : {auto& a = data.args.Initialize           ; os << a.reserved;                                        } break;
        default: break;
        }
        break;
    case NVTX_CB_MODULE_SYNC:
    case NVTX_CB_MODULE_CUDA:
    case NVTX_CB_MODULE_CUDART:
    case NVTX_CB_MODULE_OPENCL:
    case NVTX_CB_MODULE_INVALID:
    case NVTX_CB_MODULE_SIZE:
    case NVTX_CB_MODULE_FORCE_INT:
    default: break;
    }
    os << ')';
    return os;
}

using Call = std::shared_ptr<CallData>;

// Helper to write CALL(CORE, NameCategoryA, id, str) to construct a Call with arg values
#define CALL0(m,c) [=]{ Call v(new CallData); v->id = CALLID(m,c); DeepCopyAssign(v->args.c, Args##c{}); return v; }()
#define CALL(m,c,...) [=]{ Call v(new CallData); v->id = CALLID(m,c); DeepCopyAssign(v->args.c, Args##c{__VA_ARGS__}); return v; }()

#define CALL_LOAD(s) [=]{ Call v(new CallData); v->id = CALLID_LOAD(); v->args.Load = ArgsLoad{s}; return v; }()

// Helpers to construct unions from NVTX C API types
inline nvtxMessageValue_t MakeMessage(const char*        msg) { nvtxMessageValue_t v; v.ascii      = msg; return v; }
inline nvtxMessageValue_t MakeMessage(const wchar_t*     msg) { nvtxMessageValue_t v; v.unicode    = msg; return v; }
inline nvtxMessageValue_t MakeMessage(nvtxStringHandle_t msg) { nvtxMessageValue_t v; v.registered = msg; return v; }

inline nvtxEventAttributes_t::payload_t MakePayload(uint64_t v) { nvtxEventAttributes_t::payload_t p; p.ullValue = v; return p; }
inline nvtxEventAttributes_t::payload_t MakePayload(int64_t  v) { nvtxEventAttributes_t::payload_t p; p.llValue  = v; return p; }
inline nvtxEventAttributes_t::payload_t MakePayload(double   v) { nvtxEventAttributes_t::payload_t p; p.dValue   = v; return p; }
inline nvtxEventAttributes_t::payload_t MakePayload(uint32_t v) { nvtxEventAttributes_t::payload_t p; p.uiValue  = v; return p; }
inline nvtxEventAttributes_t::payload_t MakePayload(int32_t  v) { nvtxEventAttributes_t::payload_t p; p.iValue   = v; return p; }
inline nvtxEventAttributes_t::payload_t MakePayload(float    v) { nvtxEventAttributes_t::payload_t p; p.fValue   = v; return p; }

// Define Same() overloads for NVTX API types
inline bool Same(nvtxEventAttributes_t const& lhs, nvtxEventAttributes_t const& rhs, SAME_COMMON_ARGS)
{
    bool same = true
        && MEMBER_SAME(version)
        && MEMBER_SAME(size)
        && MEMBER_SAME(category)
        && MEMBER_SAME(colorType)
        && MEMBER_SAME(color)
        && MEMBER_SAME(payloadType)
        && (false
            || lhs.payloadType == NVTX_PAYLOAD_UNKNOWN
            || (lhs.payloadType == NVTX_PAYLOAD_TYPE_UNSIGNED_INT64 && MEMBER_SAME(payload.ullValue))
            || (lhs.payloadType == NVTX_PAYLOAD_TYPE_INT64          && MEMBER_SAME(payload.llValue))
            || (lhs.payloadType == NVTX_PAYLOAD_TYPE_DOUBLE         && MEMBER_SAME(payload.dValue))
            || (lhs.payloadType == NVTX_PAYLOAD_TYPE_UNSIGNED_INT32 && MEMBER_SAME(payload.uiValue))
            || (lhs.payloadType == NVTX_PAYLOAD_TYPE_INT32          && MEMBER_SAME(payload.iValue))
            || (lhs.payloadType == NVTX_PAYLOAD_TYPE_FLOAT          && MEMBER_SAME(payload.fValue))
            )
        && MEMBER_SAME(messageType)
        && (false
            || lhs.messageType == NVTX_MESSAGE_UNKNOWN
            || (lhs.messageType == NVTX_MESSAGE_TYPE_ASCII      && MEMBER_SAME(message.ascii))
            || (lhs.messageType == NVTX_MESSAGE_TYPE_UNICODE    && MEMBER_SAME(message.unicode))
            || (lhs.messageType == NVTX_MESSAGE_TYPE_REGISTERED && MEMBER_SAME(message.registered))
            )
        ;
    VERBOSE_PRINT()
        << std::string(depth, ' ') << "Expected: " << rhs << "\n"
        << std::string(depth, ' ') << "Provided: " << lhs << "\n";
    return same;
}
DEFINE_EQ_NE_DEEP(nvtxEventAttributes_t)

inline bool Same(nvtxResourceAttributes_t const& lhs, nvtxResourceAttributes_t const& rhs, SAME_COMMON_ARGS)
{
    bool same = true
        && MEMBER_SAME(version)
        && MEMBER_SAME(size)
        && MEMBER_SAME(identifierType)
        && (false
            || lhs.identifierType == NVTX_RESOURCE_TYPE_UNKNOWN
            || (lhs.identifierType == NVTX_RESOURCE_TYPE_GENERIC_POINTER       && PVOID_MEMBER_SAME(identifier.pValue))
            || (lhs.identifierType == NVTX_RESOURCE_TYPE_GENERIC_HANDLE        && MEMBER_SAME(identifier.ullValue))
            || (lhs.identifierType == NVTX_RESOURCE_TYPE_GENERIC_THREAD_NATIVE && MEMBER_SAME(identifier.ullValue))
            || (lhs.identifierType == NVTX_RESOURCE_TYPE_GENERIC_THREAD_POSIX  && MEMBER_SAME(identifier.ullValue))
            )
        && MEMBER_SAME(messageType)
        && (false
            || lhs.messageType == NVTX_MESSAGE_UNKNOWN
            || (lhs.messageType == NVTX_MESSAGE_TYPE_ASCII      && MEMBER_SAME(message.ascii))
            || (lhs.messageType == NVTX_MESSAGE_TYPE_UNICODE    && MEMBER_SAME(message.unicode))
            || (lhs.messageType == NVTX_MESSAGE_TYPE_REGISTERED && MEMBER_SAME(message.registered))
            )
        ;
    VERBOSE_PRINT();
    return same;
}
DEFINE_EQ_NE_DEEP(nvtxResourceAttributes_t)

// Define Same() overloads (and operators == and !=) for NVTX arg pack types & Args union

#define DEFINE_ARGS_SAME_0(cb)          DEFINE_SAME_0(Args##cb)
#define DEFINE_ARGS_SAME_1(cb, a)       DEFINE_SAME_1(Args##cb, a)
#define DEFINE_ARGS_SAME_2(cb, a, b)    DEFINE_SAME_2(Args##cb, a, b)
#define DEFINE_ARGS_SAME_3(cb, a, b, c) DEFINE_SAME_3(Args##cb, a, b, c)

#define DEFINE_ARGS_PVOID_SAME_1(cb, a) DEFINE_PVOID_SAME_1(Args##cb, a)

DEFINE_ARGS_SAME_1(Load, success)
// CORE
DEFINE_ARGS_SAME_1(MarkEx, eventAttrib)
DEFINE_ARGS_SAME_1(MarkA, str)
DEFINE_ARGS_SAME_1(MarkW, str)
DEFINE_ARGS_SAME_1(RangeStartEx, eventAttrib)
DEFINE_ARGS_SAME_1(RangeStartA, str)
DEFINE_ARGS_SAME_1(RangeStartW, str)
DEFINE_ARGS_SAME_0(RangeEnd)
DEFINE_ARGS_SAME_1(RangePushEx, eventAttrib)
DEFINE_ARGS_SAME_1(RangePushA, str)
DEFINE_ARGS_SAME_1(RangePushW, str)
DEFINE_ARGS_SAME_0(RangePop)
DEFINE_ARGS_SAME_2(NameCategoryA, id, str)
DEFINE_ARGS_SAME_2(NameCategoryW, id, str)
DEFINE_ARGS_SAME_2(NameOsThreadA, id, str)
DEFINE_ARGS_SAME_2(NameOsThreadW, id, str)
// CORE2
DEFINE_ARGS_SAME_2(DomainMarkEx, domain, eventAttrib)
DEFINE_ARGS_SAME_2(DomainRangeStartEx, domain, eventAttrib)
DEFINE_ARGS_SAME_2(DomainRangeEnd, domain, id)
DEFINE_ARGS_SAME_2(DomainRangePushEx, domain, eventAttrib)
DEFINE_ARGS_SAME_1(DomainRangePop, domain)
DEFINE_ARGS_SAME_2(DomainResourceCreate, domain, attr)
DEFINE_ARGS_SAME_1(DomainResourceDestroy, attr)
DEFINE_ARGS_SAME_3(DomainNameCategoryA, domain, id, str)
DEFINE_ARGS_SAME_3(DomainNameCategoryW, domain, id, str)
DEFINE_ARGS_SAME_2(DomainRegisterStringA, domain, str)
DEFINE_ARGS_SAME_2(DomainRegisterStringW, domain, str)
DEFINE_ARGS_SAME_1(DomainCreateA, name)
DEFINE_ARGS_SAME_1(DomainCreateW, name)
DEFINE_ARGS_SAME_1(DomainDestroy, domain)
DEFINE_ARGS_PVOID_SAME_1(Initialize, reserved)

inline bool Same(CallData const& lhs, CallData const& rhs, SAME_COMMON_ARGS)
{
    bool same = true
        && MEMBER_SAME(id)
        && (false
            || UNION_MEMBER_SAME(id, CALLID_LOAD(), args.Load)
            || UNION_MEMBER_SAME(id, CALLID(CORE, MarkEx), args.MarkEx)
            || UNION_MEMBER_SAME(id, CALLID(CORE, MarkA), args.MarkA)
            || UNION_MEMBER_SAME(id, CALLID(CORE, MarkW), args.MarkW)
            || UNION_MEMBER_SAME(id, CALLID(CORE, RangeStartEx), args.RangeStartEx)
            || UNION_MEMBER_SAME(id, CALLID(CORE, RangeStartA), args.RangeStartA)
            || UNION_MEMBER_SAME(id, CALLID(CORE, RangeStartW), args.RangeStartW)
            || UNION_MEMBER_SAME(id, CALLID(CORE, RangeEnd), args.RangeEnd)
            || UNION_MEMBER_SAME(id, CALLID(CORE, RangePushEx), args.RangePushEx)
            || UNION_MEMBER_SAME(id, CALLID(CORE, RangePushA), args.RangePushA)
            || UNION_MEMBER_SAME(id, CALLID(CORE, RangePushW), args.RangePushW)
            || UNION_MEMBER_SAME(id, CALLID(CORE, RangePop), args.RangePop)
            || UNION_MEMBER_SAME(id, CALLID(CORE, NameCategoryA), args.NameCategoryA)
            || UNION_MEMBER_SAME(id, CALLID(CORE, NameCategoryW), args.NameCategoryW)
            || UNION_MEMBER_SAME(id, CALLID(CORE, NameOsThreadA), args.NameOsThreadA)
            || UNION_MEMBER_SAME(id, CALLID(CORE, NameOsThreadW), args.NameOsThreadW)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainMarkEx), args.DomainMarkEx)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainRangeStartEx), args.DomainRangeStartEx)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainRangeEnd), args.DomainRangeEnd)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainRangePushEx), args.DomainRangePushEx)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainRangePop), args.DomainRangePop)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainResourceCreate), args.DomainResourceCreate)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainResourceDestroy), args.DomainResourceDestroy)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainNameCategoryA), args.DomainNameCategoryA)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainNameCategoryW), args.DomainNameCategoryW)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainRegisterStringA), args.DomainRegisterStringA)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainRegisterStringW), args.DomainRegisterStringW)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainCreateA), args.DomainCreateA)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainCreateW), args.DomainCreateW)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, DomainDestroy), args.DomainDestroy)
            || UNION_MEMBER_SAME(id, CALLID(CORE2, Initialize), args.Initialize)
            )
        ;

    VERBOSE_PRINT();
    return same;
}
DEFINE_EQ_NE_DEEP(CallData)

inline nvtxDomainHandle_t   PostInc(nvtxDomainHandle_t  & h) { auto v = h; h = reinterpret_cast<nvtxDomainHandle_t  >(reinterpret_cast<intptr_t>(h) + 1); return v; }
inline nvtxStringHandle_t   PostInc(nvtxStringHandle_t  & h) { auto v = h; h = reinterpret_cast<nvtxStringHandle_t  >(reinterpret_cast<intptr_t>(h) + 1); return v; }
inline nvtxResourceHandle_t PostInc(nvtxResourceHandle_t& h) { auto v = h; h = reinterpret_cast<nvtxResourceHandle_t>(reinterpret_cast<intptr_t>(h) + 1); return v; }
inline nvtxRangeId_t        PostInc(nvtxRangeId_t       & h) { return h++; }

struct Callbacks
{
    std::function<void(Call const&)> Default;
    std::function<void(int)> Load;

    std::function<void(const nvtxEventAttributes_t*)> MarkEx;
    std::function<void(const char*)> MarkA;
    std::function<void(const wchar_t*)> MarkW;
    std::function<nvtxRangeId_t(const nvtxEventAttributes_t*)> RangeStartEx;
    std::function<nvtxRangeId_t(const char*)> RangeStartA;
    std::function<nvtxRangeId_t(const wchar_t*)> RangeStartW;
    std::function<void(nvtxRangeId_t)> RangeEnd;
    std::function<int(const nvtxEventAttributes_t*)> RangePushEx;
    std::function<int(const char*)> RangePushA;
    std::function<int(const wchar_t*)> RangePushW;
    std::function<int()> RangePop;
    std::function<void(uint32_t, const char*)> NameCategoryA;
    std::function<void(uint32_t, const wchar_t*)> NameCategoryW;
    std::function<void(uint32_t, const char*)> NameOsThreadA;
    std::function<void(uint32_t, const wchar_t*)> NameOsThreadW;

    std::function<void(nvtxDomainHandle_t, const nvtxEventAttributes_t*)> DomainMarkEx;
    std::function<nvtxRangeId_t(nvtxDomainHandle_t, const nvtxEventAttributes_t*)> DomainRangeStartEx;
    std::function<void(nvtxDomainHandle_t, nvtxRangeId_t)> DomainRangeEnd;
    std::function<int(nvtxDomainHandle_t, const nvtxEventAttributes_t*)> DomainRangePushEx;
    std::function<int(nvtxDomainHandle_t)> DomainRangePop;
    std::function<nvtxResourceHandle_t(nvtxDomainHandle_t, nvtxResourceAttributes_t*)> DomainResourceCreate;
    std::function<void(nvtxResourceHandle_t)> DomainResourceDestroy;
    std::function<void(nvtxDomainHandle_t, uint32_t, const char*)> DomainNameCategoryA;
    std::function<void(nvtxDomainHandle_t, uint32_t, const wchar_t*)> DomainNameCategoryW;
    std::function<nvtxStringHandle_t(nvtxDomainHandle_t, const char*)> DomainRegisterStringA;
    std::function<nvtxStringHandle_t(nvtxDomainHandle_t, const wchar_t*)> DomainRegisterStringW;
    std::function<nvtxDomainHandle_t(const char*)> DomainCreateA;
    std::function<nvtxDomainHandle_t(const wchar_t*)> DomainCreateW;
    std::function<void(nvtxDomainHandle_t)> DomainDestroy;
    std::function<void(const void*)> Initialize;


    Callbacks(Callbacks const&) = default;
    Callbacks& operator=(Callbacks const&) = default;
    Callbacks(Callbacks&&) = default;
    Callbacks& operator=(Callbacks&&) = default;

    nvtxDomainHandle_t nextDomainHandle = reinterpret_cast<nvtxDomainHandle_t>(1);
    struct DomainData
    {
        int pushPopDepth = 0;
        nvtxRangeId_t nextRangeId = static_cast<nvtxRangeId_t>(1);
        nvtxStringHandle_t nextStringHandle = reinterpret_cast<nvtxStringHandle_t>(1);
        nvtxResourceHandle_t nextResourceHandle = reinterpret_cast<nvtxResourceHandle_t>(1);
    };
    std::map<nvtxDomainHandle_t, DomainData> domainData;

    Callbacks()
    : Default([](Call const&) {})
    , Load   ([&](int success) { Default(CALL_LOAD(success)); })
    // CORE
    , MarkEx       ([&](const nvtxEventAttributes_t* a) { Default(CALL(CORE, MarkEx       , a   )); })
    , MarkA        ([&](const char*                  a) { Default(CALL(CORE, MarkA        , a   )); })
    , MarkW        ([&](const wchar_t*               a) { Default(CALL(CORE, MarkW        , a   )); })
    , RangeStartEx ([&](const nvtxEventAttributes_t* a) { Default(CALL(CORE, RangeStartEx , a   )); return PostInc(domainData[nullptr].nextRangeId); })
    , RangeStartA  ([&](const char*                  a) { Default(CALL(CORE, RangeStartA  , a   )); return PostInc(domainData[nullptr].nextRangeId); })
    , RangeStartW  ([&](const wchar_t*               a) { Default(CALL(CORE, RangeStartW  , a   )); return PostInc(domainData[nullptr].nextRangeId); })
    , RangeEnd     ([&](nvtxRangeId_t                a) { Default(CALL(CORE, RangeEnd     , a   )); })
    , RangePushEx  ([&](const nvtxEventAttributes_t* a) { Default(CALL(CORE, RangePushEx  , a   )); return ++domainData[nullptr].pushPopDepth; })
    , RangePushA   ([&](const char*                  a) { Default(CALL(CORE, RangePushA   , a   )); return ++domainData[nullptr].pushPopDepth; })
    , RangePushW   ([&](const wchar_t*               a) { Default(CALL(CORE, RangePushW   , a   )); return ++domainData[nullptr].pushPopDepth; })
    , RangePop     ([&](                              ) { Default(CALL0(CORE, RangePop          )); return domainData[nullptr].pushPopDepth--; })
    , NameCategoryA([&](uint32_t a, const char*      b) { Default(CALL(CORE, NameCategoryA, a, b)); })
    , NameCategoryW([&](uint32_t a, const wchar_t*   b) { Default(CALL(CORE, NameCategoryW, a, b)); })
    , NameOsThreadA([&](uint32_t a, const char*      b) { Default(CALL(CORE, NameOsThreadA, a, b)); })
    , NameOsThreadW([&](uint32_t a, const wchar_t*   b) { Default(CALL(CORE, NameOsThreadW, a, b)); })
    // CORE2
    , DomainMarkEx         ([&](nvtxDomainHandle_t a, const nvtxEventAttributes_t* b) { Default(CALL(CORE2, DomainMarkEx         , a, b   )); })
    , DomainRangeStartEx   ([&](nvtxDomainHandle_t a, const nvtxEventAttributes_t* b) { Default(CALL(CORE2, DomainRangeStartEx   , a, b   )); return PostInc(domainData[a].nextRangeId); })
    , DomainRangeEnd       ([&](nvtxDomainHandle_t a, nvtxRangeId_t                b) { Default(CALL(CORE2, DomainRangeEnd       , a, b   )); })
    , DomainRangePushEx    ([&](nvtxDomainHandle_t a, const nvtxEventAttributes_t* b) { Default(CALL(CORE2, DomainRangePushEx    , a, b   )); return ++domainData[a].pushPopDepth; })
    , DomainRangePop       ([&](nvtxDomainHandle_t                                 a) { Default(CALL(CORE2, DomainRangePop       , a      )); return domainData[a].pushPopDepth--; })
    , DomainResourceCreate ([&](nvtxDomainHandle_t a, nvtxResourceAttributes_t*    b) { Default(CALL(CORE2, DomainResourceCreate , a, b   )); return PostInc(domainData[a].nextResourceHandle); })
    , DomainResourceDestroy([&](nvtxResourceHandle_t                               a) { Default(CALL(CORE2, DomainResourceDestroy, a      )); })
    , DomainNameCategoryA  ([&](nvtxDomainHandle_t a, uint32_t b, const char*      c) { Default(CALL(CORE2, DomainNameCategoryA  , a, b, c)); })
    , DomainNameCategoryW  ([&](nvtxDomainHandle_t a, uint32_t b, const wchar_t*   c) { Default(CALL(CORE2, DomainNameCategoryW  , a, b, c)); })
    , DomainRegisterStringA([&](nvtxDomainHandle_t a, const char*                  b) { Default(CALL(CORE2, DomainRegisterStringA, a, b   )); return PostInc(domainData[a].nextStringHandle); })
    , DomainRegisterStringW([&](nvtxDomainHandle_t a, const wchar_t*               b) { Default(CALL(CORE2, DomainRegisterStringW, a, b   )); return PostInc(domainData[a].nextStringHandle); })
    , DomainCreateA        ([&](const char*                                        a) { Default(CALL(CORE2, DomainCreateA        , a      )); return PostInc(nextDomainHandle); })
    , DomainCreateW        ([&](const wchar_t*                                     a) { Default(CALL(CORE2, DomainCreateW        , a      )); return PostInc(nextDomainHandle); })
    , DomainDestroy        ([&](nvtxDomainHandle_t                                 a) { Default(CALL(CORE2, DomainDestroy        , a      )); })
    , Initialize           ([&](const void*                                        a) { Default(CALL(CORE2, Initialize           , a      )); })
    {
    }
};

extern Callbacks g_callbacks;


