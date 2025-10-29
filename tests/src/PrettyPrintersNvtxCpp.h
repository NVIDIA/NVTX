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

#pragma once
#include "PrettyPrintersNvtxC.h"
#include <nvtx3/nvtx3.hpp>

inline std::ostream& operator<<(std::ostream& os, nvtx3::event_attributes const& attr)
{
    return os << *attr.get();
}

inline std::ostream& operator<<(std::ostream& os, nvtx3::payload const& p)
{
    WritePayload(os, p.get_type(), p.get_value());
    return os;
}

inline std::ostream& operator<<(std::ostream& os, nvtx3::message const& m)
{
    WriteMessage(os, m.get_type(), m.get_value());
    return os;
}
