# SPDX-FileCopyrightText: Copyright (c) 2021-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

import sys
import pytest

from .conftest import verify_push, verify_pop, verify_registration_events
from nvtx._lib.profiler import Profile


@pytest.fixture
def profiler():
    return Profile(linenos=False, annotate_cfuncs=False)


def test_profiler(nvtx_events, profiler):
    domain = "nvtx.py"
    message = "foo"

    def foo():
        pass

    profiler.enable()
    assert sys.getprofile() == (profiler._profile if nvtx_events else None)
    foo()
    profiler.disable()

    if nvtx_events:
        verify_registration_events(nvtx_events, domain, message)
        verify_push(nvtx_events, domain, message, None, None, None)
        verify_pop(nvtx_events, domain)
