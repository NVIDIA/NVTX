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

import shutil

from Cython.Build import cythonize
from pathlib import Path
from setuptools import setup
from setuptools.command.sdist import sdist
from setuptools.extension import Extension


# ../c/include
c_include_path = Path(__file__).parent.parent / 'c' / 'include'

# When building from source distribution (.tar.gz), ./include dir exists (added by sdist command)
# Otherwise, we are building from sources, so we need to use `c_include_path`
include_dirs = ['include' if Path('include').exists() else str(c_include_path)]


class NvtxSdist(sdist):
    def run(self):
        try:
            shutil.copytree(c_include_path, 'include')
            super().run()
        finally:
            shutil.rmtree('include', ignore_errors=True)


setup(
    cmdclass=dict(sdist=NvtxSdist),
    ext_modules=cythonize(
        Extension('*', sources=['src/nvtx/_lib/*.pyx'], include_dirs=include_dirs),
        compiler_directives=dict(language_level=3, embedsignature=True))
)
