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

#include <nvtx3/nvtx3.hpp>

#include <string>

extern void dummy();
void dummy()
{
    {
        std::string unit{"bytes"};
        nvtx3::counter_semantic sem;
        sem.unit(unit);
    }
    {
        std::string display_name{"CorrelationDomain"};
        nvtx3::correlation_semantic sem;
        sem.display_name(display_name);
    }

#ifdef COUNTER_SEMANTIC_UNIT_FROM_TEMPORARY_STRING
    {
        nvtx3::counter_semantic sem;
        sem.unit(std::string{"bytes"});
    }
#endif

#ifdef CORRELATION_SEMANTIC_DISPLAY_NAME_FROM_TEMPORARY_STRING
    {
        nvtx3::correlation_semantic sem;
        sem.display_name(std::string{"CorrelationDomain"});
    }
#endif
}
