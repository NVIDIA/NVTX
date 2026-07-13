/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#define NVTX3_CPP_REQUIRE_EXPLICIT_VERSION

#include <nvtx3/nvtx3.hpp>

int main()
{
  nvtx3::v1::scoped_range range{"user-range"};
  return 0;
}
