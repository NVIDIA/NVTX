# Copyright 2020  NVIDIA Corporation.  All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Utilities for implementing no-op functions/methods

import contextlib

from functools import wraps


class NullContextDecorator(contextlib.nullcontext):
    def __init__(self, *args, **kwargs):
        pass

    def __call__(self, func):
        return func


def disable(func):
    # disable the wrapped function
    @wraps(func)
    def inner(*args, **kwargs):
        pass
    return inner
