/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/* This file is a reference implementation of an NVTXW backend loader.
 * Applications are free to use this loader as-is, adapt it, or replace it.
 *
 * The loader is its own translation unit and does not provide the NVTX
 * core/payload/counter implementations, so opt out of them before including
 * the writer header. */
#ifndef NVTX_NO_IMPL
#define NVTX_NO_IMPL
#endif
#include <nvtxw3/nvtxw3_loader.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(_QNX_SOURCE)
#include <signal.h>
#else
#include <sys/signal.h>
#endif
#include <sys/stat.h>
#include <sys/wait.h>
#endif

#if defined(__APPLE__)
#include <libproc.h>
#endif

/*-------------------------------------------------------------*/
/* Path string helpers -- implement here to avoid dependencies */

#if defined(_WIN32)
static const char pathSep = '\\';
#if defined(NVTXW_TEST_PATH_UTILITIES)
static const char pathDelimiter = ';';
#endif
static const size_t initialPathBufSize = MAX_PATH; /* Grows if not big enough */
#define NVTXW_DLLHANDLE  HMODULE
#define NVTXW_DLLOPEN(x) LoadLibraryA(x)
/* GetProcAddress returns FARPROC (a function pointer).  MSVC warns on
 * function-pointer <-> void* casts (C4054/C4055); cast directly there.
 * MinGW/GCC still need the void* intermediate for -Wcast-function-type. */
#if defined(_MSC_VER)
#define NVTXW_DLLFUNC_AS(type, h, name) ((type)GetProcAddress((h), (name)))
#else
#define NVTXW_DLLFUNC_AS(type, h, name) ((type)(void*)GetProcAddress((h), (name)))
#endif
#define NVTXW_DLLCLOSE   FreeLibrary
#else
static const char pathSep = '/';
#if defined(NVTXW_TEST_PATH_UTILITIES)
static const char pathDelimiter = ':';
#endif
static const size_t initialPathBufSize = 260; /* Grows if not big enough */
#define NVTXW_DLLHANDLE  void*
#define NVTXW_DLLOPEN(x) dlopen(x, RTLD_LAZY)
/* Cast via void* so GCC -Wcast-function-type accepts dlsym's void* return. */
#define NVTXW_DLLFUNC_AS(type, h, name) ((type)(void*)dlsym((h), (name)))
#define NVTXW_DLLCLOSE   dlclose
#endif

static size_t MinSizeT(size_t a, size_t b)
{
    return a < b ? a : b;
}

#if defined(NVTXW_TEST_PATH_UTILITIES)
/* If native path separator is not forward slash (e.g. backslash on Windows),
 * do in-place conversion of forward slashes to native path separator. */
static void ForwardSlashesToNative(char* path)
{
#if _WIN32
    char* cur;
    if (!path)
    {
        return;
    }
    for (cur = path; *cur; ++cur)
    {
        if (*cur == '/')
        {
            *cur = pathSep;
        }
    }
#else
    (void)path;
#endif
}
#endif

/* Take pointers to string buffer begin/end.  End must equal begin + strlen(begin),
 * or NULL, in which case it will be set to begin + strlen(begin).
 * Remove trailing slashes in-place by overwriting first trailing slash with null. */
static void StripTrailingSlashes(char* path)
{
    char* newPathEnd;
    char* pathEnd = path + strlen(path);

    newPathEnd = pathEnd;
    while (newPathEnd != path)
    {
        char* cur = newPathEnd - 1;
        if (*cur != pathSep)
        {
            break;
        }
        newPathEnd = cur;
    }
    if (newPathEnd != pathEnd)
    {
        *newPathEnd = '\0';
    }
}

/* Take pointers to string buffer begin/end.  End must equal begin + strlen(begin),
 * or NULL, in which case it will be set to begin + strlen(begin).
 * Remove leading slashes in-place by memmove-ing from first character after leading
 * slashes to beginning of buffer, including null terminator. */
#if defined(NVTXW_TEST_PATH_UTILITIES)
static char* AfterLeadingSlashes(char* cur)
{
    while (*cur && *cur == pathSep)
    {
        ++cur;
    }
    return cur;
}
#endif
static const char* AfterLeadingSlashesConst(const char* cur)
{
    while (*cur && *cur == pathSep)
    {
        ++cur;
    }
    return cur;
}

