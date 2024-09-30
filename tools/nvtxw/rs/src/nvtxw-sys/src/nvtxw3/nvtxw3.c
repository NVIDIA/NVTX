/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <nvtxw3/nvtxw3.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#if defined (_QNX_SOURCE)
#include <signal.h>
#include <errno.h>
#else
#include <sys/signal.h>
#endif
#include <sys/wait.h>
#include <sys/stat.h>
#endif

#if defined(__APPLE__)
#include <libproc.h>
#endif

/*-------------------------------------------------------------*/
/* Path string helpers -- implement here to avoid dependencies */

#if defined(_WIN32)
static const char pathSep = '\\';
#if defined(NVTXW3_TEST_PATH_UTILITIES)
static const char pathDelimiter = ';';
#endif
static const size_t initialPathBufSize = MAX_PATH; /* Grows if not big enough */
#define NVTXW3_DLLHANDLE  HMODULE
#define NVTXW3_DLLOPEN(x) LoadLibraryA(x)
#define NVTXW3_DLLFUNC    GetProcAddress
#define NVTXW3_DLLCLOSE   FreeLibrary
#else
static const char pathSep = '/';
#if defined(NVTXW3_TEST_PATH_UTILITIES)
static const char pathDelimiter = ':';
#endif
static const size_t initialPathBufSize = 260; /* Grows if not big enough */
#define NVTXW3_DLLHANDLE  void*
#define NVTXW3_DLLOPEN(x) dlopen(x, RTLD_LAZY)
#define NVTXW3_DLLFUNC    dlsym
#define NVTXW3_DLLCLOSE   dlclose
#endif

#if defined(NVTXW3_TEST_PATH_UTILITIES)
/* If native path separator is not forward slash (e.g. backslash on Windows),
*  do in-place conversion of forward slashes to native path separator. */
static void ForwardSlashesToNative(char* path)
{
#if _WIN32
    char* cur;
    if (!path) return;
    for (cur = path; *cur; ++cur)
    {
        if (*cur == '/') *cur = pathSep;
    }
#else
    (void)path;
#endif
}
#endif

/* Take pointers to string buffer begin/end.  End must equal begin + strlen(begin),
*  or NULL, in which case it will be set to begin + strlen(begin).
*  Remove trailing slashes in-place by overwriting first trailing slash with null. */
static void StripTrailingSlashes(char* path)
{
    char* newPathEnd;
    char* pathEnd = path + strlen(path);

    newPathEnd = pathEnd;
    while (newPathEnd != path)
    {
        char* cur = newPathEnd - 1;
        if (*cur != pathSep) break;
        newPathEnd = cur;
    }
    if (newPathEnd != pathEnd)
    {
        *newPathEnd = '\0';
    }
}

/* Take pointers to string buffer begin/end.  End must equal begin + strlen(begin),
*  or NULL, in which case it will be set to begin + strlen(begin).
*  Remove leading slashes in-place by memmove-ing from first character after leading
*  slashes to beginning of buffer, including null terminator. */
#if defined(NVTXW3_TEST_PATH_UTILITIES)
static char* AfterLeadingSlashes(char* cur)
{
    for (; *cur && *cur == pathSep; ++cur);
    return cur;
}
#endif
static const char* AfterLeadingSlashesConst(const char* cur)
{
    for (; *cur && *cur == pathSep; ++cur);
    return cur;
}

#if defined(NVTXW3_TEST_PATH_UTILITIES)
/* Take pointers to string buffer begin/end.  End must equal begin + strlen(begin),
*  or NULL, in which case it will be set to begin + strlen(begin).
*  Remove leading slashes in-place by memmove-ing from first character after leading
*  slashes to beginning of buffer, including null terminator. */
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

