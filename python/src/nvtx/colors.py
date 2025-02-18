# SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://nvidia.github.io/NVTX/LICENSE.txt for license information.

import functools

_NVTX_COLORS = {
    None: 0x0000FF,  # blue
    "green": 0x008000,
    "blue": 0x0000FF,
    "yellow": 0xFFFF00,
    "purple": 0x800080,
    "rapids": 0x7400FF,
    "cyan": 0x00FFFF,
    "red": 0xFF0000,
    "white": 0xFFFFFF,
    "darkgreen": 0x006400,
    "orange": 0xFFA500,
}


@functools.lru_cache()
def color_to_hex(color=None):
    """
    Convert color to ARGB hex value.
    """
    if isinstance(color, int):
        return color
    if color in _NVTX_COLORS:
        return _NVTX_COLORS[color]
    try:
        import matplotlib.colors
    except ImportError as e:
        raise TypeError(
            f"Invalid color {color}. Please install matplotlib "
            "for additional colors support"
        ) from e
    rgba = matplotlib.colors.to_rgba(color)
    argb = (rgba[-1], rgba[0], rgba[1], rgba[2])
    return int(matplotlib.colors.to_hex(argb, keep_alpha=True)[1:], 16)
