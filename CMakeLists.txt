# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

# Unused, but required by cmake. Real project is in c/.
project(nvtx_dummy_project)

# All of the cmake logic lives in c/:
add_subdirectory(c)
