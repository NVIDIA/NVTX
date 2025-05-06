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

#ifndef NVTX_IMPL_GUARD
#error Never include this file directly -- it is automatically included by nvToolsExt.h (except when NVTX_NO_IMPL is defined).
#endif

#if defined(NVTX_AS_SYSTEM_HEADER)
#if defined(__clang__)
#pragma clang system_header
#elif defined(__GNUC__) || defined(__NVCOMPILER)
#pragma GCC system_header
#elif defined(_MSC_VER)
#pragma system_header
#endif
#endif

/* ---- Platform-independent helper definitions and functions ---- */

/* Prefer macros over inline functions to reduce symbol resolution at link time */

#if defined(_WIN32)
#define NVTX_PATHCHAR   wchar_t
#define NVTX_STR(x)     L##x
#define NVTX_GETENV     _wgetenv
#define NVTX_BUFSIZE    16384
#define NVTX_DLLHANDLE  HMODULE
#define NVTX_DLLOPEN(x) LoadLibraryW(x)
#define NVTX_DLLFUNC(h, x) NVTX_REINTERPRET_CAST(void(*)(void), GetProcAddress((h), (x)))
#define NVTX_DLLCLOSE   FreeLibrary
#define NVTX_DLLDEFAULT NVTX_NULLPTR
#define NVTX_YIELD()    SwitchToThread()
#define NVTX_MEMBAR()   MemoryBarrier()
#define NVTX_ATOMIC_WRITE_32(address, value) \
    InterlockedExchange(NVTX_REINTERPRET_CAST(volatile LONG*, (address)), (value))
#define NVTX_ATOMIC_CAS_32(old, address, exchange, comparand) \
    (old) = InterlockedCompareExchange(NVTX_REINTERPRET_CAST(volatile LONG*, (address)), (exchange), (comparand))
#elif defined(__GNUC__)
#define NVTX_PATHCHAR   char
#define NVTX_STR(x)     x
#define NVTX_GETENV     getenv
#define NVTX_BUFSIZE    16384
#define NVTX_DLLHANDLE  void*
#define NVTX_DLLOPEN(x) dlopen(x, RTLD_LAZY)
#define NVTX_DLLFUNC(h, x) dlsym((h), (x))
#define NVTX_DLLCLOSE   dlclose
#if !defined(__APPLE__)
#define NVTX_DLLDEFAULT NVTX_NULLPTR
#else
#define NVTX_DLLDEFAULT RTLD_DEFAULT
#endif
#define NVTX_YIELD()    sched_yield()
#define NVTX_MEMBAR()   __sync_synchronize()
/* Ensure full memory barrier for atomics, to match Windows functions */
#define NVTX_ATOMIC_WRITE_32(address, value) \
    __sync_synchronize(); *(address) = (value); __sync_synchronize()
#define NVTX_ATOMIC_CAS_32(old, address, exchange, comparand) \
    (old) = __sync_val_compare_and_swap((address), (comparand), (exchange))
#else
#error The library does not support your configuration!
#endif

