# Copyright 2020-2022 NVIDIA Corporation.  All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

class CachedInstanceMeta(type):
    __instances = {}

    def __call__(self, *args, **kwargs):
        arg_tuple = args + tuple(kwargs.values())
        if arg_tuple in self.__instances:
            return self.__instances[arg_tuple]
        else:
            obj = super().__call__(*args, **kwargs)
            self.__instances[arg_tuple] = obj
            return obj
