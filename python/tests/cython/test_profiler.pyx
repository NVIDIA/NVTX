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

import os
import sys

import cython

from nvtx._lib.profiler cimport Profile
from nvtx._lib.profiler import Profile


def test_profiler_message():
    # test that when we call a function foo(), the name of the
    # function and its module are captured correctly. Ideally, we
    # should also be testing that we capture its location within the
    # module (i.e., line number info) correctly, but I can't figure
    # out how to do that in a Cython module. If we were in Python
    # land, it would be something like:
    #
    # got_lineno = sys._getframe().f_code.co_firstlineno

    prof = Profile()

    def foo():
        got_message = prof.__attrib.message
        got_fname = got_message.string.split(":")[0]
        got_funcname = got_message.string.split("(")[1][:-1]
        assert got_fname == "test_profiler.pyx"
        # assert got_lineno == ???  # see note above
        assert got_funcname == "foo"

    prof.enable()
    foo()
    prof.disable()
