/*
 * SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/* This header defines types which are used by the internal implementation
*  of NVTX and callback subscribers.  API clients do not use these types,
*  so they are defined here instead of in nvToolsExt.h to clarify they are
*  not part of the NVTX client API. */

#ifndef NVTXEXTTYPES_H
#define NVTXEXTTYPES_H

#ifndef NVTX_EXT_TYPES_GUARD
#error Never include this file directly -- it is automatically included by nvToolsExt[EXTENSION].h.
#endif

typedef intptr_t (NVTX_API * NvtxExtGetExportFunction_t)(uint32_t exportFunctionId);

typedef struct nvtxExtModuleSegment_t
{
    size_t segmentId;
    size_t slotCount;
    intptr_t* functionSlots;
} nvtxExtModuleSegment_t;

typedef struct nvtxExtModuleInfo_t
{
    uint16_t nvtxVer;
    uint16_t structSize;
    uint16_t moduleId;
    uint16_t compatId;
    size_t segmentsCount;
    nvtxExtModuleSegment_t* segments;
    NvtxExtGetExportFunction_t getExportFunction;
    const void* extInfo;
} nvtxExtModuleInfo_t;

typedef int (NVTX_API * NvtxExtInitializeInjectionFunc_t)(nvtxExtModuleInfo_t* moduleInfo);

#endif /* NVTXEXTTYPES_H */