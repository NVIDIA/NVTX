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
    BatchOrdering,
    CounterNoValueReason,
    EntryKind,
    EventKind,
    PayloadEntryType,
    PredefinedScope,
    TimestampType,
    numpy_dtype,
)
from nvtx.colors import color_to_hex
from nvtx._lib.lib import _normalize_counter_dtype
from nvtx._metadata import PayloadSchemaKey, _nvtx_metadata_from_dtype

from nvtx._lib.counters cimport (
    NVTX_COUNTER_ID_NONE,
    _fill_counter_semantics,
    nvtxSemanticsCounter_t,
)
from nvtx._lib.lib cimport (
    NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64,
    NVTX_PAYLOAD_ENTRY_TYPE_INT64,
)

try:
    import numpy as np
except ImportError:
    np = None

try:
    import pandas as pd
except ImportError:
    pd = None


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


def _validate_counter_layout(dtype):
    """
    Reject event entry roles in a counter layout; return the name of the
    embedded ``EntryKind.COUNTER_TIMESTAMP`` field, or ``None``.
    """
    if dtype is int or dtype is float:
        return None
    if dtype.subdtype is not None:
        raise TypeError(
            "Top-level fixed-size array counter dtypes are not supported."
        )
    _validate_native_byte_order(dtype, "a counter layout")
    if getattr(dtype, "fields", None) is None:
        return None
    timestamp_fields = []
    for field_name in dtype.names:
        metadata = _nvtx_metadata_from_dtype(dtype.fields[field_name][0])
        entry_kind = getattr(metadata, "entry_kind", None)
        if entry_kind is None:
            continue
        if entry_kind is EntryKind.COUNTER_TIMESTAMP:
            timestamp_fields.append(field_name)
        else:
            raise ValueError(
                f"EntryKind.{entry_kind.name} is not valid in a counter "
                f"layout (field {field_name!r})"
            )
    if len(timestamp_fields) > 1:
        raise ValueError(
            "a counter layout may embed at most one "
            "EntryKind.COUNTER_TIMESTAMP field"
        )
    return timestamp_fields[0] if timestamp_fields else None


def _batch_rows(data, dt):
    """Return batch data as a flat, contiguous array of ``dt`` rows."""
    if isinstance(data, np.ndarray):
        if data.ndim != 1:
            raise ValueError("batch data must be one-dimensional")
        return np.ascontiguousarray(data, dtype=dt)

    if pd is not None and isinstance(data, pd.DataFrame):
        rows = np.empty(len(data), dtype=dt)
        for name in dt.names:
            try:
                column = data[name]
            except KeyError:
                raise ValueError(
                    f"batch data is missing a column for dtype field {name!r}"
                )
            rows[name] = column.to_numpy(copy=False)
        return rows

    return np.fromiter(data, dtype=dt)


_COMPLETE_EVENT_KINDS = (
    EventKind.MARK,
    EventKind.RANGE_PUSHPOP,
    EventKind.RANGE_STARTEND,
)


def _validate_native_byte_order(dt, operation):
    subdtype = dt.subdtype
    if subdtype is not None:
        _validate_native_byte_order(subdtype[0], operation)
        return
    if dt.fields is not None:
        for field_name in dt.names:
            _validate_native_byte_order(dt.fields[field_name][0], operation)
        return
    if not dt.isnative:
        raise TypeError(
            f"{operation} requires native byte order; got dtype {dt}"
        )