/* NVTX_LOAD_SEQUENCE_VERSION macro
*
*  NVTX3 can update the search sequence used for finding a suitable injection library.
*  If multiple copies of the NVTX3 headers are included in the same translation unit,
*  the one included first sets the loader sequence.  If there is any problem where a
*  tool is expected to load, but is not loading, the app can test this macro to verify
*  which version of the search is being used.  Check if NVTX_LOAD_SEQUENCE_VERSION is
*  defined; if it is not, the version is 1.  Otherwise, the version is indicated by
*  the value of NVTX_LOAD_SEQUENCE_VERSION.
*
*  Version history:
*    1: NVTX3 initial implementation.  The search continues until a usable function
*       pointer is found.  If none is found, init aborts and rolls back anything it
*       did during the search (e.g. any loaded libraries are unloaded).  If a non-zero
*       function pointer is found, it is called.  If that function returns non-zero
*       ("true" in C), that indicates a tool successfully initialized.  If it returns
*       zero ("false"), the tool init was unsuccessful, so init aborts and rolls back
*       anything it did.  No further attempt is made to search for a different init
*       function if the first one found returns false.  The search order is:
*       - Check for env var NVTX_INJECTION64_PATH (or "32" in 32-bit process)
*         - Treat env var value as path to dynamic library, try loading it
*         - If it loads, try get the exported symbol "InitializeInjectionNvtx2"
*         - If this returns a non-null pointer, the search finishes here
*       - (Android only) Look for libNvtxInjection64.so (or "32" in 32-bit process)
*         - Must be in the /data/data/<package name>/files" directory
*         - Treat env var value as path to dynamic library, try loading it
*         - If it loads, try get the exported symbol "InitializeInjectionNvtx2"
*         - If this returns a non-null pointer, the search finishes here
*       Note: There were two other options partially implemented, but disabled.
*       - For supporting a pre-injected library on POSIX platforms, e.g. with
*         LD_PRELOAD, try using dlsym with a null module handle to get the init
*         function.  This was unconditionally disabled after finding cases where
*         a tool loaded multiple injections that supported NVTX, and couldn't
*         control which one was getting picked by the NVTX loader.
*       - (Linux only, not including Cygwin) Check for static injection using a
*         weak symbol.  This was implemented incorrectly, so it wasn't usable.
*
*    2: Fix the support for static injection libraries.  This is meant for cases
*       where dlopen is not supported or allowed, and the executable format has
*       support for weak symbols.  Tools may provide a static library with a
*       C-linkage symbol named "InitializeInjectionNvtx2_fnptr", whose type is
*       NvtxInitializeInjectionNvtxFunc_t, i.e. a function pointer to NVTX init
*       function.  If such a symbol is provided by a static library, the NVTX
*       loader's weak symbol will bind to it and call it for initialization.
*       Otherwise, the weak symbol will be defined by NVTX and default to null,
*       indicating no static injection library is present.  Static injection is
*       last in the load sequence, because it gives all the run-time methods of
*       injection to override a program's compiled-in tool without rebuilding the
*       program.  The search order is:
*       - Check for env var NVTX_INJECTION64_PATH (or "32" in 32-bit process)
*         - Treat env var value as path to dynamic library, try loading it
*         - If it loads, try get the exported symbol "InitializeInjectionNvtx2"
*         - If this returns a non-null pointer, the search finishes here
*       - (Android only) Look for libNvtxInjection64.so (or "32" in 32-bit process)
*         - Must be in the /data/data/<package name>/files" directory
*         - Treat env var value as path to dynamic library, try loading it
*         - If it loads, try get the exported symbol "InitializeInjectionNvtx2"
*         - If this returns a non-null pointer, the search finishes here
*       - (Currently disabled, experimental support for non-Windows) Use dlsym
*         with a null module handle to query the process-wide dynamic symbol
*         table for a function named "InitializeInjectionNvtx2Preinject".  The
*         symbol is different to prevent injections from being loaded this way
*         unless they choose to do so.
*         - If this returns a non-null pointer, the search finishes here
*       - (GCC-like compilers with ELF binary targets only) Check for static
*         injection using a weak symbol "InitializeInjectionNvtx2_fnptr".
*       If the default support choices in this header are not working as expected,
*       clients may now override load sequence support decisions by defining these
*       macros before including the NVTX header files:
*       - NVTX_SUPPORT_ENV_VARS
*       - NVTX_SUPPORT_DYNAMIC_INJECTION_LIBRARY
*       - NVTX_SUPPORT_ANDROID_INJECTION_LIBRARY_IN_PACKAGE
*       - NVTX_SUPPORT_ALREADY_INJECTED_LIBRARY
*       - NVTX_SUPPORT_STATIC_INJECTION_LIBRARY
*/
#define NVTX_LOAD_SEQUENCE_VERSION 2

#ifndef NVTX_SUPPORT_ALREADY_INJECTED_LIBRARY
/* Define this to 1 for platforms that where pre-injected libraries can be discovered. */
#if defined(_WIN32)
/* Windows has no process-wide table of dynamic library symbols, so this can't be supported. */
#define NVTX_SUPPORT_ALREADY_INJECTED_LIBRARY 0
#else
/* POSIX platforms allow calling dlsym on a null module to use the process-wide table.
*  Note: Still disabled in load sequence version 2.  Needs to support following the
*  RTLD_NEXT chain, and needs more testing before support can be enabled by default.*/
#define NVTX_SUPPORT_ALREADY_INJECTED_LIBRARY 0
#endif
#endif

#ifndef NVTX_SUPPORT_ENV_VARS
/* Define this to 1 for platforms that support environment variables */
/* TODO: Detect UWP, a.k.a. Windows Store app, and set this to 0. */
/* Try:  #if defined(WINAPI_FAMILY_PARTITION) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) */
#define NVTX_SUPPORT_ENV_VARS 1
#endif

#ifndef NVTX_SUPPORT_DYNAMIC_INJECTION_LIBRARY
/* Define this to 1 for platforms that support dynamic/shared libraries */
#define NVTX_SUPPORT_DYNAMIC_INJECTION_LIBRARY 1
#endif

