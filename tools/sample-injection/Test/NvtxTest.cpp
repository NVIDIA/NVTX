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

#include <chrono>
#include <thread>

#include <nvtx3/nvToolsExt.h>


int main()
{
    nvtxRangePush("Test push/pop range");
    nvtxMark("Test mark");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    nvtxRangePop();

    nvtxDomainHandle_t domain = nvtxDomainCreateA("Domain #1");
    nvtxDomainMarkEx(domain, nullptr);

    nvtxDomainDestroy(domain);

    return 0;
}