/* Take pointers to string buffer begin/end.  End must equal begin + strlen(begin),
*  or NULL, in which case it will be set to begin + strlen(begin).
*  Returns pointer to heap-allocated copy of input, must be freed with free(). */
static char* AssignHeapString(char* lhs, const char* rhs)
{
    size_t lenWithNull;

    if (!rhs) return NULL;

    lenWithNull = strlen(rhs) + 1;
    lhs = (char*)realloc(lhs, lenWithNull);
    memcpy(lhs, rhs, lenWithNull);
    return lhs;
}

static char* AssignHeapStringFromRange(char* lhs, const char* rhsBegin, const char* rhsEnd)
{
    size_t lenWithoutNull;

    if (!rhsBegin || !rhsEnd) return NULL;

    lenWithoutNull = rhsEnd - rhsBegin;
    lhs = (char*)realloc(lhs, lenWithoutNull + 1);
    memcpy(lhs, rhsBegin, lenWithoutNull);
    lhs[lenWithoutNull] = '\0';
    return lhs;
}

/* Take pointers to string buffer begin/end.  End must equal begin + strlen(begin),
*  or NULL, in which case it will be set to begin + strlen(begin).
*  Returns pointer to heap-allocated copy of input, must be freed with free(). */
static char* MakeHeapString(const char* str)
{
    return AssignHeapString(NULL, str);
}

static char* MakeHeapStringFromRange(const char* strBegin, const char* strEnd)
{
    return AssignHeapStringFromRange(NULL, strBegin, strEnd);
}

#if defined(NVTXW3_TEST_PATH_UTILITIES)
static char* MakeHeapStringWithNativeSlashes(const char* str)
{
    char* buf = AssignHeapString(NULL, str);
    ForwardSlashesToNative(buf);
    return buf;
}

/* Take pointer to a HeapString (lhs) and any C string (rhs), append rhs to lhs,
*  reallocating the heap memory for lhs if necessary.  Returns pointer to result
*  HeapString, which may or may not be the same pointer passed in as lhs.
*  HeapString must be freed with free(). */
static char* AppendToHeapString(char* lhs, const char* rhs)
{
    size_t lenLhs, lenRhs;
    lenLhs = strlen(lhs);
    lenRhs = strlen(rhs);
    if (lenRhs == 0) return lhs;
    lhs = (char*)realloc(lhs, lenLhs + lenRhs + 1);
    memcpy(lhs + lenLhs, rhs, lenRhs + 1);
    return lhs;
}
#endif

/* Take pointer to a HeapString (lhs) and any C string (rhs), append rhs to lhs,
*  with a path separator between them, reallocating the heap memory for lhs if
*  necessary.  If rhs is null or empty, then the result is lhs unmodified.  If
*  lhs is null or empty and rhs is not, then the result is a path separator
*  followed by rhs.  Returns pointer to result HeapString, which may or may not
*  be the same pointer passed in as lhs.  HeapString must be freed with free(). */
static char* AppendToHeapStringWithSep(char* lhs, const char* rhs)
{
    size_t lenLhs, lenRhs;
    lenLhs = strlen(lhs);
    lenRhs = strlen(rhs);
    if (lenRhs == 0) return lhs;
    lhs = (char*)realloc(lhs, lenLhs + lenRhs + 2);
    lhs[lenLhs] = pathSep;
    memcpy(lhs + lenLhs + 1, rhs, lenRhs + 1);
    return lhs;
}

/* dir is a HeapString.  If dir is empty or just slashes, result will be a
*  path relative to the root, i.e. beginning with a path separator.
*  relativePath must be a valid relative path (not empty, not just slashes).
*  Returns pointer to result HeapString, which may or may not be the same
*  pointer passed in as lhs.  HeapString must be freed with free(). */
static char* AppendToPathHeapString(char* dir, const char* relativePath)
{
    const char* relPathAfterLeadingSlashes;
    relPathAfterLeadingSlashes = AfterLeadingSlashesConst(relativePath);
    StripTrailingSlashes(dir);
    return AppendToHeapStringWithSep(dir, relPathAfterLeadingSlashes);
}