#ifndef NVTX_SUPPORT_ANDROID_INJECTION_LIBRARY_IN_PACKAGE
#if defined(__ANDROID__)
#define NVTX_SUPPORT_ANDROID_INJECTION_LIBRARY_IN_PACKAGE 1
#else
#define NVTX_SUPPORT_ANDROID_INJECTION_LIBRARY_IN_PACKAGE 0
#endif
#endif

#ifndef NVTX_SUPPORT_STATIC_INJECTION_LIBRARY
/* On platforms that support weak symbols (i.e. non-Windows), injection libraries may
*  be statically linked into an application.  This is useful for platforms where dynamic
*  injection is not available.  Weak symbols not marked extern are definitions, not just
*  declarations.  They are guaranteed to be initialized to zero if no normal definitions
*  are found by the linker to override them.  This means the NVTX load sequence can safely
*  detect the presence of a static injection -- if InitializeInjectionNvtx2_fnptr is zero,
*  there is no static injection. */
#if defined(__GNUC__) && !defined(_WIN32) && !defined(__CYGWIN__)
#define NVTX_SUPPORT_STATIC_INJECTION_LIBRARY 1
#else
#define NVTX_SUPPORT_STATIC_INJECTION_LIBRARY 0
#endif
#endif

#if NVTX_SUPPORT_STATIC_INJECTION_LIBRARY && !defined(NVTX_STATIC_INJECTION_IMPL)
/* To make an NVTX injection library support static injection, it must do these things:
*  - Define InitializeInjectionNvtx2_fnptr as a normal symbol (not weak), pointing to
*    the implementation of InitializeInjectionNvtx2 (which does not need to be a
*    dynamic export if only supporting static injection).
*  - Define NVTX_STATIC_INJECTION_IMPL so the weak definition below is skipped.
*  - Compile the static injection files with -fPIC if they are to be linked with other
*    files compiled this way.  If you forget this, GCC will simply tell you to add it.
*  When building the application, there a few ways to link in a static injection:
*  - Compile the injection's source files normally, and include the .o files as inputs
*    to the linker.
*  - If the injection is provided as an archive (.a file), it will not resolve any
*    unresolved symbols, so the linker will skip it by default.  This can be fixed
*    by wrapping the static injection's name on the linker command line with options
*    to treat it differently.  For example:
*      gcc example.o libfoo.a -Wl,--whole-archive libinj-static.a -Wl,--no-whole-archive libbar.a
*    Note that libinj-static.a is bracketed by options to turn on "whole archive" and
*    then back off again afterwards, so libfoo.a and libbar.a are linked normally.
*  - In CMake, a static injection can be added with options like this:
*      target_link_libraries(app PRIVATE -Wl,--whole-archive inj-static -Wl,--no-whole-archive)
*/
__attribute__((weak)) NvtxInitializeInjectionNvtxFunc_t InitializeInjectionNvtx2_fnptr;
#endif