#if defined(NVTXW_TEST_PATH_UTILITIES)
/* Take pointers to string buffer begin/end.  End must equal begin + strlen(begin),
 * or NULL, in which case it will be set to begin + strlen(begin).
 * Remove leading slashes in-place by memmove-ing from first character after leading
 * slashes to beginning of buffer, including null terminator. */
static void StripLeadingSlashes(char* path)
{
    char* afterSlashes = AfterLeadingSlashes(path);
    if (afterSlashes != path)
    {
        size_t sizeAfterSlashesWithNull = strlen(afterSlashes) + 1;
        memmove(path, afterSlashes, sizeAfterSlashesWithNull);
    }
}
#endif

/* Returns pointer to heap-allocated copy of input, must be freed with free(). */
static char* DuplicateHeapString(const char* str)
{
    size_t lenWithNull;
    char* result;

    if (!str)
    {
        return NULL;
    }

    lenWithNull = strlen(str) + 1;
    result = (char*)malloc(lenWithNull);
    if (!result)
    {
        return NULL;
    }
    memcpy(result, str, lenWithNull);
    return result;
}

#if defined(NVTXW_TEST_PATH_UTILITIES)
/* Take pointers to string buffer begin/end.  End must equal begin + strlen(begin),
 * or NULL, in which case it will be set to begin + strlen(begin).
 * Returns pointer to heap-allocated copy of input, must be freed with free(). */
static char* AssignHeapString(char* lhs, const char* rhs)
{
    size_t lenWithNull;

    if (!rhs)
    {
        return NULL;
    }

    lenWithNull = strlen(rhs) + 1;
    lhs = (char*)realloc(lhs, lenWithNull);
    memcpy(lhs, rhs, lenWithNull);
    return lhs;
}

/* Take pointers to string buffer begin/end.  End must equal begin + strlen(begin),
 * or NULL, in which case it will be set to begin + strlen(begin).
 * Returns pointer to heap-allocated copy of input, must be freed with free(). */
static char* MakeHeapString(const char* str)
{
    return AssignHeapString(NULL, str);
}

static char* MakeHeapStringWithNativeSlashes(const char* str)
{
    char* buf = AssignHeapString(NULL, str);
    ForwardSlashesToNative(buf);
    return buf;
}

/* Take pointer to a HeapString (lhs) and any C string (rhs), append rhs to lhs,
 * reallocating the heap memory for lhs if necessary.  Returns pointer to result
 * HeapString, which may or may not be the same pointer passed in as lhs.
 * HeapString must be freed with free(). */
static char* AppendToHeapString(char* lhs, const char* rhs)
{
    size_t lenLhs, lenRhs;
    lenLhs = strlen(lhs);
    lenRhs = strlen(rhs);
    if (lenRhs == 0)
    {
        return lhs;
    }
    lhs = (char*)realloc(lhs, lenLhs + lenRhs + 1);
    memcpy(lhs + lenLhs, rhs, lenRhs + 1);
    return lhs;
}
#endif

/* Take pointer to a HeapString (lhs) and any C string (rhs), append rhs to lhs,
 * with a path separator between them, reallocating the heap memory for lhs if
 * necessary.  If rhs is null or empty, then the result is lhs unmodified.  If
 * lhs is null or empty and rhs is not, then the result is a path separator
 * followed by rhs.  Returns pointer to result HeapString, which may or may not
 * be the same pointer passed in as lhs.  HeapString must be freed with free(). */
static char* AppendToHeapStringWithSep(char* lhs, const char* rhs)
{
    size_t lenLhs, lenRhs;
    lenLhs = strlen(lhs);
    lenRhs = strlen(rhs);
    if (lenRhs == 0)
    {
        return lhs;
    }
    lhs = (char*)realloc(lhs, lenLhs + lenRhs + 2);
    lhs[lenLhs] = pathSep;
    memcpy(lhs + lenLhs + 1, rhs, lenRhs + 1);
    return lhs;
}

/* dir is a HeapString.  If dir is empty or just slashes, result will be a
 * path relative to the root, i.e. beginning with a path separator.
 * relativePath must be a valid relative path (not empty, not just slashes).
 * Returns pointer to result HeapString, which may or may not be the same
 * pointer passed in as lhs.  HeapString must be freed with free(). */
