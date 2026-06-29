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
import enum
import functools
import itertools
import os
import threading
from pathlib import Path

from nvtx import (
    PredefinedScope,
    TimestampType,
    numpy_dtype,
)
from nvtx.colors import color_to_hex
from nvtx._metadata import PayloadSchemaKey


_GET_INTERFACE_SYMBOL_NAME = NVTXW_GET_INTERFACE_SYMBOL_NAME.decode()
_FINALIZE_SYMBOL_NAME = NVTXW_FINALIZE_SYMBOL_NAME.decode()
_BACKEND_MODULES = {}
_BACKEND_MODULES_LOCK = threading.RLock()


class StreamInterleaving(enum.Enum):
    """Whether ordering guarantees apply across the whole stream or per scope."""
    NONE = NVTXW_STREAM_ORDER_INTERLEAVING_NONE
    SCOPE = NVTXW_STREAM_ORDER_INTERLEAVING_SCOPE


class StreamOrdering(enum.Enum):
    """How fully events are sorted in the stream."""
    UNKNOWN = NVTXW_STREAM_ORDERING_TYPE_UNKNOWN
    STRICT = NVTXW_STREAM_ORDERING_TYPE_STRICT
    PACKED_RANGE_START = NVTXW_STREAM_ORDERING_TYPE_PACKED_RANGE_START
    PACKED_RANGE_END = NVTXW_STREAM_ORDERING_TYPE_PACKED_RANGE_END


class StreamSkid(enum.Enum):
    """How partial-sort "skid" is quantified (paired with ``skid_amount``)."""
    NONE = NVTXW_STREAM_ORDERING_SKID_NONE
    TIME_NS = NVTXW_STREAM_ORDERING_SKID_TIME_NS
    EVENT_COUNT = NVTXW_STREAM_ORDERING_SKID_EVENT_COUNT


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


cdef class _WriterSchemaRegistrar(SchemaRegistrar):
    """Registers schemas via the backend interface's ``SchemaRegister``."""

    cdef Domain _domain

    cdef uint64_t _do_register(
        self, const nvtxPayloadSchemaAttr_t* attr
    ) except *:
        cdef uint64_t schema_id = 0
        cdef Domain domain = self._domain
        cdef nvtxwResultCode_t rc
        with domain._session._lock:
            domain._ensure_valid()
            rc = domain._backend._iface.SchemaRegister(
                domain._handle, attr, &schema_id)
            _check(rc, "SchemaRegister")
        return schema_id


cdef bytes _as_bytes(object s):
    if s is None:
        return None
    if isinstance(s, bytes):
        return <bytes>s
    if isinstance(s, str):
        return (<str>s).encode("utf-8")
    raise TypeError("expected a str or bytes")


cdef object _as_str(object s):
    if s is None or isinstance(s, str):
        return s
    if isinstance(s, bytes):
        return (<bytes>s).decode("utf-8")
    raise TypeError("expected a str or bytes")


cdef uint32_t _resolve_color(object color) except? 0:
    # NVTXW event attributes use 0 for "no explicit color", so ``None`` maps to
    # 0 rather than the instrumentation default (blue).
    if color is None:
        return 0
    return <uint32_t>color_to_hex(color)


cdef uint32_t _resolve_category(Domain domain, object category) except? 0:
    # None -> 0 (no category); str/bytes are named via the domain; ints pass
    # through and are reserved so the named-category allocator skips them.
    if category is None:
        return 0
    if isinstance(category, (str, bytes)):
        return <uint32_t>domain.get_category_id(category)
    if isinstance(category, int):
        if category < 0 or category > 0xFFFFFFFF:
            raise ValueError("category must be in [0, 2**32 - 1]")
        # Reserving an integer category only needs the lifecycle lock once.
        # The second check handles another thread reserving it first.
        if category not in domain._user_category_ids:
            with domain._session._lock:
                domain._ensure_valid()
                if category not in domain._user_category_ids:
                    domain._user_category_ids.add(category)
        return <uint32_t>category
    raise TypeError("category must be a str, bytes, int, or None")


cdef uint64_t _resolve_scope_id(Domain domain, object scope) except? 0:
    if scope is None:
        return NVTX_SCOPE_NONE
    if isinstance(scope, PredefinedScope):
        return <uint64_t>scope.value
    if isinstance(scope, Scope):
        if domain is None:
            raise ValueError("scope requires a registered stream domain")
        if (<Scope>scope)._domain is not domain:
            raise ValueError("scope is not registered in this domain")
        return (<Scope>scope)._scope_id
    if isinstance(scope, int) and not isinstance(scope, bool):
        if not (
            NVTX_SCOPE_ID_STATIC_START
            <= scope
            < NVTX_SCOPE_ID_DYNAMIC_START
        ):
            raise ValueError(
                "integer scope must be in the NVTX static scope ID range "
                f"[{NVTX_SCOPE_ID_STATIC_START}, "
                f"{NVTX_SCOPE_ID_DYNAMIC_START})"
            )
        return <uint64_t>scope
    raise TypeError(
        "scope must be None, a PredefinedScope, a Scope, or an int"
    )


