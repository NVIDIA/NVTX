/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#define NVTX_NO_IMPL
#include "nvtx3/nvToolsExt.h"

#ifdef ENABLE_CUDA
#include "nvtx3/nvToolsExtCuda.h"
#endif

#ifdef ENABLE_CUDART
#include "nvtx3/nvToolsExtCudaRt.h"
#endif

#ifdef ENABLE_OPENCL
#include "nvtx3/nvToolsExtOpenCL.h"
#endif

#ifdef ENABLE_PAYLOAD
#include "nvtx3/nvToolsExtPayload.h"
#include "nvtx3/nvToolsExtSemanticsCorrelation.h"
#include "nvtx3/nvToolsExtSemanticsCounters.h"
#include "nvtx3/nvToolsExtSemanticsScope.h"
#include "nvtx3/nvToolsExtSemanticsTime.h"
#endif

#ifdef ENABLE_COUNTERS
#include "nvtx3/nvToolsExtCounters.h"
#endif

#ifdef ENABLE_MEMORY
#include "nvtx3/nvToolsExtMem.h"
#endif

#ifdef ENABLE_MEMORY_CUDART
#include "nvtx3/nvToolsExtMemCudaRt.h"
#endif

#include "nvtx3/nvToolsExtSync.h"