static char* AppendToPathHeapString(char* dir, const char* relativePath)
{
    const char* relPathAfterLeadingSlashes;
    relPathAfterLeadingSlashes = AfterLeadingSlashesConst(relativePath);
    StripTrailingSlashes(dir);
    return AppendToHeapStringWithSep(dir, relPathAfterLeadingSlashes);
}

#if defined(NVTXW_TEST_PATH_UTILITIES)
static int HasSlashes(const char* cur)
{
    for (; *cur; ++cur)
    {
        if (*cur == pathSep)
        {
            return 1;
        }
    }
    return 0;
}

static int HasTrailingSlash(const char* str)
{
    size_t len = strlen(str);
    if (len == 0)
    {
        return 0;
    }
    return str[len - 1] == pathSep;
}
#endif

static char* GetCurrentWorkingDir(void)
{
#if defined(_WIN32)
    DWORD size;
    char* buf;

    /* Returns size including space for null terminator */
    size = GetCurrentDirectoryA(0, NULL);
    if (size == 0) return NULL;
    buf = (char*)malloc(size);
    if (!buf) return NULL;
    if (GetCurrentDirectoryA(size, buf) == 0)
    {
        free(buf);
        return NULL;
    }
    return buf;
#else
    size_t size = initialPathBufSize;
    char* buf;
    char* tmp;

    buf = (char*)malloc(size);
    if (!buf)
    {
        return NULL;
    }
    while (!getcwd(buf, size))
    {
        int getcwdErrno = errno;
        if (getcwdErrno != ERANGE)
        {
            free(buf);
            return NULL;
        }
        size *= 2;
        tmp = (char*)realloc(buf, size);
        if (!tmp)
        {
            free(buf);
            return NULL;
        }
        buf = tmp;
    }
    tmp = (char*)realloc(buf, strlen(buf) + 1);
    if (tmp)
    {
        buf = tmp;
    }
    return buf;
#endif
}

#if defined(NVTXW_TEST_PATH_UTILITIES)
/* Take pointer to string buffer of possibly-relative path, and returns
 * equivalent absolute path.  Input path must not be empty.
 * Returns pointer to heap-allocated string, must be freed with free(). */
static char* AbsolutePath(const char* path)
{
#if defined(_WIN32)
    size_t size;
    char* buf;

    if (!path)
    {
        return NULL;
    }

    /* Returns size including space for null terminator */
    size = (size_t)GetFullPathNameA(path, 0, NULL, NULL);
    if (size == 0) return NULL;
    buf = (char*)malloc(size);
    if (!buf) return NULL;
    if (GetFullPathNameA(path, (DWORD)size, buf, NULL) == 0)
    {
        free(buf);
        return NULL;
    }
    return buf;
#else
    if (!path)
    {
        return NULL;
    }

    return path[0] == pathSep
           ? MakeHeapString(path) /* Absolute already */
           : AppendToPathHeapString(GetCurrentWorkingDir(), path);
#endif
}
#endif

/* Take pointer to heap string of path, and modifies it in-place to be its
 * parent directory, i.e. the directory containing the input file/directory.
 * String is shortened, but not reallocated, permitting possibly faster
 * appending of different path later.  Returns the pointer passed in without
 * modifying it for convenient chaining of path functions.  If input path is
 * NULL, NULL is returned.  If input is an empty string, or root directory,
 * the heap string will be set to an empty string to indicate there is no
 * parent directory.  Returned pointer to heap-allocated string must be
 * freed with free(). */
static char* ToParentDir(char* path)
{
    char* cur;

    if (!path)
    {
        return NULL;
    }

    StripTrailingSlashes(path);

    for (cur = path + strlen(path); cur >= path; --cur)
    {
        if (*cur == pathSep)
        {
            /* Found the last slash */
            if (cur == path)
            {
                /* Special case -- last slash is first character
                 * in buffer.  Trailing slashes were trimmed first,
                 * so this can only occur when ParentDir should
                 * return the root directory.  This is the only
                 * case where we want to keep the slash we found,
                 * so write the null terminator after the slash. */
                *(cur + 1) = '\0';
            }
            else
            {
                /* Change slash to null, terminating the string
                 * before the last slash */
                *cur = '\0';
            }
            return path;
        }
    }

    /* No slashes found, so there's no parent directory.  Assign empty
     * string by nulling first character, which is safe because all heap
     * strings must be at least one byte long. */
    path[0] = '\0';
    return path;
}

