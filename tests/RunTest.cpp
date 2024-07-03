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

#include "DllHelper.h"
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <libproc.h>
#endif

#if defined(_WIN32)
constexpr char pathsep = '\\';
#else
constexpr char pathsep = '/';
#endif

bool SetEnvVar(const char* name, const char* value)
{
#if defined(_WIN32)
    auto result = _putenv_s(name, value);
#else
    auto result = setenv(name, value, 1);
#endif
    return result == 0;
}

// Adapted from C function in NVTXW implementation
std::string GetCurrentProcessPath()
{
    char* buf;
#if defined(_WIN32)
    {
        DWORD size = MAX_PATH;
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
        int ret;
        pid_t pid = getpid();
        buf = (char*)malloc(PROC_PIDPATHINFO_MAXSIZE);
        ret = proc_pidpath(pid, buf, PROC_PIDPATHINFO_MAXSIZE);
        if (ret == 0)
        {
            free(buf);
            buf = NULL;
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
        size_t size = 1024;
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

    std::string result;
    if (buf) result = buf;
    free(buf);
    return result;
}

int MainInternal(int argc, const char** argv)
{
    const std::string testArg("-t");
    const std::string injectionArg("-i");
    std::string test;
    std::string injection;

    auto oldArgv = argv;
    ++argv;
    while (*argv)
    {
        if      (*argv == testArg     ) { ++argv; if (*argv) test      = *argv; else return 100; }
        else if (*argv == injectionArg) { ++argv; if (*argv) injection = *argv; else return 101; }
        else break;
        ++argv;
    }
    argc -= (int)(argv - oldArgv);

    printf("RunTest:\n");

    auto runTestDir = GetCurrentProcessPath();
    runTestDir.resize(runTestDir.find_last_of(pathsep));
    runTestDir += pathsep;

    if (test.empty())
    {
        return 103;
    }
    else
    {
        test = runTestDir + DLL_PREFIX + test + DLL_SUFFIX;
    }

    printf("  - Using test:      %s\n", test.c_str());

    if (!injection.empty())
    {
        const char* injectionVar = (sizeof(void*) == 8)
            ? "NVTX_INJECTION64_PATH"
            : "NVTX_INJECTION32_PATH";

        if (injection == "-")
        {
            injection = test;
        }
        else
        {
            injection = runTestDir + DLL_PREFIX + injection + DLL_SUFFIX;
        }
        bool success = SetEnvVar(injectionVar, injection.c_str());
        if (!success) return 102;
    }

    printf("  - Using injection: %s\n", injection.empty() ? "<none>" : injection.c_str());

    auto hDll = LOAD_DLL(test.c_str());
    if (!hDll) return 104;

    using pfnRunTest_t = int(*)(int, const char**);

    auto pfnRunTest = (pfnRunTest_t)GET_DLL_FUNC(hDll, "RunTest");
    if (!pfnRunTest) return 105;

    int result = pfnRunTest(argc, argv); // Forward remaining args
    if (result) return result;

    return 0;
}

int main(int argc, const char** argv)
{
    int result = MainInternal(argc, argv);
    if (result == 0)
    {
        printf("RunTest PASSED\n");
    }
    else
    {
        // For error codes known to this test driver, print useful error descriptions.
        // Otherwise, rely on test to print information about errors.
        switch (result)
        {
            case 100:
                puts("RunTest: -t requires an argument, the base name of the library to use as a test");
                break;
            case 101:
                puts("RunTest: -i requires an argument, the base name of the library to use as an injection");
                break;
            case 102:
                puts("RunTest: Failed to set NVTX injection environment variable");
                break;
            case 103:
                puts("RunTest: Missing required argument: -t <base name of library to use as a test>");
                break;
            case 104:
                puts("RunTest: Test library failed to load");
#ifndef _WIN32
                printf("    dlerror: %s\n", dlerror());
#endif
                break;
            case 105:
                puts("RunTest: Test library loaded, but does not export required entry point RunTest");
                break;
        }

        printf("RunTest FAILED with return code: %d\n", result);
    }

    return result;
}