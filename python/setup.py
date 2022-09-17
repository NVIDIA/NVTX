# Copyright 2020-2022 NVIDIA Corporation.  All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import os
import glob
import sysconfig
from distutils.sysconfig import get_python_lib

from Cython.Build import cythonize
from setuptools import find_packages, setup
from setuptools.extension import Extension

cython_files = ["nvtx/**/*.pyx"]

try:
    nthreads = int(os.environ.get("PARALLEL_LEVEL", "0") or "0")
except Exception:
    nthreads = 0

include_dirs = [os.path.dirname(sysconfig.get_path("include")),]
library_dirs = [get_python_lib()]

extensions = [
    Extension(
        "*",
        sources=cython_files,
        include_dirs=include_dirs,
        library_dirs=library_dirs,
        language="c",
    )
]

cython_tests = glob.glob("nvtx/_lib/tests/*.pyx")

# tests:
extensions += cythonize(
    [
        Extension(
            "*",
            sources=cython_tests,
            include_dirs=include_dirs,
            library_dirs=library_dirs,
            language="c"
        )
    ],
    nthreads=nthreads,
    compiler_directives=dict(
        profile=True, language_level=3, embedsignature=True, binding=True
    ),
)


setup(
    name="nvtx",
    version="0.2.5",
    description="PyNVTX - Python code annotation library",
    url="https://github.com/NVIDIA/nvtx",
    author="NVIDIA Corporation",
    license="Apache 2.0",
    classifiers=[
        "Intended Audience :: Developers",
        "Topic :: Database",
        "Topic :: Scientific/Engineering",
        "License :: OSI Approved :: Apache Software License",
        "Programming Language :: Python",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
    ],
    # Include the separately-compiled shared library
    ext_modules=cythonize(
        extensions,
        nthreads=nthreads,
        compiler_directives=dict(
            profile=False, language_level=3, embedsignature=True
        ),
    ),
    packages=find_packages(include=["nvtx", "nvtx.*"]),
    package_data=dict.fromkeys(
        find_packages(include=["nvtx._lib*"]), ["*.pxd"],
    ),
    license_files=["LICENSE.txt"],
    zip_safe=False,
)