#if defined(NVTXW_TEST_PATH_UTILITIES)
/* Take pointer to string buffer of path, and returns the parent directory,
 * i.e. the directory containing the input file/directory.  If input path is
 * NULL, empty string, or root directory, NULL is returned to indicate there
 * is no parent directory, so return value must be NULL-checked.
 * Returns pointer to heap-allocated string, must be freed with free(). */
static char* ParentDir(const char* path)
{
    char* buf;

    if (!path)
    {
        return NULL;
    }

    buf = ToParentDir(MakeHeapString(path));

    if (strlen(buf) == 0)
    {
        /* No slashes found, so there's no parent directory */
        free(buf);
        return NULL;
    }
    else
    {
        return buf;
    }
}

static int PathExists(const char* path)
{
#if defined(_WIN32)
    DWORD result = GetFileAttributesA(path);
    return result != INVALID_FILE_ATTRIBUTES;
#else
    int result = access(path, F_OK);
    return result != -1;
#endif
}
#endif

/* Return a heap string containing the full path of the current process's
 * executable file.  Buffer allocated may be a little larger than the path
 * string it contains, and is not realloc'ed to fit since typical usage of
 * this function involves getting the parent directory and appending to it.
 * Returned pointer to heap-allocated string must be freed with free(). */
static char* GetCurrentProcessPath(void)
{
    char* buf;
#if defined(_WIN32)
    {
        size_t size = initialPathBufSize;
        DWORD newSize;
        buf = NULL;
        while (1)
        {
            char* tmp;
            /* GetModuleFileNameA takes a DWORD length. On Win64, size_t can
             * exceed MAXDWORD; on Win32 the types are the same width so the
             * bound is redundant and triggers -Wtautological-type-limit-compare. */
#if defined(_WIN64)
            if (size > (size_t)MAXDWORD)
            {
                free(buf);
                return NULL;
            }
#endif
            tmp = (char*)realloc(buf, size);
            if (!tmp)
            {
                free(buf);
                return NULL;
            }
            buf = tmp;
            newSize = GetModuleFileNameA(NULL, buf, (DWORD)size);
            if (newSize == 0)
            {
                free(buf);
                return NULL;
            }
            if (newSize < size)
            {
                break;
            }
            if (size > (size_t)MAXDWORD / 2)
            {
                free(buf);
                return NULL;
            }
            size *= 2;
        }
    }
#elif defined(__APPLE__)
    {
        int size;
        pid_t pid = getpid();
        buf = (char*)malloc(PROC_PIDPATHINFO_MAXSIZE);
        if (!buf) return NULL;
        size = proc_pidpath(pid, buf, PROC_PIDPATHINFO_MAXSIZE);
        if (size <= 0)
        {
            free(buf);
            return NULL;
        }
    }
#elif defined(__QNX__)
    {
        size_t size = fpathconf(0, _PC_MAX_INPUT);
        if (size <= 0)
        {
            size = 4096;
        }
        ++size;
        buf = (char*)malloc(size);
        if (!buf) return NULL;
        if (!_cmdname(buf))
        {
            free(buf);
            return NULL;
        }
    }
#else
    {
        size_t size = initialPathBufSize;
        ssize_t bytesReadSigned;
        size_t bytesRead;
        const char* linkName = "/proc/self/exe";
        buf = NULL;
        while (1)
        {
            char* tmp = (char*)realloc(buf, size);
            if (!tmp)
            {
                free(buf);
                return NULL;
            }
            buf = tmp;
            bytesReadSigned = readlink(linkName, buf, size);
            if (bytesReadSigned < 0)
            {
                free(buf);
                return NULL;
            }
            bytesRead = (size_t)bytesReadSigned;
            if (bytesRead < size)
            {
                break;
            }
            if (size > ((size_t)-1) / 2)
            {
                free(buf);
                return NULL;
            }
            size *= 2;
        }
        buf[bytesRead] = '\0';
    }
#endif
    return buf;
}

static char* GetCurrentProcessDir(void)
{
    return ToParentDir(GetCurrentProcessPath());
}

/*-------------------------------------------------------------*/
/* Backend loader helpers */

