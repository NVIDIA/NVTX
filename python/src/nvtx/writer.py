# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

"""Write pre-collected trace data through an NVTXW backend.

Regular :mod:`nvtx` calls annotate work as it executes. This module instead
submits events and counter samples with explicit timestamps to a writer
backend. The backend decides where the data is written or how it is merged.

Backend implementations and configuration are supplied by tools or other
integrations. For example, NVIDIA Nsight Systems provides the ``nsys_writer``
integration; see the `Nsight Systems documentation
<https://docs.nvidia.com/nsight-systems/AnalysisGuide/index.html#writing-post-collection-events>`_
for its setup and report-specific usage.
"""

from nvtx._lib.writer import (
    Backend,
    Counter,
    Domain,
    PredefinedScope,
    RegisteredString,
    Schema,
    Scope,
    Session,
    Stream,
    StreamInterleaving,
    StreamOrdering,
    StreamSkid,
    WriterError,
    load_backend,
)

__all__ = [
    "Backend",
    "Counter",
    "Domain",
    "PredefinedScope",
    "RegisteredString",
    "Schema",
    "Scope",
    "Session",
    "Stream",
    "StreamInterleaving",
    "StreamOrdering",
    "StreamSkid",
    "WriterError",
    "load_backend",
]