cdef uint64_t _resolve_stream_scope_id(
    Domain domain, object scope
) except? 0:
    if isinstance(scope, PredefinedScope):
        if scope is PredefinedScope.NONE or scope is PredefinedScope.ROOT:
            return <uint64_t>scope.value
        raise ValueError(
            "runtime-resolved scopes are not valid as stream defaults"
        )
    return _resolve_scope_id(domain, scope)


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
        self._lock = threading.RLock()
        self._active_sessions = set()

    cdef void _ensure_open(self) except *:
        if self._iface == NULL or self._module is None:
            raise RuntimeError("backend is closed")

    def close(self):
        """
        Finalize and release the backend.

        Active sessions, and their open streams, are ended first.

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
        # Best-effort teardown: mark the backend closed and re-raise the first
        # session failure at the end. Only finalize after every session ends.
        error = None
        module = None
        with self._lock:
            if self._module is None:
                return
            for session in list(self._active_sessions):
                try:
                    (<Session>session).end()
                except Exception as exc:
                    if error is None:
                        error = exc
            if error is None:
                module = self._module
                self._iface = NULL
                self._module = None
        if error is not None:
            raise error
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


cdef class Session:
    """
    An NVTXW session: the top-level container data is written into.

    Use as a context manager (recommended) or call :meth:`begin` and
    :meth:`end` explicitly.

    Parameters
    ----------
    name : str or bytes
        Session name. Tools may display it or use it to name a file or
        directory representing the session.
    backend : Backend
        Backend the session writes to, from :func:`load_backend`.
    config : str or bytes, optional
        Backend-specific configuration options, one ``key=value`` pair per
        line. Backends use reasonable defaults for options not provided and
        ignore keys they do not support.

    Notes
    -----
    Writes to distinct streams may proceed concurrently. Calls on the same
    stream must not overlap unless the caller synchronizes them. Registration
    and lifecycle operations must not overlap writes: complete setup before
    starting writer threads, and stop them before closing streams or ending
    the session.
    """

    def __init__(self, name, *, backend, config=None):
        if not isinstance(backend, Backend):
            raise TypeError("backend must be an nvtx.writer.Backend")
        self._backend = backend
        self._name = _as_bytes(name)
        self._config = _as_bytes(config)
        self._handle = NULL
        self._lock = threading.RLock()
        self._open_streams = set()
        self._domains = {}

    def begin(self):
        """
        Start the session.

        When applicable, prefer using a context manager
        (``with Session("my session", backend=backend) as session:``) over
        calling this method explicitly.

        Raises
        ------
        RuntimeError
            If the session is already active or the backend is closed.
        WriterError
            If the backend fails to begin the session.
        """
        cdef nvtxwSessionAttributes_t attr
        cdef const char* name = NULL
        cdef const char* config = NULL
        cdef nvtxwResultCode_t rc
        # When both are needed, always acquire the backend lock first.
        with self._backend._lock:
            with self._lock:
                if self._handle != NULL:
                    raise RuntimeError("session is already active")
                self._backend._ensure_open()
                if self._name is not None:
                    name = self._name
                if self._config is not None:
                    config = self._config
                attr.structSize = sizeof(nvtxwSessionAttributes_t)
                attr.name = name
                attr.configString = config
                self._backend._active_sessions.add(self)
                rc = self._backend._iface.SessionBegin(
                    &attr, &self._handle)
                if rc != NVTXW_RESULT_SUCCESS:
                    self._backend._active_sessions.discard(self)
                    self._handle = NULL
                    raise WriterError(rc, "SessionBegin")

    def end(self):
        """
        End the session.

        Streams still open in this session are closed first. Objects created
        from the session (domains, streams, registered strings, and scopes)
        become invalid when it ends.

        When applicable, prefer using a context manager
        (``with Session("my session", backend=backend) as session:``) over
        calling this method explicitly.

        Raises
        ------
        RuntimeError
            If the session is not active or the backend is closed.
        WriterError
            If the backend fails to close a stream or end the session.
        """
        cdef nvtxwSessionHandle_t handle
        cdef nvtxwResultCode_t rc
        # Best-effort teardown: always reach SessionEnd and leave the session
        # in a consistent terminal state, re-raising the first failure at the
        # end.
        error = None
        with self._backend._lock:
            with self._lock:
                if self._handle == NULL:
                    raise RuntimeError("session is not active")
                self._backend._ensure_open()
                for stream in list(self._open_streams):
                    try:
                        stream.close()
                    except Exception as exc:
                        if error is None:
                            error = exc
                self._open_streams.clear()
                handle = self._handle
                self._handle = NULL
                for domain in self._domains.values():
                    (<Domain>domain)._invalidate()
                self._domains.clear()
                self._backend._active_sessions.discard(self)
                rc = self._backend._iface.SessionEnd(handle)
                if rc != NVTXW_RESULT_SUCCESS:
                    raise WriterError(rc, "SessionEnd") from error
        if error is not None:
            raise error

    def _register_domain(self, name):
        cdef nvtxwDomainAttributes_t attr
        cdef nvtxDomainHandle_t handle = NULL
        cdef nvtxwResultCode_t rc
        cdef bytes encoded = _as_bytes(name)
        cdef const char* c_name = NULL
        if encoded is not None:
            c_name = encoded
        self._backend._ensure_open()
        attr.structSize = sizeof(nvtxwDomainAttributes_t)
        attr.name = c_name
        rc = self._backend._iface.DomainRegister(self._handle, &attr, &handle)
        _check(rc, "DomainRegister")
        return Domain(self, <uintptr_t>handle)

    def get_domain(self, name=None):
        """
        Get or create a domain within this session.

        Parameters
        ----------
        name : str or bytes, optional
            Domain name. ``None`` or an empty name selects the session's
            default domain.

        Returns
        -------
        Domain
            The requested domain. Repeated calls with the same name return
            the same object.

        Raises
        ------
        RuntimeError
            If the session is not active.
        TypeError
            If ``name`` is not a str, bytes, or None.
        WriterError
            If the backend fails to register the domain.
        """
        cdef Domain domain
        with self._lock:
            if self._handle == NULL:
                raise RuntimeError("session is not active")
            name = _as_str(name) or None
            domain = self._domains.get(name)
            if domain is None:
                domain = self._register_domain(name)
                self._domains[name] = domain
            return domain

    def create_stream(
        self,
        name,
        *,
        domain=None,
        scope=None,
        time_domain_id=NVTX_TIME_DOMAIN_ID_NONE,
        interleaving=StreamInterleaving.NONE,
        ordering=StreamOrdering.UNKNOWN,
        skid=StreamSkid.NONE,
        skid_amount=0,
    ):
        """
        Create a stream associated with this session.

        This method configures the stream but does not open it. Use the stream
        as a context manager or call :meth:`Stream.open` before writing events.

        Parameters
        ----------
        name : str or bytes
            Stream name.
        domain : Domain, str, bytes, optional
            Domain object or domain name. ``None`` selects the session default
            domain. Names and ``None`` are resolved with :meth:`get_domain`.
        scope : PredefinedScope, Scope, int, or None, optional
            Default scope associated with the stream. Pass
            :attr:`PredefinedScope.NONE`, :attr:`PredefinedScope.ROOT`, or a
            dynamic scope returned by :meth:`Domain.get_scope` for this
            stream's domain. An integer must be in the NVTX static scope ID
            range.
        time_domain_id : TimestampType or int, optional
            Time domain used by event timestamps.
        interleaving : StreamInterleaving, optional
            Whether ordering guarantees apply across the stream or per scope.
        ordering : StreamOrdering, optional
            Ordering guarantee for events in the stream.
        skid : StreamSkid, optional
            Unit used to express the partial-sort skid.
        skid_amount : int, optional
            Maximum partial-sort skid in the unit selected by ``skid``.

        Returns
        -------
        Stream
            A configured, unopened stream.

        Raises
        ------
        RuntimeError
            If the session is not active or the domain is no longer valid.
        TypeError
            If ``interleaving``, ``ordering``, ``skid``, or
            ``time_domain_id`` has an unsupported type, or ``scope`` is not a
            supported stream scope.
        ValueError
            If ``domain`` belongs to a different session, ``scope`` is
            registered in a different domain, or a runtime-resolved
            predefined scope or an integer outside the static scope ID range
            is provided.
        """
        if not isinstance(interleaving, StreamInterleaving):
            raise TypeError(
                "interleaving must be an nvtx.writer.StreamInterleaving")
        if not isinstance(ordering, StreamOrdering):
            raise TypeError("ordering must be an nvtx.writer.StreamOrdering")
        if not isinstance(skid, StreamSkid):
            raise TypeError("skid must be an nvtx.writer.StreamSkid")
        if not isinstance(time_domain_id, (TimestampType, int)):
            raise TypeError(
                "time_domain_id must be an nvtx.TimestampType or an int")
        with self._lock:
            if self._handle == NULL:
                raise RuntimeError("session is not active")
            if not isinstance(domain, Domain):
                domain = self.get_domain(domain)
            elif (<Domain>domain)._session is not self:
                raise ValueError("domain belongs to a different session")
            else:
                (<Domain>domain)._ensure_valid()
            return Stream(
                self,
                name,
                domain,
                scope,
                time_domain_id,
                interleaving,
                ordering,
                skid,
                skid_amount,
            )

    def __enter__(self):
        self.begin()
        return self

    def __exit__(self, *_):
        # Backend.close() may already have ended this session.
        with self._backend._lock:
            with self._lock:
                if self._handle != NULL:
                    self.end()


cdef class RegisteredString:
    """
    Wrapper for ``nvtxStringHandle_t``, created by
    :meth:`Domain.get_registered_string`.

    Pass to the single-event range/mark write methods in place of a ``str``
    message to emit a registered-string handle instead of an inline UTF-8
    message.  Valid until the owning session ends.
    """

    def __cinit__(self, Domain domain, string, handle_addr):
        self._domain = domain
        self._string = string
        self._handle = \
            <nvtxStringHandle_t><void*><uintptr_t>handle_addr

    def __repr__(self):
        return f"RegisteredString({self._string!r})"


cdef class Scope:
    """
    Wrapper for a registered scope ID.
    Created by :meth:`Domain.get_scope`.
    Valid until the owning session ends.
    """

    def __cinit__(self, uint64_t scope_id, path=None, domain=None):
        self._scope_id = scope_id
        self._path = path
        self._domain = domain

    def __repr__(self):
        return f"Scope(scope_id={self._scope_id}, path={self._path!r})"

    @property
    def scope_id(self):
        """Registered scope ID within the domain."""
        return self._scope_id

    @property
    def path(self):
        """The path the scope was registered with (``None`` if unnamed)."""
        return self._path


cdef class Domain:
    """
    Session-owned NVTX domain. Created by :meth:`Session.get_domain`.
    Valid until the owning session ends.
    """

    def __cinit__(self, Session session, handle_addr):
        self._session = session
        self._backend = session._backend
        self._valid = True
        self._handle = <nvtxDomainHandle_t><void*><uintptr_t>handle_addr
        self._get_string_cached = functools.cache(self._register_string)
        self._get_scope_cached = functools.cache(self._register_scope)
        self._categories = {}
        # 0 is reserved for "no category", so IDs start at 1.
        self._category_ids = itertools.count(1)
        self._user_category_ids = set()
        cdef _WriterSchemaRegistrar registrar = _WriterSchemaRegistrar()
        registrar._domain = self
        self._schema_registrar = registrar

    cdef _invalidate(self):
        self._valid = False

    cdef _ensure_valid(self):
        if not self._valid:
            raise RuntimeError(
                "domain is no longer valid: its owning session has ended")

    cdef _get_event_schema_ids(
        self, nvtxwEventHelperSchemaIds_t* schema_ids_out
    ):
        cdef nvtxwResultCode_t rc
        with self._session._lock:
            self._ensure_valid()
            if not self._event_schemas_registered:
                rc = nvtxwEventSchemasRegister(
                    self._backend._iface,
                    self._handle,
                    NVTXW_EVENT_HELPER_SCHEMA_ALL,
                    &self._event_schema_ids,
                )
                _check(rc, "nvtxwEventSchemasRegister")
                self._event_schemas_registered = True
            schema_ids_out[0] = self._event_schema_ids

    def get_registered_string(self, string):
        """
        Get or create a registered string in this domain.

        Parameters
        ----------
        string : str or bytes
            String to register.

        Returns
        -------
        RegisteredString
            The registered string. Results are cached per domain; str and
            bytes spellings of the same string share one registration.

        Raises
        ------
        RuntimeError
            If the domain is no longer valid.
        TypeError
            If ``string`` is not a string or bytes object.
        WriterError
            If the backend fails to register the string.
        """
        if not isinstance(string, (str, bytes)):
            raise TypeError("string must be a str or bytes")
        with self._session._lock:
            self._ensure_valid()
            return self._get_string_cached(_as_str(string))

    def _register_string(self, string):
        cdef bytes encoded = _as_bytes(string)
        cdef const char* c_string = encoded
        cdef nvtxStringHandle_t handle = NULL
        cdef nvtxwResultCode_t rc = self._backend._iface.StringRegister(
            self._handle, c_string, &handle)
        _check(rc, "StringRegister")
        return RegisteredString(self, string, <uintptr_t>handle)

    def get_category_id(self, name: str | bytes):
        """
        Get or create a named category in this domain.

        Parameters
        ----------
        name : str or bytes
            Category name.

        Returns
        -------
        int
            The category ID. IDs start at 1 because 0 represents no category,
            and skip integer category values already passed to this domain's
            event write methods. Results are cached per domain.

        Raises
        ------
        RuntimeError
            If the domain is no longer valid.
        TypeError
            If ``name`` is not a string or bytes object.
        WriterError
            If the backend fails to register the category.
        """
        if not isinstance(name, (str, bytes)):
            raise TypeError("category name must be a str or bytes")
        self._ensure_valid()
        name = _as_str(name)
        category_id = self._categories.get(name)
        if category_id is not None:
            return category_id
        return self._register_category(name)

    def _register_category(self, name):
        cdef uint32_t category_id
        cdef bytes encoded
        cdef const char* c_name
        cdef nvtxwResultCode_t rc
        cached = None
        with self._session._lock:
            self._ensure_valid()
            cached = self._categories.get(name)
            if cached is not None:
                return cached
            category_id = next(self._category_ids)
            # Skip IDs the user already claimed by passing raw integer
            # categories to event writes.
            while category_id in self._user_category_ids:
                category_id = next(self._category_ids)
            encoded = _as_bytes(name)
            c_name = encoded
            rc = self._backend._iface.CategoryRegister(
                self._handle, category_id, c_name)
            _check(rc, "CategoryRegister")
            self._categories[name] = category_id
            return category_id

    def get_scope(self, path, *, parent=None):
        """
        Get or create a scope in this domain.

        Parameters
        ----------
        path : str or bytes
            Path of the scope relative to ``parent``.
        parent : PredefinedScope or Scope, optional
            Parent scope. ``None`` places the scope at the domain root. The
            predefined parents may be :attr:`PredefinedScope.NONE`,
            :attr:`PredefinedScope.ROOT`,
            :attr:`PredefinedScope.CURRENT_HW_MACHINE`, or
            :attr:`PredefinedScope.CURRENT_VM`.

        Returns
        -------
        Scope
            The registered scope. Results are cached for each ``path`` and
            parent pair.

        Raises
        ------
        RuntimeError
            If the domain is no longer valid.
        TypeError
            If ``parent`` is not a :class:`PredefinedScope`, a
            :class:`Scope`, or ``None``.
        ValueError
            If ``parent`` is registered in a different domain or is not a
            supported predefined parent scope.
        WriterError
            If the backend fails to register the scope.
        """
        cdef uint64_t parent_id
        if parent is None:
            parent_id = NVTX_SCOPE_ROOT
        elif isinstance(parent, PredefinedScope):
            if parent not in (
                PredefinedScope.NONE,
                PredefinedScope.ROOT,
                PredefinedScope.CURRENT_HW_MACHINE,
                PredefinedScope.CURRENT_VM,
            ):
                raise ValueError(
                    "predefined parent scope must be NONE, ROOT, "
                    "CURRENT_HW_MACHINE, or CURRENT_VM"
                )
            parent_id = <uint64_t>parent.value
        elif isinstance(parent, Scope):
            if (<Scope>parent)._domain is not self:
                raise ValueError(
                    "parent scope is registered in a different domain")
            parent_id = (<Scope>parent)._scope_id
        else:
            raise TypeError(
                "parent must be a PredefinedScope, a Scope, or None"
            )
        with self._session._lock:
            self._ensure_valid()
            return self._get_scope_cached(_as_str(path), parent_id)

    def _register_scope(self, path, uint64_t parent_id):
        cdef bytes encoded = _as_bytes(path)
        cdef const char* c_path = NULL
        cdef nvtxScopeAttr_t attr
        cdef uint64_t scope_id = 0
        cdef nvtxwResultCode_t rc
        if encoded is not None:
            c_path = encoded
        attr.structSize = sizeof(nvtxScopeAttr_t)
        attr.path = c_path
        attr.parentScope = parent_id
        attr.scopeId = NVTX_SCOPE_NONE
        rc = self._backend._iface.ScopeRegister(self._handle, &attr, &scope_id)
        _check(rc, "ScopeRegister")
        return Scope(scope_id, path, self)

    def get_schema(self, dtype):
        """
        Get or create a payload schema in this domain.

        Parameters
        ----------
        dtype : dtype-like
            Structured NumPy dtype. Fields may define event roles with
            :func:`nvtx.numpy_dtype` and its ``entry_kind`` argument.

        Returns
        -------
        int
            The schema ID, which is unique within this domain. Results are
            cached per domain and dtype.

        Raises
        ------
        RuntimeError
            If numpy is not installed or the domain is no longer valid.
        """
        with self._session._lock:
            self._ensure_valid()
            # numpy_dtype normalizes the input and raises if numpy is missing.
            return self._schema_registrar._get_numpy_dtype_schema(
                PayloadSchemaKey(numpy_dtype(dtype))
            )

cdef class Stream:
    """
    NVTXW stream: the object events and counter samples are written to.

    Created by :meth:`Session.create_stream`.  Use as a context manager
    (recommended) or call :meth:`open` and :meth:`close` explicitly before and
    after writing data.
    """

    def __cinit__(
        self,
        Session session,
        name,
        Domain domain,
        scope,
        time_domain_id,
        interleaving,
        ordering,
        skid,
        skid_amount,
    ):
        if domain._session is not session:
            raise RuntimeError(
                f"domain '{domain}' is not registered in this session. "
                "Do not construct streams directly; "
                "use Session.create_stream()."
            )
        self._session = session
        self._backend = session._backend
        self._domain = domain
        self._name = _as_bytes(name)
        self._scope_id = _resolve_stream_scope_id(domain, scope)
        if isinstance(time_domain_id, TimestampType):
            time_domain_id = time_domain_id.value
        self._time_domain_id = time_domain_id
        self._order_interleaving = interleaving.value
        self._ordering_type = ordering.value
        self._ordering_skid = skid.value
        self._ordering_skid_amount = skid_amount
        # ``iface`` is constant for the stream's lifetime; the stream handle is
        # set in open()/close() and the schema IDs lazily on the first write.
        self._writer.iface = self._backend._iface

    def open(self):
        """
        Open the stream for writing.

        Raises
        ------
        RuntimeError
            If the stream is already open or its domain is no longer valid.
        WriterError
            If the backend fails to open the stream.
        """
        cdef nvtxwStreamAttributes_t attr
        cdef const char* name = NULL
        cdef nvtxDomainHandle_t domain_handle = NULL
        cdef nvtxwResultCode_t rc
        with self._session._lock:
            if self._writer.stream != NULL:
                raise RuntimeError("stream is already open")
            if self._session._handle == NULL:
                raise RuntimeError("session is not active")
            if self._domain is not None:
                self._domain._ensure_valid()
                domain_handle = self._domain._handle
            if self._name is not None:
                name = self._name
            attr.structSize = sizeof(nvtxwStreamAttributes_t)
            attr.name = name
            attr.domain = domain_handle
            attr.scopeId = self._scope_id
            attr.timeDomainId = self._time_domain_id
            attr.orderInterleaving = self._order_interleaving
            attr.orderingType = self._ordering_type
            attr.orderingSkid = self._ordering_skid
            attr.orderingSkidAmount = self._ordering_skid_amount
            rc = self._backend._iface.StreamOpen(
                self._session._handle, &attr, &self._writer.stream)
            if rc != NVTXW_RESULT_SUCCESS:
                self._writer.stream = NULL
                raise WriterError(rc, "StreamOpen")
            self._session._open_streams.add(self)

    def close(self):
        """
        Close the stream without ending its session.

        Raises
        ------
        RuntimeError
            If the stream is not open or the backend is closed.
        WriterError
            If the backend fails to close the stream.
        """
        cdef nvtxwStreamHandle_t handle
        cdef nvtxwResultCode_t rc
        with self._session._lock:
            if self._writer.stream == NULL:
                raise RuntimeError("stream is not open")
            self._backend._ensure_open()
            handle = self._writer.stream
            self._writer.stream = NULL
            self._session._open_streams.discard(self)
            rc = self._backend._iface.StreamClose(handle)
            _check(rc, "StreamClose")

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, *_):
        with self._session._lock:
            if self._writer.stream != NULL:
                self.close()

    cdef void _ensure_writable(self) except *:
        if self._writer.stream == NULL:
            raise RuntimeError("stream is not open")
        self._backend._ensure_open()

    cdef nvtxwEventWriter_t* _get_writer(self) except NULL:
        # ``iface`` is set at construction and the stream handle in
        # open()/close().  The domain's event schema IDs are registered lazily,
        # on the first event write into the domain (``_get_event_schema_ids`` is
        # memoized and shared by every stream in the domain), and copied into
        # the writer once.  They are stable for the rest of the session, so the
        # copy never needs repeating or invalidating on close.
        self._ensure_writable()
        if self._domain is None:
            raise ValueError(
                "event helpers require a registered stream domain"
            )
        if not self._writer_ready:
            self._domain._get_event_schema_ids(&self._writer.schemaIds)
            self._writer_ready = True
        return &self._writer

    cdef object _resolve_event_attrs(
        self,
        object message,
        object color,
        object category,
        nvtxwEventAttributes_t* attr,
        nvtxwEventAttributesUtf8_t* uattr,
    ):
        cdef uint32_t color_argb = _resolve_color(color)
        cdef uint32_t category_id = _resolve_category(self._domain, category)
        cdef bytes message_bytes
        cdef const char* message_ptr
        if message is None:
            message = b""
        if isinstance(message, RegisteredString):
            # Registered-string form: the handle is owned by the domain, so
            # nothing needs to be kept alive past the call.
            if (<RegisteredString>message)._domain is not self._domain:
                raise ValueError(
                    "message is a RegisteredString from a different domain")
            attr.color = color_argb
            attr.category = category_id
            attr.message = (<RegisteredString>message)._handle
            return None
        if isinstance(message, (str, bytes)):
            message_bytes = _as_bytes(message)
            message_ptr = message_bytes
            uattr.color = color_argb
            uattr.category = category_id
            uattr.messageLength = <uint32_t>len(message_bytes)
            uattr.message = message_ptr
            return message_bytes
        raise TypeError(
            "message must be a str, bytes, RegisteredString, or None")

    cdef void _emit_at(
        self, int64_t timestamp, object message, object color, object category,
        _at_bin_fn bin_fn, _at_utf8_fn utf8_fn, str op, str utf8_op,
    ) except *:
        cdef nvtxwEventWriter_t* writer = self._get_writer()
        cdef nvtxwEventAttributes_t attr
        cdef nvtxwEventAttributesUtf8_t uattr
        cdef bytes owner = self._resolve_event_attrs(
            message, color, category, &attr, &uattr)
        if owner is None:
            _check(bin_fn(writer, timestamp, attr), op)
        else:
            _check(utf8_fn(writer, timestamp, uattr), utf8_op)

    cdef void _emit_span(
        self, int64_t begin, int64_t end, object message, object color,
        object category, _span_bin_fn bin_fn, _span_utf8_fn utf8_fn,
        str op, str utf8_op,
    ) except *:
        cdef nvtxwEventWriter_t* writer = self._get_writer()
        cdef nvtxwEventAttributes_t attr
        cdef nvtxwEventAttributesUtf8_t uattr
        cdef bytes owner = self._resolve_event_attrs(
            message, color, category, &attr, &uattr)
        if owner is None:
            _check(bin_fn(writer, begin, end, attr), op)
        else:
            _check(utf8_fn(writer, begin, end, uattr), utf8_op)

    def write_mark(
        self, timestamp, *, message=None, color=None, category=None
    ):
        """
        Write a mark event.

        Parameters
        ----------
        timestamp : int
            Event timestamp in the stream's time domain.
        message : str, bytes, or RegisteredString, optional
            Message associated with the event.
        color : int or color-like, optional
            Event color. Integers are interpreted as ARGB values.
        category : str, bytes, or int, optional
            Event category. A name is registered in the stream's domain.

        Raises
        ------
        RuntimeError
            If the stream is not open or the backend is closed.
        TypeError
            If ``message`` or ``category`` has an unsupported type.
        ValueError
            If an integer ``category`` is outside ``[0, 2**32 - 1]``, or
            ``message`` is a :class:`RegisteredString` from a different domain.
        WriterError
            If the backend fails to write the event.
        """
        self._emit_at(timestamp, message, color, category,
                      nvtxwMarkWrite, nvtxwMarkWriteUtf8,
                      "nvtxwMarkWrite", "nvtxwMarkWriteUtf8")

    def write_pushpop(
        self, start, end, *, message=None, color=None, category=None
    ):
        """
        Write a complete push/pop range event.

        Parameters
        ----------
        start : int
            Range start timestamp in the stream's time domain.
        end : int
            Range end timestamp in the stream's time domain.
        message : str, bytes, or RegisteredString, optional
            Message associated with the range.
        color : int or color-like, optional
            Range color. Integers are interpreted as ARGB values.
        category : str, bytes, or int, optional
            Range category. A name is registered in the stream's domain.

        Raises
        ------
        RuntimeError
            If the stream is not open or the backend is closed.
        TypeError
            If ``message`` or ``category`` has an unsupported type.
        ValueError
            If an integer ``category`` is outside ``[0, 2**32 - 1]``, or
            ``message`` is a :class:`RegisteredString` from a different domain.
        WriterError
            If the backend fails to write the event.
        """
        self._emit_span(start, end, message, color, category,
                        nvtxwRangePushPopWrite, nvtxwRangePushPopWriteUtf8,
                        "nvtxwRangePushPopWrite",
                        "nvtxwRangePushPopWriteUtf8")

    def write_startend(
        self, start, end, *, message=None, color=None, category=None
    ):
        """
        Write a complete start/end range event.

        Parameters
        ----------
        start : int
            Range start timestamp in the stream's time domain.
        end : int
            Range end timestamp in the stream's time domain.
        message : str, bytes, or RegisteredString, optional
            Message associated with the range.
        color : int or color-like, optional
            Range color. Integers are interpreted as ARGB values.
        category : str, bytes, or int, optional
            Range category. A name is registered in the stream's domain.

        Raises
        ------
        RuntimeError
            If the stream is not open or the backend is closed.
        TypeError
            If ``message`` or ``category`` has an unsupported type.
        ValueError
            If an integer ``category`` is outside ``[0, 2**32 - 1]``, or
            ``message`` is a :class:`RegisteredString` from a different domain.
        WriterError
            If the backend fails to write the event.
        """
        self._emit_span(start, end, message, color, category,
                        nvtxwRangeStartEndWrite, nvtxwRangeStartEndWriteUtf8,
                        "nvtxwRangeStartEndWrite",
                        "nvtxwRangeStartEndWriteUtf8")

    def write_push(
        self, timestamp, *, message=None, color=None, category=None
    ):
        """
        Write the beginning of a push/pop range.

        Push/pop ranges are nested and pair in last-in, first-out order.

        Parameters
        ----------
        timestamp : int
            Range start timestamp in the stream's time domain.
        message : str, bytes, or RegisteredString, optional
            Message associated with the range.
        color : int or color-like, optional
            Range color. Integers are interpreted as ARGB values.
        category : str, bytes, or int, optional
            Range category. A name is registered in the stream's domain.

        Raises
        ------
        RuntimeError
            If the stream is not open or the backend is closed.
        TypeError
            If ``message`` or ``category`` has an unsupported type.
        ValueError
            If an integer ``category`` is outside ``[0, 2**32 - 1]``, or
            ``message`` is a :class:`RegisteredString` from a different domain.
        WriterError
            If the backend fails to write the event.
        """
        self._emit_at(timestamp, message, color, category,
                      nvtxwRangePushWrite, nvtxwRangePushWriteUtf8,
                      "nvtxwRangePushWrite", "nvtxwRangePushWriteUtf8")

    def write_pop(self, timestamp):
        """
        Write the end of the most recently pushed range.

        Parameters
        ----------
        timestamp : int
            Range end timestamp in the stream's time domain.

        Raises
        ------
        RuntimeError
            If the stream is not open or the backend is closed.
        WriterError
            If the backend fails to write the event.
        """
        _check(
            nvtxwRangePopWrite(self._get_writer(), timestamp),
            "nvtxwRangePopWrite")

    def write_start(
        self,
        timestamp,
        range_id,
        *,
        message=None,
        color=None,
        category=None,
    ):
        """
        Write the beginning of a start/end range.

        The range is paired with a subsequent :meth:`write_end` call that
        uses the same ``range_id``.

        Parameters
        ----------
        timestamp : int
            Range start timestamp in the stream's time domain.
        range_id : int
            Nonzero identifier used to pair the start with its end.
        message : str, bytes, or RegisteredString, optional
            Message associated with the range.
        color : int or color-like, optional
            Range color. Integers are interpreted as ARGB values.
        category : str, bytes, or int, optional
            Range category. A name is registered in the stream's domain.

        Raises
        ------
        RuntimeError
            If the stream is not open or the backend is closed.
        TypeError
            If ``message`` or ``category`` has an unsupported type.
        ValueError
            If ``range_id`` is zero, an integer ``category`` is outside
            ``[0, 2**32 - 1]``, or ``message`` is a :class:`RegisteredString`
            from a different domain.
        WriterError
            If the backend fails to write the event.
        """
        cdef nvtxwEventWriter_t* writer
        cdef nvtxRangeId_t rid
        cdef nvtxwEventAttributes_t attr
        cdef nvtxwEventAttributesUtf8_t uattr
        if range_id == 0:
            raise ValueError("range_id must be non-zero")
        rid = range_id
        writer = self._get_writer()
        cdef bytes owner = self._resolve_event_attrs(
            message, color, category, &attr, &uattr)
        if owner is None:
            _check(nvtxwRangeStartWrite(writer, timestamp, rid, attr),
                   "nvtxwRangeStartWrite")
        else:
            _check(nvtxwRangeStartWriteUtf8(writer, timestamp, rid, uattr),
                   "nvtxwRangeStartWriteUtf8")

    def write_end(self, timestamp, range_id):
        """
        Write the end of a start/end range.

        Parameters
        ----------
        timestamp : int
            Range end timestamp in the stream's time domain.
        range_id : int
            Identifier passed to the matching :meth:`write_start` call.

        Raises
        ------
        RuntimeError
            If the stream is not open or the backend is closed.
        WriterError
            If the backend fails to write the event.
        """
        if range_id == 0:
            raise ValueError("range_id must be non-zero")
        _check(
            nvtxwRangeEndWrite(self._get_writer(), timestamp, range_id),
            "nvtxwRangeEndWrite")