/* This function tries to find or load an NVTX injection library and get the
*  address of its InitializeInjection2 function.  If such a function pointer
*  is found, it is called, and passed the address of this NVTX instance's
*  nvtxGetExportTable function, so the injection can attach to this instance.
*  If the initialization fails for any reason, any dynamic library loaded will
*  be freed, and all NVTX implementation functions will be set to no-ops.  If
*  initialization succeeds, NVTX functions not attached to the tool will be set
*  to no-ops.  This is implemented as one function instead of several small
*  functions to minimize the number of weak symbols the linker must resolve.
*  Order of search is:
*  - Pre-injected library exporting InitializeInjectionNvtx2
*  - Loadable library exporting InitializeInjectionNvtx2
*      - Path specified by env var NVTX_INJECTION??_PATH (?? is 32 or 64)
*      - On Android, libNvtxInjection??.so within the package (?? is 32 or 64)
*  - Statically-linked injection library defining InitializeInjectionNvtx2_fnptr
*/
NVTX_LINKONCE_FWDDECL_FUNCTION int NVTX_VERSIONED_IDENTIFIER(nvtxInitializeInjectionLibrary)(void);
NVTX_LINKONCE_DEFINE_FUNCTION int NVTX_VERSIONED_IDENTIFIER(nvtxInitializeInjectionLibrary)(void)
{
    static const char initFuncName[] = "InitializeInjectionNvtx2";
#if NVTX_SUPPORT_ALREADY_INJECTED_LIBRARY
    static const char initFuncPreinjectName[] = "InitializeInjectionNvtx2Preinject";
#endif
    NvtxInitializeInjectionNvtxFunc_t init_fnptr = NVTX_NULLPTR;
    NVTX_DLLHANDLE injectionLibraryHandle = NVTX_DLLDEFAULT;
    int entryPointStatus = 0;

#if NVTX_SUPPORT_DYNAMIC_INJECTION_LIBRARY
    /* Try discovering dynamic injection library to load */
    {
#if NVTX_SUPPORT_ENV_VARS
        /* If env var NVTX_INJECTION64_PATH is set, it should contain the path
        *  to a 64-bit dynamic NVTX injection library (and similar for 32-bit). */
        const NVTX_PATHCHAR* const nvtxEnvVarName = (sizeof(void*) == 4)
            ? NVTX_STR("NVTX_INJECTION32_PATH")
            : NVTX_STR("NVTX_INJECTION64_PATH");
#endif /* NVTX_SUPPORT_ENV_VARS */
        NVTX_PATHCHAR injectionLibraryPathBuf[NVTX_BUFSIZE];
        const NVTX_PATHCHAR* injectionLibraryPath = NVTX_NULLPTR;

        /* Refer to this variable explicitly in case all references to it are #if'ed out */
        (void)injectionLibraryPathBuf;

#if NVTX_SUPPORT_ENV_VARS
        /* Disable the warning for getenv & _wgetenv -- this usage is safe because
        *  these functions are not called again before using the returned value. */
#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4996 )
#endif
        injectionLibraryPath = NVTX_GETENV(nvtxEnvVarName);
#if defined(_MSC_VER)
#pragma warning( pop )
#endif
#endif

#if NVTX_SUPPORT_ANDROID_INJECTION_LIBRARY_IN_PACKAGE
        if (!injectionLibraryPath)
        {
            const char *bits = (sizeof(void*) == 4) ? "32" : "64";
            char cmdlineBuf[32];
            char pkgName[PATH_MAX];
            int count;
            int pid;
            FILE *fp;
            size_t bytesRead;
            size_t pos;

            pid = NVTX_STATIC_CAST(int, getpid());
            count = snprintf(cmdlineBuf, sizeof(cmdlineBuf), "/proc/%d/cmdline", pid);
            if (count <= 0 || count >= NVTX_STATIC_CAST(int, sizeof(cmdlineBuf)))
            {
                NVTX_ERR("Path buffer too small for: /proc/%d/cmdline\n", pid);
                return NVTX_ERR_INIT_ACCESS_LIBRARY;
            }

            fp = fopen(cmdlineBuf, "r");
            if (!fp)
            {
                NVTX_ERR("File couldn't be opened: %s\n", cmdlineBuf);
                return NVTX_ERR_INIT_ACCESS_LIBRARY;
            }

            bytesRead = fread(pkgName, 1, sizeof(pkgName) - 1, fp);
            fclose(fp);
            if (bytesRead == 0)
            {
                NVTX_ERR("Package name couldn't be read from file: %s\n", cmdlineBuf);
                return NVTX_ERR_INIT_ACCESS_LIBRARY;
            }

            pkgName[bytesRead] = 0;

            /* String can contain colon as a process separator. In this case the package name is before the colon. */
            pos = 0;
            while (pos < bytesRead && pkgName[pos] != ':' && pkgName[pos] != '\0')
            {
                ++pos;
            }
            pkgName[pos] = 0;

            count = snprintf(injectionLibraryPathBuf, NVTX_BUFSIZE, "/data/data/%s/files/libNvtxInjection%s.so", pkgName, bits);
            if (count <= 0 || count >= NVTX_BUFSIZE)
            {
                NVTX_ERR("Path buffer too small for: /data/data/%s/files/libNvtxInjection%s.so\n", pkgName, bits);
                return NVTX_ERR_INIT_ACCESS_LIBRARY;
            }

            /* On Android, verify path is accessible due to aggressive file access restrictions. */
            /* For dlopen, if the filename contains a leading slash, then it is interpreted as a */
            /* relative or absolute pathname; otherwise it will follow the rules in ld.so. */
            if (injectionLibraryPathBuf[0] == '/')
            {
#if (__ANDROID_API__ < 21)
                int access_err = access(injectionLibraryPathBuf, F_OK | R_OK);
#else
                int access_err = faccessat(AT_FDCWD, injectionLibraryPathBuf, F_OK | R_OK, 0);
#endif
                if (access_err != 0)
                {
                    NVTX_ERR("Injection library path wasn't accessible [code=%s] [path=%s]\n", strerror(errno), injectionLibraryPathBuf);
                    return NVTX_ERR_INIT_ACCESS_LIBRARY;
                }
            }
            injectionLibraryPath = injectionLibraryPathBuf;
        }
#endif /* NVTX_SUPPORT_ANDROID_INJECTION_LIBRARY_IN_PACKAGE */

        /* At this point, injectionLibraryPath is specified if a dynamic
        *  injection library was specified by a tool. */
        if (injectionLibraryPath)
        {
            /* Load the injection library */
            injectionLibraryHandle = NVTX_DLLOPEN(injectionLibraryPath);
            if (!injectionLibraryHandle)
            {
                NVTX_ERR("Failed to load injection library\n");
                return NVTX_ERR_INIT_LOAD_LIBRARY;
            }
            else
            {
                /* Attempt to get the injection library's entry-point */
                init_fnptr = NVTX_REINTERPRET_CAST(NvtxInitializeInjectionNvtxFunc_t, NVTX_DLLFUNC(injectionLibraryHandle, initFuncName));
                if (!init_fnptr)
                {
                    NVTX_DLLCLOSE(injectionLibraryHandle);
                    NVTX_ERR("Failed to get address of function InitializeInjectionNvtx2 from injection library\n");
                    return NVTX_ERR_INIT_MISSING_LIBRARY_ENTRY_POINT;
                }
            }
        }
    }
#endif /* NVTX_SUPPORT_DYNAMIC_INJECTION_LIBRARY */

#if NVTX_SUPPORT_ALREADY_INJECTED_LIBRARY
    if (!init_fnptr)
    {
        /* Use POSIX global symbol chain to query for init function from any module */
        init_fnptr = NVTX_REINTERPRET_CAST(NvtxInitializeInjectionNvtxFunc_t, NVTX_DLLFUNC(NVTX_DLLDEFAULT, initFuncPreinjectName));
    }
#endif

#if NVTX_SUPPORT_STATIC_INJECTION_LIBRARY
    if (!init_fnptr)
    {
        /* Check weakly-defined function pointer.  A statically-linked injection can define this
        *  as a normal symbol and set it to the address of the NVTX init function -- this will
        *  provide a non-null value here.  If there is no other definition of this symbol, it
        *  will be null here. */
        if (InitializeInjectionNvtx2_fnptr)
        {
            init_fnptr = InitializeInjectionNvtx2_fnptr;
        }
    }
#endif

    /* At this point, if init_fnptr is not set, then no tool has specified
    *  an NVTX injection library -- return non-success result so all NVTX
    *  API functions will be set to no-ops. */
    if (!init_fnptr)
    {
        return NVTX_ERR_NO_INJECTION_LIBRARY_AVAILABLE;
    }

    /* Invoke injection library's initialization function.  If it returns
    *  0 (failure) and a dynamic injection was loaded, unload it. */
    entryPointStatus = init_fnptr(NVTX_VERSIONED_IDENTIFIER(nvtxGetExportTable));
    if (entryPointStatus == 0)
    {
        NVTX_ERR("Failed to initialize injection library -- initialization function returned 0\n");
        if (injectionLibraryHandle)
        {
            NVTX_DLLCLOSE(injectionLibraryHandle);
        }
        return NVTX_ERR_INIT_FAILED_LIBRARY_ENTRY_POINT;
    }

    return NVTX_SUCCESS;
}