static char* LoadFileIntoHeapString(const char* filename)
{
    FILE* f;
    char* buf;
    int err;
    long pos;
    size_t size;
    size_t bytesRead;

    f = fopen(filename, "rb");
    if (!f) return NULL;
    err = fseek(f, 0, SEEK_END);
    if (err) { fclose(f); return NULL; }
    pos = ftell(f);
    if (pos < 0) { fclose(f); return NULL; }
    rewind(f);
    size = (size_t)pos;

    buf = (char*)malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    bytesRead = fread(buf, 1, size, f);
    if (bytesRead < size) { fclose(f); free(buf); return NULL; }

    buf[size] = '\0';
    fclose(f);
    return buf;
}

#if defined(NVTXW3_TEST_PATH_UTILITIES)
static int HasSlashes(const char* cur)
{
    for (; *cur; ++cur)
    {
        if (*cur == pathSep) return 1;
    }
    return 0;
}

static int HasTrailingSlash(const char* str)
{
    size_t len = strlen(str);
    if (len == 0) return 0;
    return str[len-1] == pathSep;
}
#endif

static char* GetCurrentWorkingDir()
{
#if defined(_WIN32)
    DWORD size;
    char* buf;

    // Returns size including space for null terminator
    size = GetCurrentDirectoryA(0, NULL);
    buf = (char*)malloc(size);
    GetCurrentDirectoryA(size, buf);
    return buf;
#else
    size_t size = initialPathBufSize;
    char* buf;

    buf = (char*)malloc(size);
    while (!getcwd(buf, size))
    {
        size *= 2;
        buf = (char*)realloc(buf, size);
    }
    buf = (char*)realloc(buf, strlen(buf) + 1);
    return buf;
#endif
}

#if defined(NVTXW3_TEST_PATH_UTILITIES)
/* Take pointer to string buffer of possibly-relative path, and returns
*  equivalent absolute path.  Input path must not be empty.
*  Returns pointer to heap-allocated string, must be freed with free(). */
static char* AbsolutePath(const char* path)
{
#if defined(_WIN32)
    size_t size;
    char* buf;

    if (!path) return NULL;

    // Returns size including space for null terminator
    size = (size_t)GetFullPathNameA(path, 0, NULL, NULL);
    buf = (char*)malloc(size);
    GetFullPathNameA(path, size, buf, NULL);
    return buf;
#else
    if (!path) return NULL;

    return path[0] == pathSep
        ? MakeHeapString(path) // Absolute already
        : AppendToPathHeapString(GetCurrentWorkingDir(), path);
#endif
}
#endif

/* Take pointer to heap string of path, and modifies it in-place to be its
*  parent directory, i.e. the directory containing the input file/directory.
*  String is shortened, but not reallocated, permitting possibly faster
*  appending of different path later.  Returns the pointer passed in without
*  modifying it for convenient chaining of path functions.  If input path is
*  NULL, NULL is returned.  If input is an empty string, or root directory,
*  the heap string will be set to an empty string to indicate there is no
*  parent directory.  Returned pointer to heap-allocated string must be
*  freed with free(). */
static char* ToParentDir(char* path)
{
    char* cur;

    if (!path) return NULL;

    StripTrailingSlashes(path);

    for (cur = path + strlen(path); cur >= path; --cur)
    {
        if (*cur == pathSep)
        {
            /* Found the last slash */
            if (cur == path)
            {
                /* Special case -- last slash is first character
                *  in buffer.  Trailing slashes were trimmed first,
                *  so this can only occur when ParentDir should
                *  return the root directory.  This is the only
                *  case where we want to keep the slash we found,
                *  so write the null terminator after the slash. */
                *(cur + 1) = '\0';
            }
            else
            {
                /* Change slash to null, terminating the string
                *  before the last slash */
                *cur = '\0';
            }
            return path;
        };
    }

    /* No slashes found, so there's no parent directory.  Assign empty
    *  string by nulling first character, which is safe because all heap
    *  strings must be at least one byte long. */
    path[0] = '\0';
    return path;
}

