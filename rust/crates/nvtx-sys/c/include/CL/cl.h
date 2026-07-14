/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef NVTX_RUST_CL_H
#define NVTX_RUST_CL_H

/* Minimal OpenCL handle declarations needed by nvToolsExtOpenCL.h. */
typedef struct _cl_device_id *cl_device_id;
typedef struct _cl_context *cl_context;
typedef struct _cl_command_queue *cl_command_queue;
typedef struct _cl_mem *cl_mem;
typedef struct _cl_sampler *cl_sampler;
typedef struct _cl_program *cl_program;
typedef struct _cl_event *cl_event;

#endif