NVTX_LINKONCE_DEFINE_FUNCTION void NVTX_VERSIONED_IDENTIFIER(nvtxInitOnce)(void)
{
    unsigned int old;
    if (NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).initState == NVTX_INIT_STATE_COMPLETE)
    {
        return;
    }

    NVTX_ATOMIC_CAS_32(
        old,
        &NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).initState,
        NVTX_INIT_STATE_STARTED,
        NVTX_INIT_STATE_FRESH);
    if (old == NVTX_INIT_STATE_FRESH)
    {
        int result;
        int forceAllToNoops;

        /* Load & initialize injection library -- it will assign the function pointers */
        result = NVTX_VERSIONED_IDENTIFIER(nvtxInitializeInjectionLibrary)();

        /* Set all pointers not assigned by the injection to null */
        forceAllToNoops = result != NVTX_SUCCESS; /* Set all to null if injection init failed */
        NVTX_VERSIONED_IDENTIFIER(nvtxSetInitFunctionsToNoops)(forceAllToNoops);

        /* Signal that initialization has finished, so now the assigned function pointers will be used */
        NVTX_ATOMIC_WRITE_32(
            &NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).initState,
            NVTX_INIT_STATE_COMPLETE);
    }
    else /* Spin-wait until initialization has finished */
    {
        NVTX_MEMBAR();
        while (NVTX_VERSIONED_IDENTIFIER(nvtxGlobals).initState != NVTX_INIT_STATE_COMPLETE)
        {
            NVTX_YIELD();
            NVTX_MEMBAR();
        }
    }
}
