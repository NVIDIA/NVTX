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

#pragma once

// [Best practices for injection implementions]
// Set NVTX_NO_IMPL to make the NVTX headers define the API types and function
// prototypes only, not the inline impls.  Be sure on GCC to use -Wno-unused-function
// to avoid warnings for undefined static prototypes.
#define NVTX_NO_IMPL

// [Best practices for injection implementions]
// Microsoft's compiler issues warning 26812 when compiling a C-style enum in C++
// instead of using the new "enum class" style.  Since the NVTX headers are written in
// C, the enums defined there will trigger this warning.  Use this code to disable it.
#if defined(_MSC_VER)
#pragma warning (disable : 26812)
#endif
#include <nvtx3/nvToolsExt.h>

#include <type_traits>
#include <utility>
#include <tuple>
#include <limits>

namespace NvtxInjectionHelper {

//============ Generic utility functions ======================================

inline namespace detail_generic {

//--- maxVal ---

// Variadic alternative to std::max that doesn't need an initializer list,
// doesn't conflict with MSVC's #define for max, and has no trouble with
// constexpr usage.  Handles having zero parameters passed, returning
// std::numeric_limits<T>::min in that case, as long as the template
// parameter T is explicitly specified.  Takes arguments by value, which
// avoids the issue of returning a reference to something when called
// with no parameters.  Example uses:
//

template <typename T>
constexpr inline T maxVal() { return std::numeric_limits<T>::min(); }

template <typename T, typename... Rest>
constexpr inline T maxVal(T first, Rest... rest)
{
    T restMax = maxVal<T>(rest...);
    return (first > restMax) ? first : restMax;
}

//--- tuple size helper ---

// Generic utility for getting the size of a std::tuple, using its value
// as opposed to std::tuple_size<> which takes the tuple's type.  In a
// generic lambda where the parameter's type is "auto", it's extra work
// to figure out the type
template <typename... Ts>
constexpr inline size_t size_of_tuple(std::tuple<Ts...> const&)
{
    return sizeof...(Ts);
}

//--- tuple helpers to loop over items ---

// We need a way to call a function f on each element of a tuple, like this:
//
//     f(std::get<0>(t));
//     f(std::get<1>(t));
//     f(std::get<2>(t)); etc.
//
// We want something like this, where "Is" is a parameter pack of 0,1,2,etc.:
//
//     f(std::get<Is>(t))...;
//
// ...but parameter pack expansion is only allowed within the context of args
// to a function call or a braced init list.  We also must handle the case
// where the tuple is empty, we should discard the results of all the calls
// to f, even if it returns different types for each call.  Easiest way to
// do this is by forwarding the elements of the tuple as args to a helper
// function that calls f on each arg, like this:
//
//    for_each_in_parameter_pack(f, std::get<Is>(t)...);
//
// But we also want perfect forwarding of the function and the tuple.
// The following utilites "for_each_in_tuple", "for_each_in_tuple_helper",
// and "for_each_in_parameter_pack" are provided to allow code such as this
// "loop" over tuple elements.  Note that "thing" in each iteration can be
// a different type, because a tuple's elements may be different types, so
// generic lambdas are very convenient here:
//
//    for_each_in_tuple(tuple_of_things,
//        [](auto const& thing)
//        {
//            std::cout << thing << std::endl;
//        }
//    );

template<typename F>
inline void for_each_in_parameter_pack(F&& f) {}

template<typename F, typename First, typename... Rest>
inline void for_each_in_parameter_pack(F&& f, First const& first, Rest const&... rest)
{
    // Call f on the first argument, and explicitly discard the result by casting to void
    static_cast<void>(std::forward<F>(f)(first));

    // Recurse to call f on the rest of the arguments
    for_each_in_parameter_pack(std::forward<F>(f), rest...);
}

// Generic utility for calling a function f for each element of a tuple t
template<typename T, typename F, size_t... Is>
inline void for_each_in_tuple_helper(T const& t, F&& f, std::index_sequence<Is...>)
{
    for_each_in_parameter_pack(
        std::forward<F>(f),
        std::get<Is>(t)...
    );
}
template<typename... Ts, typename F>
inline void for_each_in_tuple(std::tuple<Ts...> const& t, F&& f)
{
    for_each_in_tuple_helper(t, std::forward<F>(f), std::make_index_sequence<sizeof...(Ts)>());
}

} // namespace detail_generic

//============ NVTX injection helper internal utilities =======================

inline namespace detail_nvtx {

//--- id_t ---
// Define generic integer type for holding all modules' callback id enum values.
// These are used as indexes into the handler arrays for each module.
using id_t = unsigned int;

//--- id_v ---
// Nickname for std::integral_constant, which is used for all callback enum values.
// Using an integral constant allows performing correctness checks at compile time,
// which is not possible in C++ with function parameter values, only their types.
// Including the value in the type works around this problem.
template <typename EnumT, EnumT EnumVal>
using id_v = std::integral_constant<EnumT, EnumVal>;

//--- NVTX_CBID ---
// Macro to succinctly turn an NVTX_CBID_* enum value into a compile-time constant,
// using std::integral_constant.  This makes it possible to perform correctness
// checks at compile time, for example ensuring a handler's signature is compatible
// with the NVTX API call it is being installed to handle.  Syntax is meant to look
// familiar.  For example, replace:
//     NVTX_CBID_CORE_MarkA
// with:
//     NVTX_CBID(CORE_MarkA)
// when passing CBID values to NvtxInjectionHelper::MakeHandlerTable.
#define NVTX_CBID(func) NvtxInjectionHelper::id_v<decltype(NVTX_CBID_##func), NVTX_CBID_##func>{}

//--- EnumTypeToModuleId ---
// Template variable to map from call id enum types to module id values (see nvtxTypes.h)
// For example, EnumTypeToModuleId<NVTX_CBID_CORE_MarkA> == NVTX_CB_MODULE_CORE.
template <typename EnumT>
constexpr static NvtxCallbackModule EnumTypeToModuleId = NVTX_CB_MODULE_INVALID;

template<> constexpr NvtxCallbackModule EnumTypeToModuleId<NvtxCallbackIdCore  > = NVTX_CB_MODULE_CORE;
template<> constexpr NvtxCallbackModule EnumTypeToModuleId<NvtxCallbackIdCuda  > = NVTX_CB_MODULE_CUDA;
template<> constexpr NvtxCallbackModule EnumTypeToModuleId<NvtxCallbackIdOpenCL> = NVTX_CB_MODULE_OPENCL;
template<> constexpr NvtxCallbackModule EnumTypeToModuleId<NvtxCallbackIdCudaRt> = NVTX_CB_MODULE_CUDART;
template<> constexpr NvtxCallbackModule EnumTypeToModuleId<NvtxCallbackIdCore2 > = NVTX_CB_MODULE_CORE2;
template<> constexpr NvtxCallbackModule EnumTypeToModuleId<NvtxCallbackIdSync  > = NVTX_CB_MODULE_SYNC;

//--- IdToModuleId ---
// Helper for EnumTypeToModuleId to convert directly from an integral_constant of a call id enum
// to its module id.  For example, since NVTX_CBID(CORE_MarkA) is an integral_constant, it cannot
// be used directly as in EnumTypeToModuleId<NVTX_CBID(CORE_MarkA)>, since NVTX_CBID(CORE_MarkA)'s
// type is std::integral_constant<NvtxCallbackIdCore, NVTX_CBID_CORE_MarkA>.  This helper extracts
// the enum's type from the integral_constant, allowing EnumConstToModuleId<NVTX_CBID(CORE_MarkA)>.
template <typename IdT>
constexpr static NvtxCallbackModule IdToModuleId = EnumTypeToModuleId<typename IdT::value_type>;


//--- IdToHandlerType
// Template using to map from call id values to matching function pointer types.
template <typename IdT> struct IdToHandlerType { using type = nullptr_t; };

// Macro for defining IdToHandlerType specializations for each id.
// mod = module, i.e. CORE, CORE2
// func = prefixless function name, i.e. MarkEx, DomainCreateA
// impl = impl or fakeimpl, depending on whether or not to use real types or the
//        nvtxTypes.h "fakeimpl" types, which don't depend on CUDA/OpenCL headers.
#define NVTX_ID_TO_TYPE(mod, func, impl) \
template <> struct IdToHandlerType<decltype(NVTX_CBID(mod##_##func))> { using type = nvtx##func##_##impl##_fntype; }

NVTX_ID_TO_TYPE(CORE, MarkEx       , impl);
NVTX_ID_TO_TYPE(CORE, MarkA        , impl);
NVTX_ID_TO_TYPE(CORE, MarkW        , impl);
NVTX_ID_TO_TYPE(CORE, RangeStartEx , impl);
NVTX_ID_TO_TYPE(CORE, RangeStartA  , impl);
NVTX_ID_TO_TYPE(CORE, RangeStartW  , impl);
NVTX_ID_TO_TYPE(CORE, RangeEnd     , impl);
NVTX_ID_TO_TYPE(CORE, RangePushEx  , impl);
NVTX_ID_TO_TYPE(CORE, RangePushA   , impl);
NVTX_ID_TO_TYPE(CORE, RangePushW   , impl);
NVTX_ID_TO_TYPE(CORE, RangePop     , impl);
NVTX_ID_TO_TYPE(CORE, NameCategoryA, impl);
NVTX_ID_TO_TYPE(CORE, NameCategoryW, impl);
NVTX_ID_TO_TYPE(CORE, NameOsThreadA, impl);
NVTX_ID_TO_TYPE(CORE, NameOsThreadW, impl);

NVTX_ID_TO_TYPE(CORE2, DomainMarkEx         , impl);
NVTX_ID_TO_TYPE(CORE2, DomainRangeStartEx   , impl);
NVTX_ID_TO_TYPE(CORE2, DomainRangeEnd       , impl);
NVTX_ID_TO_TYPE(CORE2, DomainRangePushEx    , impl);
NVTX_ID_TO_TYPE(CORE2, DomainRangePop       , impl);
NVTX_ID_TO_TYPE(CORE2, DomainResourceCreate , impl);
NVTX_ID_TO_TYPE(CORE2, DomainResourceDestroy, impl);
NVTX_ID_TO_TYPE(CORE2, DomainNameCategoryA  , impl);
NVTX_ID_TO_TYPE(CORE2, DomainNameCategoryW  , impl);
NVTX_ID_TO_TYPE(CORE2, DomainRegisterStringA, impl);
NVTX_ID_TO_TYPE(CORE2, DomainRegisterStringW, impl);
NVTX_ID_TO_TYPE(CORE2, DomainCreateA        , impl);
NVTX_ID_TO_TYPE(CORE2, DomainCreateW        , impl);
NVTX_ID_TO_TYPE(CORE2, DomainDestroy        , impl);
NVTX_ID_TO_TYPE(CORE2, Initialize           , impl);

#undef NVTX_ID_TO_TYPE

//--- CheckHandlerTypeMatchesId ---
// Compile-time check provides easy-to-read error if FuncT isn't compatible with EnumT
template <typename IdT, typename FuncT>
constexpr inline void CheckHandlerTypeMatchesId()
{
    using ExpectedFuncT = typename IdToHandlerType<IdT>::type;

    static_assert(std::is_same<ExpectedFuncT, FuncT>(),
        "NVTX Injection Helper: The provided handler function's signature does not match the NVTX API for the given call id.");
}

//--- Handler ---
// Represents id/handler pair for an NVTX call.  Provides:
//    - the call's id (NVTX_CBID_* enum values)
//    - handler function pointer
// Preserves the type of the function as a template parameter.
// Erases the type of the enum, so it's not module-specific anymore.
// Allows being constructed and placed into a container at compile time, then
// later at run time doing the run-time-only cast of the function pointer.
// This enables processing of ids to occur at compile time.
template <typename FuncT>
class Handler
{
public:
    id_t id;
    FuncT pfn;

    template <typename EnumT, EnumT EnumVal>
    constexpr Handler(id_v<EnumT, EnumVal> e, FuncT pfn_)
        : id(static_cast<id_t>(EnumVal)) // Erase enum's type
        , pfn(pfn_)
    {}

    NvtxFunctionPointer Address() const noexcept
    {
        return reinterpret_cast<NvtxFunctionPointer>(pfn);
    }
};

//--- MakeHandler ---
// "Make" function for Handler to automatically deduce types from parameters
template <typename IdT, typename FuncT>
constexpr inline Handler<FuncT> MakeHandler(IdT id_, FuncT func)
{
    CheckHandlerTypeMatchesId<IdT, FuncT>();
    return Handler<FuncT>(id_, func);
}

//--- ModuleHandlerTable ---
// Represents the set of Handlers for one module.  Provides:
//    - the module's id (NVTX_CB_MODULE_* enum values)
//    - iterable container of id/handler pairs (empty means skip getting etbl for module)
//    - highest call id value of handler in module (to confirm client has sufficient size)
//    - a method to assign all the stored handlers into a client's handler table
// These objects can be constructed at compile time, including the highest call id used.
template <NvtxCallbackModule mod, typename... Funcs>
class ModuleHandlerTable
{
public:
    using tuple_t = std::tuple<Handler<Funcs>...>;

    static constexpr NvtxCallbackModule moduleId = mod;
    tuple_t handlers;
    id_t highestIdUsed;

    constexpr ModuleHandlerTable(tuple_t t)
        : handlers(t)
        , highestIdUsed(FindHighestId(t))
    {}

    void AssignToClient(NvtxFunctionTable clientTable) const noexcept
    {
        for_each_in_tuple(handlers,
            [clientTable](auto const& handler)
            {
                if (handler.id != 0 && handler.pfn != nullptr)
                {
                    *clientTable[handler.id] = handler.Address();
                }
            }
        );
    }

private:
    template <size_t... Is>
    static constexpr id_t FindHighestIdHelper(tuple_t t, std::index_sequence<Is...>)
    {
        return maxVal<id_t>(std::get<Is>(t).id...);
    }

    static constexpr id_t FindHighestId(tuple_t t)
    {
        return FindHighestIdHelper(t, std::make_index_sequence<sizeof...(Funcs)>());
    }
};


//--- MakeModuleHandlerTuple ---
// MakeModuleHandlerTuple takes NvtxCallbackModule "mod" as a template parameter,
// and loops over pairs of arguments (an enum and a handler function), building a
// tuple of Handler objects for the enums that are in module "mod", and ignoring
// ones that aren't.  This lets the user pass in handlers for for all modules in
// one simple call, and we can build up separate handler tables for each module.
// MakeModuleHandlerTuple is recursive, peeling off two arguments in each recursive
// case, and having no args be the base case.  The recursive case has a pair of
// overloads for whether or not the enum's type matches "mod" or not.  Since these
// overloads are separate functions, it's mutual recursion, so both are declared
// first before the definitions.

// Base case: no more arguments
template <NvtxCallbackModule mod>
constexpr inline auto MakeModuleHandlerTuple()
{
    return std::tuple<>{};
}

// Prototypes of recursive cases -- needed since they can call each other
template <NvtxCallbackModule mod, typename IdT, typename FuncT,
    std::enable_if_t<IdToModuleId<IdT> == mod, int> = 0,
    typename... Args>
constexpr inline auto MakeModuleHandlerTuple(IdT, FuncT, Args...);

template <NvtxCallbackModule mod, typename IdT, typename FuncT,
    std::enable_if_t<IdToModuleId<IdT> != mod, int> = 0,
    typename... Args>
constexpr inline auto MakeModuleHandlerTuple(IdT, FuncT, Args...);

// Recursive case 1: enum's type matches mod, so add it to the tuple
template <NvtxCallbackModule mod, typename IdT, typename FuncT,
    std::enable_if_t<IdToModuleId<IdT> == mod, int>,
    typename... Args>
constexpr inline auto MakeModuleHandlerTuple(IdT id, FuncT f, Args... rest)
{
    // Verify types of id and function, using static_assert to provide a
    // clear compile error if the types don't meet the requirements.
    static_assert(IdToModuleId<IdT> != NVTX_CB_MODULE_INVALID,
        "MakeHandlerTable arguments must be pairs of IDs and handler functions.  IDs must be enums starting with NVTX_CBID_.  An invalid ID value was provided.");

    // Before adding this id/handler pair to the tuple, check to make sure
    // there's not already an entry in the tuple with the same id.  If so,
    // provide a clear compile-time error message.
    auto restTuple = MakeModuleHandlerTuple<mod>(rest...);

    return std::tuple_cat(
        std::make_tuple(MakeHandler(id, f)),
        restTuple);
}

// Recursive case 2: id is not in module, so fwd result from remaining args
template <NvtxCallbackModule mod, typename IdT, typename FuncT,
    std::enable_if_t<IdToModuleId<IdT> != mod, int>,
    typename... Args>
constexpr inline auto MakeModuleHandlerTuple(IdT id, FuncT f, Args... rest)
{
    return MakeModuleHandlerTuple<mod>(rest...);
}

//--- MakeModuleHandlerFromTuple ---
// Helper function for MakeModuleHandlerTable.  Coverts type of Handlers into
// a ModuleHandlerTable object.  This approach was simpler than building up the
// ModuleHandlerTable incrementally, since std::tuple_cat makes it so easy to
// build up a tuple.
template <NvtxCallbackModule mod, typename... Funcs>
constexpr inline auto MakeModuleHandlerFromTuple(std::tuple<Handler<Funcs>...> t)
{
    return ModuleHandlerTable<mod, Funcs...>(t);
}

//--- "Make" function for ModuleHandlerTable to automatically deduce type ---
// First, create a tuple of just the handlers in the argument list in module "mod".
// Uses the mutually-recursive MakeModuleHandlerTuple overloads, which only add
// handlers into the tuple if the module matches.  Then, MakeModuleHandlerFromTuple
// converts the tuple into a properly-typed ModuleHandlerTable object.
template <NvtxCallbackModule mod, typename... Args>
constexpr inline auto MakeModuleHandlerTable(Args... args)
{
    const auto handlerTuple = MakeModuleHandlerTuple<mod>(args...);
    return MakeModuleHandlerFromTuple<mod>(handlerTuple);
}

} // namespace detail_nvtx

//============ NVTX injection helper public interface =========================

// Define sentinel-value constants for use in handler implementations
namespace ReturnCodes {
    constexpr auto NVTX_TOOL_ATTACHED_UNUSED_RANGE_ID = static_cast<nvtxRangeId_t>(-1LL);
    constexpr int  NVTX_TOOL_ATTACHED_UNUSED_PUSH_POP_ID = -1;
    const     auto NVTX_TOOL_ATTACHED_UNUSED_DOMAIN_HANDLE = reinterpret_cast<nvtxDomainHandle_t>(-1LL);
    const     auto NVTX_TOOL_ATTACHED_UNUSED_STRING_HANDLE = reinterpret_cast<nvtxStringHandle_t>(-1LL);
    // Note: In C++20, use bit_cast instead of reinterpret_cast, so the handles
    // (which are pointer types) can also be made constexpr.
}

template <typename... Args>
constexpr inline auto MakeHandlerTable(Args... args)
{
    return std::make_tuple(
        MakeModuleHandlerTable<NVTX_CB_MODULE_CORE  >(args...),
        MakeModuleHandlerTable<NVTX_CB_MODULE_CUDA  >(args...),
        MakeModuleHandlerTable<NVTX_CB_MODULE_OPENCL>(args...),
        MakeModuleHandlerTable<NVTX_CB_MODULE_CUDART>(args...),
        MakeModuleHandlerTable<NVTX_CB_MODULE_CORE2 >(args...),
        MakeModuleHandlerTable<NVTX_CB_MODULE_SYNC  >(args...)
    );
}

enum class InstallResult
{
    Success,
    ExportTableVersionInfoMissing,
    ExportTableVersionInfoTooSmall,
    ClientVersionTooOld,
    ExportTableCallbacksMissing,
    ExportTableCallbacksTooSmall,
    ModuleNotSupported,
    ModuleTableTooSmall
};

template <typename HandlerTableT>
inline InstallResult InstallHandlers(
    NvtxGetExportTableFunc_t getExportTable,
    HandlerTableT const& injectionHandlerTable,
    std::ostringstream* errStream = nullptr,
    uint32_t* pVersion = nullptr)
{
    uint32_t version = 0;
    auto pVersionInfo =
        reinterpret_cast<const NvtxExportTableVersionInfo*>(getExportTable(NVTX_ETID_VERSIONINFO));
    if (!pVersionInfo)
    {
        if (errStream) *errStream
            << "Client NVTX instance doesn't support NVTX_ETID_VERSIONINFO";
        return InstallResult::ExportTableVersionInfoMissing;
    }

    if (pVersionInfo->struct_size < sizeof(*pVersionInfo))
    {
        if (errStream) *errStream
            << "NvtxExportTableVersionInfo structure size is " << pVersionInfo->struct_size
            << ", expected " << sizeof(*pVersionInfo) << "!";
        return InstallResult::ExportTableVersionInfoTooSmall;
    }

    version = pVersionInfo->version;
    if (version < 2)
    {
        if (errStream) *errStream
            << "client's NVTX version is " << version << ", expected 2+";
        return InstallResult::ClientVersionTooOld;
    }

    if (pVersion) *pVersion = version;

    auto pCallbacks =
        reinterpret_cast<const NvtxExportTableCallbacks*>(getExportTable(NVTX_ETID_CALLBACKS));
    if (!pCallbacks)
    {
        if (errStream) *errStream
            << "Client NVTX instance doesn't support NVTX_ETID_CALLBACKS";
        return InstallResult::ExportTableCallbacksMissing;
    }

    if (pCallbacks->struct_size < sizeof(*pCallbacks))
    {
        if (errStream) *errStream
            << "NvtxExportTableCallbacks structure size is " << pCallbacks->struct_size
            << ", expected " << sizeof(*pCallbacks) << "!";
        return InstallResult::ExportTableCallbacksTooSmall;
    }

#if defined(DEBUG) || true
    // Simple loop to print handler table internal details
    for_each_in_tuple(injectionHandlerTable,
        [](auto const& handlerModule)
        {
            auto count = size_of_tuple(handlerModule.handlers);
            printf("Module: %d   Count: %d  Highest: %d\n",
                static_cast<int>(handlerModule.moduleId),
                static_cast<int>(count),
                static_cast<int>(handlerModule.highestIdUsed));

            if (count > 0)
            {
                for_each_in_tuple(handlerModule.handlers,
                    [](auto const& handler)
                    {
                        auto addr = static_cast<long long>(handler.Address());
                        printf("    Id: %d  Address: 0x%llx\n",
                            static_cast<int>(handler.id), addr);
                    }
                );
            }
        }
    );
#endif

    // Loop over module handler tables and install handlers into client
    bool errors = false;
    for_each_in_tuple(injectionHandlerTable,
        [&](auto const& handlerModule)
        {
            NvtxFunctionTable clientTable = 0;
            unsigned int clientTableSize = 0;
            int success;

            if (handlerModule.moduleId == NVTX_CB_MODULE_INVALID) return;

            success = pCallbacks->GetModuleFunctionTable(handlerModule.moduleId, &clientTable, &clientTableSize);
            if (!success || !clientTable)
            {
                if (errStream) *errStream
                    << "Client NVTX instance doesn't support callback module with id " << handlerModule.moduleId;
                // TODO: return InstallResult::ModuleNotSupported;
                errors = true;
            }

            // Ensure client's table is new enough to support the function pointers we want to register
            if (clientTableSize <= handlerModule.highestIdUsed)
            {
                if (errStream) *errStream
                    << "Size of client NVTX instance's handler table with module id " << handlerModule.moduleId
                    << " too small.  Size is " << clientTableSize
                    << ", but injection needs to assign table[" << handlerModule.highestIdUsed << "]";
                // TODO: return InstallResult::ModuleTableTooSmall;
                errors = true;
            }

            handlerModule.AssignToClient(clientTable);
        }
    );

    if (errors) return InstallResult::ModuleNotSupported;

    return InstallResult::Success;
}

} // namespace NvtxInjectionHelper
