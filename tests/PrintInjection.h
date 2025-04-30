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

#include "PrintInjectionImpl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* For dynamic (env-var based) and preinject (LD_PRELOAD based) support, provide
*  dynamic exports with the appropriate names.  These should be implemented as
*  tail-calls since the ABI exactly matches the internal implementation function,
*  but performance is really not a concern since these functions are called once
*  per client initialization. */

#ifdef SUPPORT_DYNAMIC_INJECTION
NVTX_DYNAMIC_EXPORT
extern int NVTX_API InitializeInjectionNvtx2(NvtxGetExportTableFunc_t getExportTable);
NVTX_DYNAMIC_EXPORT
int NVTX_API InitializeInjectionNvtx2(NvtxGetExportTableFunc_t getExportTable)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME
    return InitializeInjectionNvtx2Internal(getExportTable);
}
#endif

#ifdef SUPPORT_PREINJECTION
/* Note: this mode is not supported by the NVTX loader on Windows */
NVTX_DYNAMIC_EXPORT
extern int NVTX_API InitializeInjectionNvtx2Preinject(NvtxGetExportTableFunc_t getExportTable);
NVTX_DYNAMIC_EXPORT
int NVTX_API InitializeInjectionNvtx2Preinject(NvtxGetExportTableFunc_t getExportTable)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME
    return InitializeInjectionNvtx2Internal(getExportTable);
}
#endif

#ifdef __cplusplus
}
#endif

#ifdef SUPPORT_STATIC_INJECTION
/* Redefine the symbol without using attribute weak. */
/* Note: this mode is not supported by the NVTX loader on Windows */
extern NvtxInitializeInjectionNvtxFunc_t InitializeInjectionNvtx2_fnptr;
NvtxInitializeInjectionNvtxFunc_t InitializeInjectionNvtx2_fnptr = InitializeInjectionNvtx2Internal;
#endif
