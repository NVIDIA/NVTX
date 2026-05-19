/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include <nvtx3/nvtx3.hpp>

int main() {
    nvtx3::mark("Hello NVTX!");
    return 0;
}