#if defined(NVTXW3_TEST_PATH_UTILITIES)
/* Take pointer to string buffer of path, and returns the parent directory,
*  i.e. the directory containing the input file/directory.  If input path is
*  NULL, empty string, or root directory, NULL is returned to indicate there
*  is no parent directory, so return value must be NULL-checked.
*  Returns pointer to heap-allocated string, must be freed with free(). */
static char* ParentDir(const char* path)
{
    char* buf;

    if (!path) return NULL;

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
*  executable file.  Buffer allocated may be a little larger than the path
*  string it contains, and is not realloc'ed to fit since typical usage of
*  this function involves getting the parent directory and appending to it.
*  Returned pointer to heap-allocated string must be freed with free(). */
static char* GetCurrentProcessPath()
{
    char* buf;
#if defined(_WIN32)
    {
        DWORD size = initialPathBufSize;
        DWORD newSize;
        buf = NULL;
        while (1)
        {
            buf = (char*)realloc(buf, size);
            newSize = GetModuleFileNameA(NULL, buf, size);
            if (newSize < size) break;
            size *= 2;
        }
    }
#elif defined(__APPLE__)
    {
        size_t size = PROC_PIDPATHINFO_MAXSIZE;
        pid_t pid;
        buf = (char*)malloc(size);
        pid = getpid();
        size = proc_pidpath(pid, buf, size);
        if (size == 0)
        {
            buf[0] = '\0';
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
        _cmdname(buf);
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
            buf = (char*)realloc(buf, size);
            bytesReadSigned = readlink(linkName, buf, size);
            if (bytesReadSigned < 0) { free(buf); return NULL; }
            bytesRead = (size_t)bytesReadSigned;
            if (bytesRead < size) break;
            size *= 2;
        }
        buf[bytesRead] = '\0';
    }
#endif
    return buf;
}

static char* GetCurrentProcessDir()
{
    return ToParentDir(GetCurrentProcessPath());
}

static int KVPConsumerForSimplify(
    void* state,
    const char* readKeyBegin,
    const char* readKeyEnd,
    const char* readValBegin,
    const char* readValEnd)
{
    char* curWrite = *(char**)state;
    size_t size;
    /* Safe to cast away const here, since we are pointing at a non-const heap string */
    char* keyBegin = (char*)readKeyBegin;
    char* keyEnd   = (char*)readKeyEnd;
    char* valBegin = (char*)readValBegin;
    char* valEnd   = (char*)readValEnd;

    /* Rebuild the simplified config line at the write pointer, using memmove since the
    *  ranges may overlap or even be the exact same range. */
    size = keyEnd - keyBegin;
    memmove(curWrite, keyBegin, size);
    curWrite += size;

    *curWrite = '=';
    ++curWrite;

    size = valEnd - valBegin;
    memmove(curWrite, valBegin, size);
    curWrite += size;

    *curWrite = '\n';
    ++curWrite;

    *(char**)state = curWrite;

    return 0;
}

static char* SimplifyConfigHeapString(char* config)
{
    char* curWrite = config;

    nvtxwConsumeConfigString(config, KVPConsumerForSimplify, &curWrite);

    *curWrite = '\0';
    return (char*)realloc(config, strlen(config) + 1);
}

typedef struct GetInitModeState_t
{
    int modeFound;
    int modeStringFound;
    int mode;
    char* modeString;
} GetInitModeState_t;

static int KVPConsumerForGetInitMode(
    void* statePtr,
    const char* keyBegin,
    const char* keyEnd,
    const char* valBegin,
    const char* valEnd)
{
    GetInitModeState_t* state = (GetInitModeState_t*)statePtr;
    static const char keyMode[] = "InitMode";
    static const char keyModeString[] = "InitModeString";
    const size_t keyModeLen = strlen(keyMode);
    const size_t keyModeStringLen = strlen(keyModeString);
    size_t keyLen;

    keyLen = keyEnd - keyBegin;

    if (!state->modeFound
        && keyLen == keyModeLen
        && strncmp(keyBegin, keyMode, keyLen) == 0)
    {
        int mode;
        char* val;
        val = MakeHeapStringFromRange(valBegin, valEnd);
        mode = atoi(val);
        free(val);
        state->mode = mode;
        state->modeFound = 1;
    }

    if (!state->modeStringFound
        && keyLen == keyModeStringLen
        && strncmp(keyBegin, keyModeString, keyLen) == 0)
    {
        char* val;
        val = MakeHeapStringFromRange(valBegin, valEnd);
        state->modeString = val;
        state->modeStringFound = 1;
    }

    return state->modeFound &&
        (state->mode == NVTXW3_INIT_MODE_SEARCH_DEFAULT || state->modeStringFound);
}

/* Returns zero for success, and writes out params mode and modeString (the latter
*  is a HeapString).  If mode is not detected, or if the mode requires a modeString
*  and modeString is not detected, return non-zero error code. */
static int GetInitModeFromConfig(const char* config, int* mode, char** modeString)
{
    GetInitModeState_t state = {0};

    if (!mode || !modeString) return 1;
    *mode = 0;
    *modeString = NULL;

    nvtxwConsumeConfigString(config, KVPConsumerForGetInitMode, &state);

    /* Always an error if mode not found */
    if (!state.modeFound)
    {
        free(state.modeString);
        return 1;
    }

    /* Except in default mode, it's an error if modeString not found */
    if (state.mode != NVTXW3_INIT_MODE_SEARCH_DEFAULT && !state.modeStringFound)
    {
        return 2;
    }

    *mode = state.mode;
    *modeString = state.modeString;
    return 0;
}

/*-------------------------------------------------------------*/
/* Backend loader helpers */

static nvtxwResultCode_t InitLibraryFilename(
    const char* filename,                  /* required */
    const char* configString,              /* optional */
    nvtxwGetInterface_t* getInterfaceFunc, /* already null-checked */
    void** moduleHandle)                   /* optional */
{
    /* modeString is the filename of the library to load */
    NVTXW3_DLLHANDLE hModule;
    nvtxwLoadImplementation_t pfnLoadImplementation;
    nvtxwGetInterface_t tempGetInterfaceFunc = NULL;
    nvtxwResultCode_t result;
    char* configSimple = NULL;

    *getInterfaceFunc = NULL;
    if (moduleHandle) *moduleHandle = NULL;

    if (!filename)
    {
        return NVTXW3_RESULT_INVALID_ARGUMENT;
    }

    hModule = NVTXW3_DLLOPEN(filename);
    if (!hModule)
    {
        return NVTXW3_RESULT_LIBRARY_NOT_FOUND;
    }

    pfnLoadImplementation = (nvtxwLoadImplementation_t)NVTXW3_DLLFUNC(hModule, "nvtxwLoadImplementation");
    if (!pfnLoadImplementation)
    {
        NVTXW3_DLLCLOSE(hModule);
        return NVTXW3_RESULT_LOADER_SYMBOL_MISSING;
    }

    if (configString)
    {
        configSimple = SimplifyConfigHeapString(MakeHeapString(configString));
    }

    result = pfnLoadImplementation(configSimple, &tempGetInterfaceFunc);
    free(configSimple);
    if (result != NVTXW3_RESULT_SUCCESS || !tempGetInterfaceFunc)
    {
        NVTXW3_DLLCLOSE(hModule);
        return result;
    }

    /* Success - now write to output params */
    *getInterfaceFunc = tempGetInterfaceFunc;
    if (moduleHandle)
    {
        void* mod = (void*)hModule;
        *moduleHandle = mod;
    }

    return NVTXW3_RESULT_SUCCESS;
}

static nvtxwResultCode_t InitSearchDefault(
    const char* configString,              /* optional */
    nvtxwGetInterface_t* getInterfaceFunc, /* already null-checked */
    void** moduleHandle)                   /* optional */
{
    nvtxwResultCode_t result;
    char* filename;

    /* 1. Directory of current process's executable */
    filename = AppendToPathHeapString(GetCurrentProcessDir(), NVTXW3_LIB_FILENAME_DEFAULT);
    result = InitLibraryFilename(
        filename, configString, getInterfaceFunc, moduleHandle);
    free(filename);
    if (result == NVTXW3_RESULT_SUCCESS)
    {
        return NVTXW3_RESULT_SUCCESS;
    }

    /* 2. Standard search paths for dynamic libraries */
    result = InitLibraryFilename(
        NVTXW3_LIB_FILENAME_DEFAULT, configString, getInterfaceFunc, moduleHandle);
    if (result == NVTXW3_RESULT_SUCCESS)
    {
        return NVTXW3_RESULT_SUCCESS;
    }

    /* 3. Current working directory (may not be included in standard search paths) */
    filename = AppendToPathHeapString(GetCurrentWorkingDir(), NVTXW3_LIB_FILENAME_DEFAULT);
    result = InitLibraryFilename(
        filename, configString, getInterfaceFunc, moduleHandle);
    free(filename);

    /* No usable backend found */
    return NVTXW3_RESULT_LIBRARY_NOT_FOUND;
}

static nvtxwResultCode_t InitLibraryDirectory(
    const char* directory,                 /* required */
    const char* configString,              /* optional */
    nvtxwGetInterface_t* getInterfaceFunc, /* already null-checked */
    void** moduleHandle)                   /* optional */
{
    nvtxwResultCode_t result;
    char* filename;

    if (!directory) return NVTXW3_RESULT_INVALID_ARGUMENT;

    filename = AppendToPathHeapString(
        MakeHeapString(directory), NVTXW3_LIB_FILENAME_DEFAULT);

    result = InitLibraryFilename(filename, configString, getInterfaceFunc, moduleHandle);
    free(filename);

    return result;
}

static nvtxwResultCode_t InitConfigString(
    const char* config,
    nvtxwGetInterface_t* getInterfaceFunc,
    void** moduleHandle)
{
    nvtxwResultCode_t result;
    int err;
    int mode = 0;
    char* modeString = NULL;

    if (!config) return NVTXW3_RESULT_INVALID_ARGUMENT;

    err = GetInitModeFromConfig(config, &mode, &modeString);
    if (err)
    {
        free(modeString);
        return NVTXW3_RESULT_CONFIG_MISSING_LOADER_INFO;
    }

    switch (mode)
    {
        case NVTXW3_INIT_MODE_SEARCH_DEFAULT   : result = InitSearchDefault   (            config, getInterfaceFunc, moduleHandle); break;
        case NVTXW3_INIT_MODE_LIBRARY_FILENAME : result = InitLibraryFilename (modeString, config, getInterfaceFunc, moduleHandle); break;
        case NVTXW3_INIT_MODE_LIBRARY_DIRECTORY: result = InitLibraryDirectory(modeString, config, getInterfaceFunc, moduleHandle); break;
        default: result = NVTXW3_RESULT_UNSUPPORTED_LOADER_MODE;
    }

    free(modeString);
    return result;
}

static nvtxwResultCode_t InitConfigEnvVar(
    const char* configEnvVarName,
    nvtxwGetInterface_t* getInterfaceFunc,
    void** moduleHandle)
{
    const char* config;

    if (!configEnvVarName) return NVTXW3_RESULT_INVALID_ARGUMENT;

    config = getenv(configEnvVarName);
    if (!config) return NVTXW3_RESULT_ENV_VAR_NOT_FOUND;

    return InitConfigString(config, getInterfaceFunc, moduleHandle);
}

static nvtxwResultCode_t InitConfigFilename(
    const char* configFilename,
    nvtxwGetInterface_t* getInterfaceFunc,
    void** moduleHandle)
{
    nvtxwResultCode_t result;
    char* config;

    if (!configFilename) return NVTXW3_RESULT_INVALID_ARGUMENT;

    config = LoadFileIntoHeapString(configFilename);
    if (!config) return NVTXW3_RESULT_CONFIG_NOT_FOUND;

    result = InitConfigString(config, getInterfaceFunc, moduleHandle);
    free(config);
    return result;
}

static nvtxwResultCode_t InitConfigDirectory(
    const char* configDirectory,
    nvtxwGetInterface_t* getInterfaceFunc,
    void** moduleHandle)
{
    nvtxwResultCode_t result;
    char* configFilename;

    if (!configDirectory) return NVTXW3_RESULT_INVALID_ARGUMENT;

    configFilename = AppendToPathHeapString(
        MakeHeapString(configDirectory), NVTXW3_CONFIG_FILENAME_DEFAULT);

    result = InitConfigFilename(configFilename, getInterfaceFunc, moduleHandle);
    free(configFilename);
    return result;
}

/* #define NVTXW3_TEST_PATH_UTILITIES */
#if defined(NVTXW3_TEST_PATH_UTILITIES)
#include <test_path_utilities.h>
#endif

NVTXW3_DECLSPEC nvtxwResultCode_t nvtxwInitialize(
    nvtxwInitMode_t mode,
    const char* modeString,
    nvtxwGetInterface_t* getInterfaceFunc,
    void** moduleHandle)
{
#if defined(NVTXW3_TEST_PATH_UTILITIES)
    TestPathUtilities();
#endif

    if (!getInterfaceFunc)
    {
        return NVTXW3_RESULT_INVALID_ARGUMENT;
    }

    switch (mode)
    {
        case NVTXW3_INIT_MODE_SEARCH_DEFAULT   : return InitSearchDefault   (            NULL, getInterfaceFunc, moduleHandle);
        case NVTXW3_INIT_MODE_LIBRARY_FILENAME : return InitLibraryFilename (modeString, NULL, getInterfaceFunc, moduleHandle);
        case NVTXW3_INIT_MODE_LIBRARY_DIRECTORY: return InitLibraryDirectory(modeString, NULL, getInterfaceFunc, moduleHandle);
        case NVTXW3_INIT_MODE_CONFIG_FILENAME  : return InitConfigFilename  (modeString,       getInterfaceFunc, moduleHandle);
        case NVTXW3_INIT_MODE_CONFIG_DIRECTORY : return InitConfigDirectory (modeString,       getInterfaceFunc, moduleHandle);
        case NVTXW3_INIT_MODE_CONFIG_STRING    : return InitConfigString    (modeString,       getInterfaceFunc, moduleHandle);
        case NVTXW3_INIT_MODE_CONFIG_ENV_VAR   : return InitConfigEnvVar    (modeString,       getInterfaceFunc, moduleHandle);
    }

    return NVTXW3_RESULT_INVALID_INIT_MODE;
}

NVTXW3_DECLSPEC void nvtxwUnload(
    void* moduleHandle)
{
    nvtxwUnloadImplementation_t pfnUnload;
    NVTXW3_DLLHANDLE hModule = (NVTXW3_DLLHANDLE)moduleHandle;

    if (!hModule) return;

    pfnUnload = (nvtxwUnloadImplementation_t)NVTXW3_DLLFUNC(hModule, "nvtxwUnloadImplementation");
    if (pfnUnload)
    {
        pfnUnload();
    }

    NVTXW3_DLLCLOSE(hModule);
}