static nvtxwResultCode_t InitLibraryFilename(
    const char* filename,                /* required */
    nvtxwGetInterface_t* getInterfaceFunc, /* already null-checked */
    void** moduleHandle)                 /* optional */
{
    /* filename is the path of the library to load */
    NVTXW_DLLHANDLE hModule;
    nvtxwGetInterface_t pfnGetInterface;

    *getInterfaceFunc = NULL;
    if (moduleHandle)
    {
        *moduleHandle = NULL;
    }

    if (!filename)
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }

    hModule = NVTXW_DLLOPEN(filename);
    if (!hModule)
    {
        return NVTXW_RESULT_LIBRARY_LOAD_FAILED;
    }

    /* Resolving the single backend entry point is what validates the library.
     * The loader does not call into the backend; the caller invokes the
     * returned function pointer to request an interface table. */
    pfnGetInterface = NVTXW_DLLFUNC_AS(
        nvtxwGetInterface_t, hModule, NVTXW_GET_INTERFACE_SYMBOL_NAME);
    if (!pfnGetInterface)
    {
        NVTXW_DLLCLOSE(hModule);
        return NVTXW_RESULT_LIBRARY_SYMBOL_MISSING;
    }

    /* Success - now write to output params */
    *getInterfaceFunc = pfnGetInterface;
    if (moduleHandle)
    {
        void* mod = (void*)hModule;
        *moduleHandle = mod;
    }

    return NVTXW_RESULT_SUCCESS;
}

static nvtxwResultCode_t InitSearchDefault(
    nvtxwGetInterface_t* getInterfaceFunc, /* already null-checked */
    void** moduleHandle)                 /* optional */
{
    nvtxwResultCode_t result;
    char* filename;
    char* dir;

    /* 1. Directory of current process's executable */
    dir = GetCurrentProcessDir();
    /* AppendToPathHeapString takes ownership of dir and frees/reallocates it */
    filename = dir
        ? AppendToPathHeapString(dir, NVTXW_LIB_FILENAME_DEFAULT)
        : DuplicateHeapString(NVTXW_LIB_FILENAME_DEFAULT);
    result = InitLibraryFilename(filename, getInterfaceFunc, moduleHandle);
    free(filename);
    if (result == NVTXW_RESULT_SUCCESS)
    {
        return NVTXW_RESULT_SUCCESS;
    }

    /* 2. Standard search paths for dynamic libraries */
    result = InitLibraryFilename(
        NVTXW_LIB_FILENAME_DEFAULT, getInterfaceFunc, moduleHandle);
    if (result == NVTXW_RESULT_SUCCESS)
    {
        return NVTXW_RESULT_SUCCESS;
    }

    /* 3. Current working directory (may not be included in standard search paths) */
    dir = GetCurrentWorkingDir();
    /* AppendToPathHeapString takes ownership of dir and frees/reallocates it */
    filename = dir
        ? AppendToPathHeapString(dir, NVTXW_LIB_FILENAME_DEFAULT)
        : DuplicateHeapString(NVTXW_LIB_FILENAME_DEFAULT);
    result = InitLibraryFilename(filename, getInterfaceFunc, moduleHandle);
    free(filename);
    if (result == NVTXW_RESULT_SUCCESS)
    {
        return NVTXW_RESULT_SUCCESS;
    }

    /* No usable backend found */
    return NVTXW_RESULT_LIBRARY_NOT_FOUND;
}

/* #define NVTXW_TEST_PATH_UTILITIES */
#if defined(NVTXW_TEST_PATH_UTILITIES)
#include <test_path_utilities.h>
#endif

NVTXW_DECLSPEC nvtxwResultCode_t nvtxwLoad(
    const char* library,
    nvtxwGetInterface_t* getInterfaceFunc,
    void** moduleHandle)
{
    nvtxwResultCode_t result;
    const char* envLibrary;

#if defined(NVTXW_TEST_PATH_UTILITIES)
    TestPathUtilities();
#endif

    if (!getInterfaceFunc)
    {
        return NVTXW_RESULT_INVALID_ARGUMENT;
    }

    /* Resolution priority (see nvtxw3_loader.h):
     *   1. NVTXW3_LIBRARY environment variable (authoritative, no fallback)
     *   2. the `library` argument (no fallback)
     *   3. the built-in default search */
    /* Disable the MSVC deprecation warning for getenv -- this usage is safe
     * because the returned value is used before any subsequent call. */
#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4996 )
#endif
    envLibrary = getenv(NVTXW_LIBRARY_ENV_VAR);
#if defined(_MSC_VER)
#pragma warning( pop )
#endif
    if (envLibrary && envLibrary[0])
    {
        result = InitLibraryFilename(envLibrary, getInterfaceFunc, moduleHandle);
    }
    else if (library)
    {
        result = InitLibraryFilename(library, getInterfaceFunc, moduleHandle);
    }
    else
    {
        result = InitSearchDefault(getInterfaceFunc, moduleHandle);
    }

    if (result == NVTXW_RESULT_SUCCESS && !*getInterfaceFunc)
    {
        return NVTXW_RESULT_FAILED;
    }

    return result;
}

