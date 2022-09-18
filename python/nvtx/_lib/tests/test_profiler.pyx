# Copyright 2021-2022 NVIDIA Corporation.  All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
        got_fname = got_message.split(":")[0]
        got_funcname = got_message.split("(")[1][:-1]
        assert got_fname == "test_profiler.pyx"
        # assert got_lineno == ???  # see note above
        assert got_funcname == "foo"

    prof.enable()
    foo()
    prof.disable()
