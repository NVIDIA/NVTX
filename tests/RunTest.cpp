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

#include "PathHelper.h"
#include "DllHelper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

static bool SetEnvVar(const char* name, const char* value)
{
#if defined(_WIN32)
    auto result = _putenv_s(name, value);
#else
    auto result = setenv(name, value, 1);
#endif
    return result == 0;
}

static int MainInternal(int argc, const char** argv)
{
    const std::string testArg("-t");
    const std::string injectionArg("-i");
    std::string testName;
    std::string injectionName;

    auto oldArgv = argv;
    ++argv;
    while (*argv)
    {
        if      (*argv == testArg     ) { ++argv; if (*argv) testName      = *argv; else return 100; }
        else if (*argv == injectionArg) { ++argv; if (*argv) injectionName = *argv; else return 101; }
        else break;
        ++argv;
    }
    argc -= static_cast<int>(argv - oldArgv);

    if (testName.empty())
    {
        return 103;
    }

    printf("RunTest:\n");

    std::string test = AbsolutePathToLibraryInCurrentProcessPath(testName);
    printf("  - Using test:      %s\n", test.c_str());

    std::string injection;
    if (!injectionName.empty())
    {
        const char* injectionVar = (sizeof(void*) == 8)
            ? "NVTX_INJECTION64_PATH"
            : "NVTX_INJECTION32_PATH";

        // Passing - for the injection means to use the test library as its own injection
        injection = (injectionName == "-")
            ? test
            : AbsolutePathToLibraryInCurrentProcessPath(injectionName);

        bool success = SetEnvVar(injectionVar, injection.c_str());
        if (!success) return 102;
    }

    printf("  - Using injection: %s\n", injection.empty() ? "<none>" : injection.c_str());

    DLL_HANDLE hDll = DLL_OPEN(test.c_str());
    if (!hDll) return 104;

    using pfnRunTest_t = int(*)(int, const char**);

    auto pfnRunTest = reinterpret_cast<pfnRunTest_t>(GET_DLL_FUNC(hDll, "RunTest"));
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
            default:
                printf("RunTest FAILED with return code: %d\n", result);
        }
    }

    return result;
}