NVTXW_DECLSPEC void nvtxwUnload(void* moduleHandle)
{
    nvtxwFinalize_t pfnFinalize;
    NVTXW_DLLHANDLE hModule = (NVTXW_DLLHANDLE)moduleHandle;

    if (!hModule)
    {
        return;
    }

    pfnFinalize = NVTXW_DLLFUNC_AS(
        nvtxwFinalize_t, hModule, NVTXW_FINALIZE_SYMBOL_NAME);
    if (pfnFinalize)
    {
        pfnFinalize();
    }

    NVTXW_DLLCLOSE(hModule);
}

NVTXW_DECLSPEC size_t nvtxwGetError(
    nvtxwResultCode_t code,
    char* messageOut,
    size_t messageOutLen)
{
    const char* errBuf;
    size_t errBufSize;
    size_t copyErrLen;

    if (!messageOut || messageOutLen == 0)
    {
        return 0;
    }

    switch (code)
    {
    case NVTXW_RESULT_SUCCESS:
    {
        static const char str[] = "success";
        errBuf = str;
        errBufSize = sizeof(str);
        break;
    }
    case NVTXW_RESULT_FAILED:
    {
        static const char str[] = "failed";
        errBuf = str;
        errBufSize = sizeof(str);
        break;
    }
    case NVTXW_RESULT_INVALID_ARGUMENT:
    {
        static const char str[] = "invalid argument";
        errBuf = str;
        errBufSize = sizeof(str);
        break;
    }
    case NVTXW_RESULT_LIBRARY_NOT_FOUND:
    {
        static const char str[] = "library not found";
        errBuf = str;
        errBufSize = sizeof(str);
        break;
    }
    case NVTXW_RESULT_LIBRARY_SYMBOL_MISSING:
    {
        static const char str[] = "library symbol missing: ";
        errBuf = str;
        errBufSize = sizeof(str);
        break;
    }
    case NVTXW_RESULT_LIBRARY_LOAD_FAILED:
    {
        static const char str[] = "library load failed: ";
        errBuf = str;
        errBufSize = sizeof(str);
        break;
    }
    case NVTXW_RESULT_INTERFACE_VERSION_NOT_SUPPORTED:
    {
        static const char str[] = "interface ID not supported";
        errBuf = str;
        errBufSize = sizeof(str);
        break;
    }
    case NVTXW_RESULT_NOT_SUPPORTED:
    {
        static const char str[] = "operation/feature not supported";
        errBuf = str;
        errBufSize = sizeof(str);
        break;
    }
    default:
    {
        static const char str[] = "unknown result code";
        errBuf = str;
        errBufSize = sizeof(str);
        break;
    }
    }

    copyErrLen = MinSizeT(errBufSize, messageOutLen) - 1;
    memcpy(messageOut, errBuf, copyErrLen);
    messageOut[copyErrLen] = '\0';

    if (code == NVTXW_RESULT_LIBRARY_LOAD_FAILED ||
        code == NVTXW_RESULT_LIBRARY_SYMBOL_MISSING)
    {
#ifdef _WIN32
        DWORD dwErr = GetLastError();
        DWORD dwChars = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dwErr,
            0,
            messageOut + copyErrLen,
            (DWORD)(messageOutLen - copyErrLen),
            NULL);

        return copyErrLen + dwChars;
#else
        const char* msg = dlerror();
        if (msg)
        {
            const size_t copyMsgLen =
                MinSizeT(strlen(msg), messageOutLen - copyErrLen - 1);
            const size_t finalLen = copyErrLen + copyMsgLen;

            memcpy(messageOut + copyErrLen, msg, copyMsgLen);
            messageOut[finalLen] = '\0';

            return finalLen;
        }
#endif
    }

    return copyErrLen;
}
