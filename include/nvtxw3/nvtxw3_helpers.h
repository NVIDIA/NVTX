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

#if !defined(NVTXW_HELPERS_API)
#define NVTXW_HELPERS_API

/**
 * \file nvtxw3_helpers.h
 * \brief Umbrella header aggregating the NVTX Writer helper APIs.
 *
 * These stateless helpers provide a simpler API for common producer use cases.
 * They can also serve as compact templates for applications that need to
 * tailor the underlying NVTXW calls.
 */

#include <nvtxw3/nvtxw3_counter_helpers.h>
#include <nvtxw3/nvtxw3_event_helpers.h>
#include <nvtxw3/nvtxw3_setup_helpers.h>

#endif
