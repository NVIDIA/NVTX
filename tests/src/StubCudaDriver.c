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

/*
 * A stub of the CUDA driver, built as libcuda.so.1 / nvcuda.dll so the NVTX
 * loader finds it by the name the real driver uses.  It exercises the loader's
 * CUDA driver injection discovery step without a real driver.  Build-time defines
 * select its behavior.
 *   - Default provides an init function that succeeds
 *   - STUB_CUDA_INIT_FAILS provides an init function that fails
 *   - STUB_CUDA_GETTER_ERROR makes cuNvtxGetInjectionInitializer return a non-zero status
 *   - STUB_CUDA_OMIT_GETTER does not export cuNvtxGetInjectionInitializer
 *   - STUB_CUDA_NVTX_EXTENSION provides an init function for the NVTX extension kind
 */

#define NVTX_NO_IMPL
#include "nvtx3/nvToolsExt.h"
#if defined(STUB_CUDA_NVTX_EXTENSION)
#include "nvtx3/nvToolsExtPayload.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(STUB_CUDA_OMIT_GETTER)

#if !defined(STUB_CUDA_GETTER_ERROR)
#if !defined(STUB_CUDA_NVTX_EXTENSION)
static int NVTX_API StubInit(NvtxGetExportTableFunc_t getExportTable)
{
    (void)getExportTable;
#if defined(STUB_CUDA_INIT_FAILS)
    return 0;
#else
    return 1;
#endif
}
#else
static int NVTX_API StubExtInit(nvtxExtModuleInfo_t* moduleInfo)
{
    (void)moduleInfo;
#if defined(STUB_CUDA_INIT_FAILS)
    return 0;
#else
    return 1;
#endif
}
#endif /* STUB_CUDA_NVTX_EXTENSION */
#endif /* !STUB_CUDA_GETTER_ERROR */

NVTX_DYNAMIC_EXPORT
int NVTX_API cuNvtxGetInjectionInitializer(
    NvtxInjectionInitializerKind kind,
    NvtxInjectionGenericFunc_t* initializer);
NVTX_DYNAMIC_EXPORT
int NVTX_API cuNvtxGetInjectionInitializer(
    NvtxInjectionInitializerKind kind,
    NvtxInjectionGenericFunc_t* initializer)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME
#if defined(STUB_CUDA_GETTER_ERROR)
    (void)kind;
    (void)initializer;
    return 1;
#else
#if !defined(STUB_CUDA_NVTX_EXTENSION)
    if (kind == NVTX_INJECTION_INITIALIZER_CORE)
    {
        *initializer = (NvtxInjectionGenericFunc_t)StubInit;
    }
#else
    if (kind == NVTX_INJECTION_INITIALIZER_EXTENSION)
    {
        *initializer = (NvtxInjectionGenericFunc_t)StubExtInit;
    }
#endif
    else
    {
        *initializer = NVTX_NULLPTR;
    }
    return 0;
#endif /* STUB_CUDA_GETTER_ERROR */
}

#else /* STUB_CUDA_OMIT_GETTER */

/* Placeholder export so this variant is still a well-formed shared library that
 * simply lacks cuNvtxGetInjectionInitializer. */
NVTX_DYNAMIC_EXPORT
int NVTX_API stubCudaDriverPlaceholder(void);
NVTX_DYNAMIC_EXPORT
int NVTX_API stubCudaDriverPlaceholder(void)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME
    return 1;
}

#endif /* STUB_CUDA_OMIT_GETTER */

#ifdef __cplusplus
}
#endif
