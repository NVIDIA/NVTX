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

import ctypes
import os
import threading
from pathlib import Path


_GET_INTERFACE_SYMBOL_NAME = NVTXW_GET_INTERFACE_SYMBOL_NAME.decode()
_FINALIZE_SYMBOL_NAME = NVTXW_FINALIZE_SYMBOL_NAME.decode()
_BACKEND_MODULES = {}
_BACKEND_MODULES_LOCK = threading.RLock()


_RESULT_MESSAGES = {
    NVTXW_RESULT_SUCCESS: "success",
    NVTXW_RESULT_FAILED: "failed",
    NVTXW_RESULT_INVALID_ARGUMENT: "invalid argument",
    NVTXW_RESULT_LIBRARY_NOT_FOUND: "library not found",
    NVTXW_RESULT_LIBRARY_LOAD_FAILED: "library load failed",
    NVTXW_RESULT_LIBRARY_SYMBOL_MISSING: "library symbol missing",
    NVTXW_RESULT_INTERFACE_VERSION_NOT_SUPPORTED:
        "interface version not supported",
    NVTXW_RESULT_NOT_SUPPORTED: "not supported by this backend",
}


class WriterError(Exception):
    """
    Raised when an NVTXW backend call returns a non-success result code.

    Parameters
    ----------
    result_code : int
        The ``NVTXW_RESULT_*`` code returned by the failing call.
    operation : str, optional
        Name of the backend call that failed.

    Attributes
    ----------
    result_code : int
        The raw ``NVTXW_RESULT_*`` code returned by the failing call.
    operation : str or None
        Name of the backend call that failed, if known.
    """

    def __init__(self, int result_code, operation=None):
        self.result_code = result_code
        self.operation = operation
        message = _RESULT_MESSAGES.get(result_code, "unknown error")
        if operation:
            message = f"{operation} failed: {message} (code {result_code})"
        super().__init__(message)


cdef void _check(nvtxwResultCode_t rc, str op) except *:
    if rc != NVTXW_RESULT_SUCCESS:
        raise WriterError(rc, op)


cdef bytes _as_bytes(object s):
    if s is None:
        return None
    if isinstance(s, bytes):
        return <bytes>s
    if isinstance(s, str):
        return (<str>s).encode("utf-8")
    raise TypeError("expected a str or bytes")


def _canonical_backend_path(path):
    """Return the canonical registry key and filename for a backend path."""
    path = Path(os.fsdecode(path)).expanduser().resolve()
    return os.path.normcase(os.fspath(path))


cdef class _BackendModule:
    """One canonical backend module shared by all public wrappers."""

    def __cinit__(self, lib):
        self._iface = NULL
        self._lib = lib
        self._refcount = 0

    cdef void _initialize(self) except *:
        cdef nvtxwGetInterface_t get_interface
        cdef const nvtxwInterface_v2_t* iface = NULL
        cdef nvtxwResultCode_t rc
        get_interface_obj = getattr(
            self._lib, _GET_INTERFACE_SYMBOL_NAME, None)
        if get_interface_obj is None:
            raise WriterError(
                NVTXW_RESULT_LIBRARY_SYMBOL_MISSING, "load_backend")
        get_interface_addr = ctypes.cast(
            get_interface_obj, ctypes.c_void_p).value
        if not get_interface_addr:
            raise WriterError(
                NVTXW_RESULT_LIBRARY_SYMBOL_MISSING, "load_backend")
        get_interface = \
            <nvtxwGetInterface_t><void*><uintptr_t>get_interface_addr
        rc = get_interface(
            NVTXW_INTERFACE_VERSION, <const void**>&iface)
        _check(rc, _GET_INTERFACE_SYMBOL_NAME)
        if iface == NULL:
            raise WriterError(NVTXW_RESULT_FAILED, _GET_INTERFACE_SYMBOL_NAME)
        self._iface = iface

    cdef void _finalize(self) except *:
        self._iface = NULL
        finalize = getattr(self._lib, _FINALIZE_SYMBOL_NAME, None)
        if finalize is not None:
            finalize.restype = None
            finalize()


def _release_backend_module(module):
    """Drop one wrapper reference and finalize the module at zero."""
    cdef _BackendModule loaded = module
    with _BACKEND_MODULES_LOCK:
        if loaded._refcount <= 0:
            raise RuntimeError("backend module reference count underflow")
        loaded._refcount -= 1
        if loaded._refcount == 0:
            loaded._finalize()


cdef class Backend:
    """
    A loaded NVTXW backend: the module handle plus its interface table.

    Do not construct directly; use :func:`load_backend`.
    """

    def __cinit__(self, module):
        if not isinstance(module, _BackendModule):
            raise ValueError(
                "module must be a loaded backend module; "
                "use nvtx.writer.load_backend to create a Backend"
            )
        self._iface = (<_BackendModule>module)._iface
        self._module = module

    def close(self):
        """
        Finalize and release the backend.

        Calling this method is optional because backends normally remain
        loaded until the process exits. Calling it more than once has no
        effect. When multiple backends were loaded from the same library, the
        library is finalized only after every wrapper is closed. Its module
        remains loaded until process exit.

        Raises
        ------
        WriterError
            If the backend fails to end an active session.
        """
        if self._module is None:
            return
        module = self._module
        self._iface = NULL
        self._module = None
        _release_backend_module(module)


def load_backend(path):
    """
    Load and initialize an NVTXW backend from an explicit library path.

    Parameters
    ----------
    path : pathlib.Path or str
        Path to the backend shared library.

    Returns
    -------
    Backend
        The loaded backend.

    Raises
    ------
    OSError
        If the library cannot be loaded.
    WriterError
        If the backend does not export ``nvtxwGetInterface`` or does not
        provide the requested interface version.
    """

    cdef _BackendModule loaded

    canonical_path = _canonical_backend_path(path)
    with _BACKEND_MODULES_LOCK:
        module = _BACKEND_MODULES.get(canonical_path)
        if module is None:
            lib = ctypes.CDLL(canonical_path)
            module = _BackendModule(lib)
            _BACKEND_MODULES[canonical_path] = module

        loaded = <_BackendModule>module
        if loaded._refcount == 0:
            try:
                loaded._initialize()
            except Exception:
                loaded._finalize()
                raise
        loaded._refcount += 1
        try:
            return Backend(module)
        except Exception:
            _release_backend_module(module)
            raise
