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

/* Dynamic libraries should be loaded with absolute paths to avoid
 * problems not finding things in the search paths.  Construct the
 * absolute path to a dynamic library in the same directory as the
 * process's executable, or some subdirectory of it, using these
 * utility functions.  C++17's std::filesystem makes this much
 * easier, but these utilities should work in C++11.
 */

#if defined(_WIN32)

#include <windows.h>

#else

#if defined(__CYGWIN__)
#if defined(__POSIX_VISIBLE)
#if __POSIX_VISIBLE < 200112L
#error On Cygwin, you must `#define _POSIX_C_SOURCE 200112L` or greater before including any headers so that readlink() is available. You can achieve this by including this header before any others.
#endif
#endif
#if defined(_POSIX_C_SOURCE)
#undef _POSIX_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#endif

#include <unistd.h>

#endif

#if defined(__APPLE__)
#include <libproc.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "DllHelper.h"

#if defined(_WIN32)
constexpr char pathSep = '\\';
#else
constexpr char pathSep = '/';
#endif

// Adapted from C functions in NVTXW implementation
static std::string GetCurrentProcessPath(void);
static std::string GetCurrentProcessPath(void)
{
    char* buf;
#if defined(_WIN32)
    {
        DWORD size = MAX_PATH;
        DWORD newSize;
        buf = nullptr;
        while (1)
        {
            buf = static_cast<char*>(realloc(buf, size));
            if (!buf)
            {
                return nullptr;
            }
            newSize = GetModuleFileNameA(nullptr, buf, size);
            if (newSize < size)
            {
                break;
            }
            size *= 2;
        }
    }
#elif defined(__APPLE__)
    {
        int ret;
        pid_t pid = getpid();
        buf = static_cast<char*>(malloc(PROC_PIDPATHINFO_MAXSIZE));
        if (!buf)
        {
            return nullptr;
        }
        ret = proc_pidpath(pid, buf, PROC_PIDPATHINFO_MAXSIZE);
        if (ret == 0)
        {
            free(buf);
            return nullptr;
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
        buf = static_cast<char*>(malloc(size));
        if (!buf)
        {
            return nullptr;
        }
        _cmdname(buf);
    }
#else
    {
        size_t size = 1024;
        ssize_t bytesReadSigned;
        size_t bytesRead;
        static const char linkName[] = "/proc/self/exe";
        buf = nullptr;
        while (1)
        {
            buf = static_cast<char*>(realloc(buf, size));
            if (!buf)
            {
                return nullptr;
            }
            bytesReadSigned = readlink(linkName, buf, size);
            if (bytesReadSigned < 0)
            {
                free(buf);
                return nullptr;
            }
            bytesRead = static_cast<size_t>(bytesReadSigned);
            if (bytesRead < size) break;
            size *= 2;
        }
        buf[bytesRead] = '\0';
    }
#endif

    std::string result;
    if (buf)
    {
        result = buf;
        free(buf);
    }
    return result;
}

/*
 * We know the absolute path must have at least one slash in it,
 * right before the exe filename.  So we can truncate the string
 * to end just after the last slash, and append other file or
 * directory names.  Examples:
 *    C:\path\to\foo.exe -> C:\path\to\
 *    C:\foo.exe -> C:\
 *    /path/to/foo -> /path/to/
 *    /foo -> /
 */
static std::string GetCurrentProcessDirWithSep(void);
static std::string GetCurrentProcessDirWithSep(void)
{
    std::string exeAbsPath = GetCurrentProcessPath();
    exeAbsPath.resize(exeAbsPath.find_last_of(pathSep) + 1);
    return exeAbsPath;
}

/*
 * Take the absolute path to the current process's executable,
 * remove the executable's name, and then append the library
 * filename.  Applies the standard dynamic library prefix and
 * suffix to the library's base name, but the suffix may be
 * overridden if it isn't the standard one (e.g. ".so.1.1").
 * If subDirs has any entries, they are added between the
 * directory and the library name, with path separators added
 * between each.  Examples:
 *   (Assuming process is C:\path\to\foo.exe on Windows)
 *     AbsolutePathToLibraryInCurrentProcessPath("example")
 *       -> C:\path\to\example.dll
 *     AbsolutePathToLibraryInCurrentProcessPath("example", {"nested", "deeper"})
 *       -> C:\path\to\nested\deeper\example.dll
 *   (Assuming process is /path/to/foo on Linux)
 *     AbsolutePathToLibraryInCurrentProcessPath("example")
 *       -> /path/to/libexample.so
 *     AbsolutePathToLibraryInCurrentProcessPath("example", {"nested", "deeper"}, ".so.1")
 *       -> /path/to/nested/deeper/libexample.so.1
 */
static std::string AbsolutePathToLibraryInCurrentProcessPath(
    std::string libraryBaseName,
    std::vector<std::string> subDirs = {},
    std::string libSuffix = DLL_SUFFIX);
static std::string AbsolutePathToLibraryInCurrentProcessPath(
    std::string libraryBaseName,
    std::vector<std::string> subDirs,
    std::string libSuffix)
{
    std::string result = GetCurrentProcessDirWithSep();

    for (auto const& subDir : subDirs)
    {
        result += subDir;
        result += pathSep;
    }

    result += DLL_PREFIX;
    result += libraryBaseName;
    result += libSuffix;

    return result;
}
