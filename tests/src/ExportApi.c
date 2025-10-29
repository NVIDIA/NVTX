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

#define NVTX_EXPORT_API

#include <nvtx3/nvToolsExt.h>
#include <nvtx3/nvToolsExtCuda.h>
#include <nvtx3/nvToolsExtCudaRt.h>
#include <nvtx3/nvToolsExtOpenCL.h>
#include <nvtx3/nvToolsExtSync.h>

#ifdef SUPPORT_EXTENSIONS
#include <nvtx3/nvToolsExtMem.h>
#include <nvtx3/nvToolsExtMemCudaRt.h>
#include <nvtx3/nvToolsExtPayload.h>
#include <nvtx3/nvToolsExtPayloadHelper.h>
#include <nvtx3/nvToolsExtCounters.h>
#include <nvtx3/nvToolsExtSemanticsCounters.h>
#include <nvtx3/nvToolsExtSemanticsScope.h>
#include <nvtx3/nvToolsExtSemanticsTime.h>
#endif
