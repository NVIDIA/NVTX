# Copyright (c) 2020, NVIDIA CORPORATION.

import os

from nvtx.nvtx import _annotate_nop, annotate, pop_range, push_range, mark

if os.getenv("PYNVTX_DISABLE"):
    annotate = _annotate_nop
