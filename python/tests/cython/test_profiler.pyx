# SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

from nvtx._lib.lib import _to_bytes
from nvtx._lib.lib cimport *
from nvtx._lib.profiler cimport Profile


def test_profiler_message():
    # test that when we call a function foo(), the name of the
    # function is captured correctly.

    prof = Profile(linenos=False)

    def foo():
        assert prof.__attrib.message.registered == nvtxDomainRegisterStringA(
            prof.__domain, _to_bytes("foo"))

    prof.enable()
    foo()
    prof.disable()