def _validate_event_dtype(kind, dt, operation):
    """Validate the static layout and event roles of an event dtype."""
    if dt.fields is None:
        raise TypeError(f"{operation} requires a structured dtype")
    _validate_native_byte_order(dt, operation)

    role_fields = {
        entry_kind: []
        for entry_kind in (
            EntryKind.RANGE_BEGIN,
            EntryKind.RANGE_END,
            EntryKind.MARK,
            EntryKind.COUNTER_TIMESTAMP,
        )
    }
    range_id_fields = []

    for field_name in dt.names:
        field_dtype = dt.fields[field_name][0]
        if field_dtype.hasobject:
            raise TypeError(
                f"{operation} requires fields with self-contained, "
                f"fixed-width storage; field {field_name!r} uses managed storage"
            )

        metadata = _nvtx_metadata_from_dtype(field_dtype)
        entry_kind = getattr(metadata, "entry_kind", None)
        entry_type = getattr(metadata, "entry_type", None)
        if entry_type is PayloadEntryType.RANGE_ID:
            range_id_fields.append(field_name)
        if entry_kind in role_fields:
            if entry_type is not None:
                raise ValueError(
                    f"{operation} field {field_name!r} cannot combine the "
                    f"EntryKind.{entry_kind.name} timestamp role with "
                    f"PayloadEntryType.{entry_type.name}"
                )
            role_fields[entry_kind].append(field_name)

    if kind is EventKind.MARK:
        required_roles = (EntryKind.MARK,)
    elif kind in (EventKind.RANGE_PUSH, EventKind.RANGE_START):
        required_roles = (EntryKind.RANGE_BEGIN,)
    elif kind in (EventKind.RANGE_POP, EventKind.RANGE_END):
        required_roles = (EntryKind.RANGE_END,)
    else:
        required_roles = (EntryKind.RANGE_BEGIN, EntryKind.RANGE_END)

    for entry_kind, fields in role_fields.items():
        expected_count = 1 if entry_kind in required_roles else 0
        if len(fields) != expected_count:
            role = f"EntryKind.{entry_kind.name}"
            if expected_count:
                raise ValueError(
                    f"{operation} dtype for {kind!s} must contain exactly one "
                    f"{role} field"
                )
            raise ValueError(f"{role} is not valid for {kind!s}")

    expected_range_ids = (
        1 if kind in (EventKind.RANGE_START, EventKind.RANGE_END) else 0
    )
    if len(range_id_fields) != expected_range_ids:
        role = "PayloadEntryType.RANGE_ID"
        if expected_range_ids:
            raise ValueError(
                f"{operation} dtype for {kind!s} must contain exactly one "
                f"{role} field"
            )
        raise ValueError(f"{role} is not valid for {kind!s}")


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
        from the session (domains, streams, registered strings, scopes, and
        counters) become invalid when it ends.

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
    message. The numeric :attr:`handle` can also be stored in a payload field
    annotated with :attr:`nvtx.PayloadEntryType.REGISTERED_STRING`. Valid until
    the owning session ends.
    """

    def __cinit__(self, Domain domain, string, handle_addr):
        self._domain = domain
        self._string = string
        self._handle = \
            <nvtxStringHandle_t><void*><uintptr_t>handle_addr

    def __repr__(self):
        return f"RegisteredString({self._string!r})"

    @property
    def handle(self):
        """Opaque numeric handle for a registered-string payload field."""
        return <uintptr_t>self._handle


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


cdef class Counter:
    """
    Domain-owned counter handle. Created by :meth:`Domain.get_counter`.
    Pass to the :class:`Stream` counter write methods.
    Valid until the owning session ends.
    """

    def __cinit__(
        self,
        Domain domain,
        name,
        dtype,
        description,
        semantics,
        uint64_t scope_id,
        uint64_t counter_id,
        uint64_t schema_id,
        timestamp_field,
    ):
        self._domain = domain
        self._name = name
        self._dtype = dtype
        self._description = description
        self._semantics = semantics
        self._scope_id = scope_id
        self._counter_id = counter_id
        self._schema_id = schema_id
        self._timestamp_field = timestamp_field

    def __repr__(self):
        return f"Counter(name={self._name!r}, counter_id={self._counter_id})"

    @property
    def counter_id(self):
        """Registered counter ID within the domain."""
        return self._counter_id

    @property
    def name(self):
        """The counter's display name."""
        return self._name

    @property
    def dtype(self):
        """The normalized dtype the counter was registered with."""
        return self._dtype

    @property
    def description(self):
        """The counter's description, or ``None``."""
        return self._description

    @property
    def semantics(self):
        """The counter's :class:`nvtx.CounterSemantics`, or ``None``."""
        return self._semantics


