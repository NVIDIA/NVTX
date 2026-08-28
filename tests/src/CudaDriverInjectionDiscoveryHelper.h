/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef CUDA_DRIVER_INJECTION_DISCOVERY_HELPER_H
#define CUDA_DRIVER_INJECTION_DISCOVERY_HELPER_H

#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(_WIN32)
/* Register the stub directory so the loader's LOAD_LIBRARY_SEARCH_USER_DIRS load
 * finds the stub nvcuda.dll.  Must run before the first NVTX call. */
static void nvtxCudaDriverAddDllDirectory(void)
{
    wchar_t dir[MAX_PATH];
    DWORD length = GetEnvironmentVariableW(L"NVTX_CUDA_DRIVER_STUB_DIR", dir, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        AddDllDirectory(dir);
    }
}
#endif

static int nvtxCudaDriverLoaded(void)
{
#if defined(_WIN32)
    return GetModuleHandleW(L"nvcuda.dll") != NULL;
#else
    void* handle = dlopen("libcuda.so.1", RTLD_LAZY | RTLD_NOLOAD);
    if (handle)
    {
        dlclose(handle);
        return 1;
    }
    return 0;
#endif
}

static int nvtxCudaDriverCheckExpectation(int argc, const char** argv)
{
    int expectLoaded;
    int loaded;

    if (argc < 1)
    {
        return 2;
    }
    if (strcmp(argv[0], "loaded") == 0)
    {
        expectLoaded = 1;
    }
    else if (strcmp(argv[0], "unloaded") == 0)
    {
        expectLoaded = 0;
    }
    else
    {
        return 3;
    }

    loaded = nvtxCudaDriverLoaded();
    if (expectLoaded && !loaded)
    {
        return 10;
    }
    if (!expectLoaded && loaded)
    {
        return 11;
    }
    return 0;
}

#endif /* CUDA_DRIVER_INJECTION_DISCOVERY_HELPER_H */
