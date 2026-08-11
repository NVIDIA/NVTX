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
 * Backend library wrapper around the recording mock NVTXW backend.  It exports
 * the symbols the NVTXW loader (nvtxw3_loader.c) looks for, plus accessors that let the
 * loader tests inspect what the backend recorded once it has been dlopen-ed.
 */

#include <nvtx3/nvToolsExt.h> /* For NVTX_DYNAMIC_EXPORT / NVTX_EXPORT_UNMANGLED_FUNCTION_NAME */

#include "MockNvtxwBackend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Prototypes for the exported entry points (satisfies -Wmissing-prototypes). */
NVTX_DYNAMIC_EXPORT
nvtxwResultCode_t nvtxwGetInterface(
    nvtxwInterfaceVersion_t version,
    const void** ifaceOut);
NVTX_DYNAMIC_EXPORT
void nvtxwFinalize(void);
NVTX_DYNAMIC_EXPORT
MockNvtxwRecorder* MockNvtxwBackendGetRecorder(void);
NVTX_DYNAMIC_EXPORT
void MockNvtxwBackendReset(void);

NVTX_DYNAMIC_EXPORT
nvtxwResultCode_t nvtxwGetInterface(
    nvtxwInterfaceVersion_t version,
    const void** ifaceOut)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME
    return MockNvtxwGetInterface(version, ifaceOut);
}

NVTX_DYNAMIC_EXPORT
void nvtxwFinalize(void)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME
    /* Nothing to free; recorder is a static singleton. */
}

/* Exported accessor so a loader test can fetch the recorder via the module
 * handle returned by nvtxwLoad. */
NVTX_DYNAMIC_EXPORT
MockNvtxwRecorder* MockNvtxwBackendGetRecorder(void)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME
    return MockNvtxwGetRecorder();
}

NVTX_DYNAMIC_EXPORT
void MockNvtxwBackendReset(void)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME
    MockNvtxwReset();
}

#ifdef __cplusplus
}
#endif