cdef class Schema:
    """
    Domain-owned payload schema handle. Created by :meth:`Domain.get_schema`.
    Pass event schemas (created with a ``kind``) to
    :meth:`Stream.write_event` and :meth:`Stream.write_event_batch`.
    Valid until the owning session ends.
    """

    def __cinit__(self, Domain domain, uint64_t schema_id, dtype, kind):
        self._domain = domain
        self._schema_id = schema_id
        self._dtype = dtype
        self._kind = kind

    def __repr__(self):
        return (
            f"Schema(schema_id={self._schema_id}, dtype={self._dtype!r}, "
            f"kind={self._kind})"
        )

    @property
    def schema_id(self):
        """Registered schema ID within the domain."""
        return self._schema_id

    @property
    def dtype(self):
        """The NumPy dtype the schema was registered with."""
        return self._dtype

    @property
    def kind(self):
        """The schema's :class:`EventKind`, or ``None`` if generic."""
        return self._kind


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
        self._schemas = {}
        self._get_counter_cached = functools.cache(self._register_counter)
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

    def get_schema(self, dtype, *, kind=None):
        """
        Get or create a payload schema in this domain.

        Parameters
        ----------
        dtype : dtype-like
            NumPy dtype describing the payload layout. In an event schema,
            fields define their roles with :func:`nvtx.numpy_dtype` and its
            ``entry_kind`` argument, and may define specially interpreted
            integer fields with its ``entry_type`` argument.
        kind : EventKind, optional
            Event family described by the schema; required for use with
            :meth:`Stream.write_event` and :meth:`Stream.write_event_batch`.
            The ID-correlated :attr:`EventKind.RANGE_START` and
            :attr:`EventKind.RANGE_END` families require exactly one field
            annotated with :attr:`PayloadEntryType.RANGE_ID`. ``None`` creates
            a generic payload schema.

        Returns
        -------
        Schema
            The registered schema. Results are cached per domain, dtype,
            and kind.

        Raises
        ------
        RuntimeError
            If numpy is not installed or the domain is no longer valid.
        TypeError
            If ``dtype`` is not dtype-like, or ``kind`` is not an
            :class:`nvtx.EventKind` or ``None``. For event schemas, if the
            dtype is not structured, a field uses managed storage or a
            non-native byte order, or a timestamp role field is not a
            64-bit integer.
        ValueError
            If ``kind`` cannot be described by a dtype, or the dtype's
            timestamp roles do not match ``kind``.
        WriterError
            If the backend fails to register the schema.
        """
        cdef uint64_t schema_id
        if kind is not None and not isinstance(kind, EventKind):
            raise TypeError("kind must be an nvtx.EventKind or None")
        with self._session._lock:
            self._ensure_valid()
            # numpy_dtype normalizes the input and raises if numpy is missing.
            dt = numpy_dtype(dtype)
            schema_key = PayloadSchemaKey(
                dt, schema_flags=0 if kind is None else kind.value
            )
            schema = self._schemas.get(schema_key)
            if schema is None:
                if kind is not None:
                    _validate_event_dtype(kind, dt, "an event schema")
                schema_id = self._schema_registrar._get_numpy_dtype_schema(
                    schema_key
                )
                schema = Schema(self, schema_id, dt, kind)
                self._schemas[schema_key] = schema
            return schema

    def get_counter(
        self,
        name,
        dtype,
        *,
        description=None,
        scope=None,
        semantics=None,
    ):
        """
        Get or create a counter or counter group in this domain.

        Parameters
        ----------
        name : str or bytes
            Display name for the counter.
        dtype : int, float, or dtype-like
            Counter value type. ``int`` records signed 64-bit integer samples,
            ``float`` records double-precision samples, and a NumPy dtype-like
            object records samples with that dtype. Top-level fixed-size array
            dtypes are not supported. A structured dtype describes a flat
            counter group. Its fields may define per-field
            semantics with :func:`nvtx.numpy_dtype` and its
            ``counter_semantics`` argument, and at most one field may have the
            :attr:`nvtx.EntryKind.COUNTER_TIMESTAMP` role.
        description : str or bytes, optional
            Longer description for the counter.
        scope : PredefinedScope, Scope, int, or None, optional
            Scope associated with the counter. Pass a predefined scope, a
            dynamic scope returned by :meth:`get_scope`, or an integer in the
            NVTX static scope ID range.
        semantics : CounterSemantics, optional
            Semantics for the counter as a whole. For per-field semantics in a
            structured dtype, use :func:`nvtx.numpy_dtype`.

        Returns
        -------
        Counter
            The requested counter. Results are cached per domain.

        Raises
        ------
        RuntimeError
            If NumPy is required but not installed, or the domain is no
            longer valid.
        TypeError
            If ``dtype`` is not dtype-like, is a top-level fixed-size array,
            or contains nested or array fields, or if ``scope`` has an
            unsupported type.
        ValueError
            If the counter layout contains an unsupported field role, more
            than one embedded counter timestamp, or ``scope`` is registered
            in a different domain.
        WriterError
            If the backend fails to register the counter or its schema.
        """
        dtype = _normalize_counter_dtype(dtype)
        cdef uint64_t scope_id
        with self._session._lock:
            self._ensure_valid()
            scope_id = _resolve_scope_id(self, scope)
            schema_key = PayloadSchemaKey(
                dtype,
                counter_group=getattr(dtype, "fields", None) is not None,
            )
            return self._get_counter_cached(
                _as_str(name), schema_key, _as_str(description), scope_id,
                semantics,
            )

    def _register_counter(
        self, name, schema_key, description, uint64_t scope_id, semantics
    ):
        cdef nvtxCounterAttr_t attr
        cdef nvtxSemanticsCounter_t counter_semantics
        cdef uint64_t schema_id
        cdef uint64_t counter_id = 0
        cdef bytes name_bytes = _as_bytes(name)
        cdef bytes description_bytes
        cdef const char* description_ptr = NULL
        cdef nvtxwResultCode_t rc

        dtype = schema_key.dtype
        timestamp_field = _validate_counter_layout(dtype)
        if dtype is int:
            schema_id = NVTX_PAYLOAD_ENTRY_TYPE_INT64
        elif dtype is float:
            schema_id = NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64
        elif (
            dtype.fields is None
            and dtype.subdtype is None
            and dtype.kind == "i"
            and dtype.itemsize == 8
        ):
            schema_id = NVTX_PAYLOAD_ENTRY_TYPE_INT64
        elif (
            dtype.fields is None
            and dtype.subdtype is None
            and dtype.kind == "f"
            and dtype.itemsize == 8
        ):
            schema_id = NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64
        else:
            schema_id = self._schema_registrar._get_numpy_dtype_schema(
                schema_key
            )

        if description is not None:
            description_bytes = _as_bytes(description)
            description_ptr = description_bytes

        attr.structSize = sizeof(nvtxCounterAttr_t)
        attr.schemaId = schema_id
        attr.name = name_bytes
        attr.description = description_ptr
        attr.scopeId = scope_id
        attr.semantics = NULL
        if semantics is not None:
            _fill_counter_semantics(&counter_semantics, semantics)
            attr.semantics = &counter_semantics.header
        attr.counterId = NVTX_COUNTER_ID_NONE
        rc = self._backend._iface.CounterRegister(
            self._handle, &attr, &counter_id)
        _check(rc, "CounterRegister")

        return Counter(
            self, name, dtype, description, semantics,
            scope_id, counter_id, schema_id, timestamp_field,
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

    cdef void _ensure_counter(self, Counter counter) except *:
        if counter._domain is not self._domain:
            raise ValueError(
                "counter is not registered in this stream's domain")

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

    def write_event(self, Schema schema not None, row):
        """
        Write an event described by a role-annotated dtype schema.

        All event families are supported. The ID-correlated
        :attr:`EventKind.RANGE_START` and :attr:`EventKind.RANGE_END` schemas
        include a :attr:`PayloadEntryType.RANGE_ID` field.

        Parameters
        ----------
        schema : Schema
            Event schema returned by :meth:`Domain.get_schema` with a
            ``kind``, for this stream's domain.
        row : array-like
            Values for one row of the schema's dtype, such as a tuple or a
            NumPy scalar.

        Raises
        ------
        RuntimeError
            If the stream is not open or the backend is closed.
        TypeError
            If ``schema`` is not a :class:`Schema`.
        ValueError
            If ``schema`` belongs to another domain or is not an event
            schema, or ``row`` does not contain exactly one event.
        WriterError
            If the backend fails to write the event.

        Notes
        -----
        Each call converts ``row`` to a contiguous array. For high write
        rates with a complete-event schema, prefer :meth:`write_event_batch`.
        """
        if schema._domain is not self._domain:
            raise ValueError(
                "schema is not registered in this stream's domain")
        if schema._kind is None:
            raise ValueError(
                "write_event requires an event schema; pass kind= to "
                "Domain.get_schema"
            )
        self._ensure_writable()
        payload = np.ascontiguousarray(row, schema._dtype)
        if payload.size != 1:
            raise ValueError(
                f"write_event expects one row of dtype {schema._dtype}; "
                f"got {payload.size}"
            )

        cdef nvtxPayloadData_t data
        data.schemaId = schema._schema_id
        data.size = payload.nbytes
        data.payload = <const void*><size_t>payload.ctypes.data

        cdef nvtxwResultCode_t rc = self._backend._iface.EventWrite(
            self._writer.stream, &data, 1)
        _check(rc, "EventWrite")

    def write_event_batch(
        self, Schema schema not None, rows, *,
        ordering=BatchOrdering.SORTED,
    ):
        """
        Write a batch of complete events.

        Parameters
        ----------
        schema : Schema
            Complete-event schema returned by :meth:`Domain.get_schema`
            with a ``kind`` of :attr:`EventKind.MARK`,
            :attr:`EventKind.RANGE_PUSHPOP`, or
            :attr:`EventKind.RANGE_STARTEND`, for this stream's domain.
        rows : numpy.ndarray, pandas.DataFrame, or iterable
            Event rows. DataFrame columns are matched to fields by name.
        ordering : BatchOrdering, optional
            Timestamp ordering of the rows. The default is
            :attr:`BatchOrdering.SORTED`.

        Raises
        ------
        RuntimeError
            If the stream is not open or the backend is closed.
        TypeError
            If ``schema`` is not a :class:`Schema` or ``ordering`` is not a
            :class:`BatchOrdering`.
        ValueError
            If ``schema`` belongs to another domain or is not a
            complete-event schema, or ``rows`` contains no events.
        WriterError
            If the backend fails to write the batch.
        """
        if not isinstance(ordering, BatchOrdering):
            raise TypeError("ordering must be an nvtx.BatchOrdering")
        if schema._domain is not self._domain:
            raise ValueError(
                "schema is not registered in this stream's domain")
        if schema._kind not in _COMPLETE_EVENT_KINDS:
            raise ValueError(
                "write_event_batch accepts only complete-event schemas; "
                "pass kind=EventKind.MARK, EventKind.RANGE_PUSHPOP, or "
                "EventKind.RANGE_STARTEND to Domain.get_schema"
            )
        self._ensure_writable()
        events = _batch_rows(rows, schema._dtype)
        if events.size == 0:
            raise ValueError("batch rows must contain at least one event")

        cdef const void* event_data = <const void*><size_t>events.ctypes.data
        cdef nvtxEventBatch_t batch
        batch.eventSchemaId = schema._schema_id
        batch.size = events.nbytes
        batch.events = event_data
        batch.scope = self._scope_id
        batch.flags = ordering.value
        batch.flexData = NULL
        batch.flexDataSize = 0
        batch.flexDataOffset = 0
        cdef nvtxwResultCode_t rc = self._backend._iface.EventBatchWrite(
            self._writer.stream, &batch)
        _check(rc, "EventBatchWrite")

    def write_counter_sample(
        self, Counter counter not None, int64_t timestamp, value
    ):
        """
        Write a single counter sample.

        Parameters
        ----------
        counter : Counter
            Counter returned by :meth:`Domain.get_counter` for this stream's
            domain.
        timestamp : int
            Sample timestamp in the stream's time domain.
        value
            Sample value matching the counter's registered layout. For a
            counter group, this contains one row of field values.

        Raises
        ------
        RuntimeError
            If the stream is not open or the backend is closed.
        TypeError
            If ``counter`` is not a :class:`Counter`.
        ValueError
            If ``counter`` belongs to another domain, its layout embeds a
            counter timestamp, or ``value`` does not contain exactly one
            sample.
        WriterError
            If the backend fails to write the sample.

        Notes
        -----
        A structured sample is converted to a contiguous array on each call.
        For high write rates, prefer :meth:`write_counter_batch`.
        """
        self._ensure_counter(counter)
        self._ensure_writable()
        if counter._timestamp_field is not None:
            raise ValueError(
                "write_counter_sample does not support a counter layout that "
                "embeds an EntryKind.COUNTER_TIMESTAMP field; use "
                "write_counter_batch instead"
            )
        cdef const nvtxwInterface_v2_t* iface = self._backend._iface
        cdef int64_t i64
        cdef double f64
        cdef nvtxwResultCode_t rc
        if counter._schema_id == NVTX_PAYLOAD_ENTRY_TYPE_INT64:
            i64 = value
            rc = nvtxwCounterInt64Write(
                iface, self._writer.stream, timestamp,
                counter._counter_id, i64)
        elif counter._schema_id == NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64:
            f64 = value
            rc = nvtxwCounterFloat64Write(
                iface, self._writer.stream, timestamp,
                counter._counter_id, f64)
        else:
            payload = np.ascontiguousarray(value, counter._dtype)
            if payload.size != 1:
                raise ValueError(
                    f"write_counter_sample expects one sample of dtype "
                    f"{counter._dtype}; "
                    f"got {payload.size}"
                )
            rc = iface.CounterWrite(
                self._writer.stream, timestamp, counter._counter_id,
                <const void*><size_t>payload.ctypes.data, payload.nbytes)
        _check(rc, "CounterWrite")

    def write_counter_sample_no_value(
        self, Counter counter not None, int64_t timestamp, reason
    ):
        """
        Write a counter sample with no explicit value.

        Parameters
        ----------
        counter : Counter
            Counter returned by :meth:`Domain.get_counter` for this stream's
            domain.
        timestamp : int
            Sample timestamp in the stream's time domain.
        reason : CounterNoValueReason
            Reason the sample has no explicit value.

        Raises
        ------
        RuntimeError
            If the stream is not open or the backend is closed.
        TypeError
            If ``counter`` is not a :class:`Counter` or ``reason`` is not a
            :class:`nvtx.CounterNoValueReason`.
        ValueError
            If ``counter`` belongs to another domain.
        WriterError
            If the backend fails to write the sample.
        """
        self._ensure_counter(counter)
        if not isinstance(reason, CounterNoValueReason):
            raise TypeError("reason must be an nvtx.CounterNoValueReason")
        self._ensure_writable()
        _check(
            self._backend._iface.CounterNoValueWrite(
                self._writer.stream, timestamp, counter._counter_id,
                <uint8_t>reason.value),
            "CounterNoValueWrite")

    def write_counter_batch(
        self, Counter counter not None, data, *,
        timestamps=None, ordering=BatchOrdering.SORTED,
    ):
        """
        Write a batch of counter samples.

        Timestamps come from one of two sources, selected by the counter's
        registered layout:

        * **Separate**: ``timestamps`` is an iterable of per-sample
          timestamps, same length as ``data``, and ``data`` carries the
          values only.  Required unless the layout embeds a timestamp field.
        * **Embedded**: the counter-group layout has an
          ``entry_kind=nvtx.EntryKind.COUNTER_TIMESTAMP`` field and each row
          in ``data`` carries it; ``timestamps`` must be omitted.

        Parameters
        ----------
        counter : Counter
            Counter returned by :meth:`Domain.get_counter` for this stream's
            domain.
        data : numpy.ndarray, pandas.DataFrame, or iterable
            Sample rows. For scalar counters, this is an iterable of values.
            DataFrame columns are matched to counter fields by name.
        timestamps : iterable of int, optional
            Timestamp for each sample. Required unless the counter layout
            contains an embedded counter timestamp and otherwise must be
            omitted.
        ordering : BatchOrdering, optional
            Timestamp ordering of the samples. The default is
            :attr:`BatchOrdering.SORTED`.

        Raises
        ------
        RuntimeError
            If the stream is not open, the backend is closed, or NumPy is not
            installed.
        TypeError
            If ``counter`` is not a :class:`Counter` or ``ordering`` is not a
            :class:`BatchOrdering`.
        ValueError
            If ``counter`` belongs to another domain, the timestamp source is
            invalid, ``data`` is empty, ``data`` or ``timestamps`` is not
            one-dimensional, or their lengths differ.
        WriterError
            If the backend fails to write the batch.
        """
        if not isinstance(ordering, BatchOrdering):
            raise TypeError("ordering must be an nvtx.BatchOrdering")
        self._ensure_counter(counter)
        self._ensure_writable()
        if np is None:
            raise RuntimeError("Install numpy to use write_counter_batch.")
        dt = counter._dtype
        if counter._timestamp_field is not None:
            if timestamps is not None:
                raise ValueError(
                    "timestamps is mutually exclusive with a counter layout "
                    "that embeds an EntryKind.COUNTER_TIMESTAMP field"
                )
        elif timestamps is None:
            raise ValueError(
                "write_counter_batch requires timestamps unless the counter "
                "layout embeds an EntryKind.COUNTER_TIMESTAMP field"
            )

        if dt.fields is not None:
            samples = _batch_rows(data, dt)
        else:
            if isinstance(data, np.ndarray):
                samples = np.ascontiguousarray(data, dt)
            else:
                samples = np.fromiter(data, dtype=dt)
            if samples.ndim != 1:
                raise ValueError("batch data must be one-dimensional")
        if samples.size == 0:
            raise ValueError("batch data must contain at least one sample")

        cdef object timestamps_np = None
        cdef nvtxCounterBatch_t batch
        batch.counterId = counter._counter_id
        batch.counters = <const void*><size_t>samples.ctypes.data
        batch.countersSize = samples.nbytes
        batch.flags = ordering.value
        batch.timestamps = NULL
        batch.timestampsSize = 0
        if timestamps is not None:
            if isinstance(timestamps, np.ndarray):
                timestamps_np = np.ascontiguousarray(
                    timestamps, dtype=np.int64)
            else:
                timestamps_np = np.fromiter(timestamps, dtype=np.int64)
            if timestamps_np.ndim != 1:
                raise ValueError("timestamps must be one-dimensional")
            if timestamps_np.size != samples.size:
                raise ValueError(
                    "data and timestamps must have the same length")
            batch.timestamps = \
                <const int64_t*><size_t>timestamps_np.ctypes.data
            batch.timestampsSize = timestamps_np.nbytes
        _check(
            self._backend._iface.CounterBatchWrite(
                self._writer.stream, &batch),
            "CounterBatchWrite")
