# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception


import contextlib
import ctypes
import enum
import struct
from dataclasses import dataclass
from pathlib import Path
from types import SimpleNamespace
from typing import (
    Dict,
    Iterable,
    Iterator,
    Optional,
    Sequence,
    Tuple,
)

import pytest
import setuptools

import nvtx
import nvtx.writer

try:
    import numpy as np
except ImportError:
    np = None

EVENT_BUFFER_SIZE = 256
SCHEMA_ENTRY_BUFFER_COUNT = 32
SEMANTICS_BUFFER_COUNT = 4
TIMESTAMP_BUFFER_COUNT = 32

TESTS_DIR = Path(__file__).parent.parent
INCLUDE_DIR = TESTS_DIR.parent.parent / "c" / "include"
BACKEND_DIR = TESTS_DIR / "writer_backend"
BACKEND_MODULE_NAME = "nvtxw_test_backend"


def build_backend(build_dir: Path) -> str:
    """Compile the NVTXW test backend and return the path to the shared lib."""
    build_dir.mkdir(parents=True, exist_ok=True)
    extension = setuptools.Extension(
        BACKEND_MODULE_NAME,
        sources=[str(BACKEND_DIR / "NvtxWTestBackend.cpp")],
        include_dirs=[str(INCLUDE_DIR)],
        depends=[str(path) for path in INCLUDE_DIR.rglob("*.h")],
    )
    distribution = setuptools.Distribution(
        {
            "name": BACKEND_MODULE_NAME,
            "ext_modules": [extension],
        }
    )
    command = distribution.get_command_obj("build_ext")
    command.ensure_finalized()
    command.build_lib = str(build_dir / "lib")
    command.build_temp = str(build_dir / "temp")
    command.force = True
    command.inplace = False
    command.run()

    backend_path = Path(command.get_ext_fullpath(BACKEND_MODULE_NAME)).resolve()
    if not backend_path.is_file():
        raise RuntimeError(f"test backend was not built at {backend_path}")
    return str(backend_path)


class WriterEventKind(enum.Enum):
    # Must match WriterEventKind in NvtxWTestBackend.cpp.
    SESSION_BEGIN = 1
    SESSION_END = 2
    DOMAIN_REGISTER = 3
    STREAM_OPEN = 4
    STREAM_CLOSE = 5
    SCHEMA_REGISTER = 6
    EVENT_WRITE = 7
    STRING_REGISTER = 8
    SCOPE_REGISTER = 9
    CATEGORY_REGISTER = 10
    EVENT_BATCH_WRITE = 11
    COUNTER_REGISTER = 12
    COUNTER_WRITE = 13
    COUNTER_NO_VALUE_WRITE = 14
    COUNTER_BATCH_WRITE = 15


# IntEnum is intentional for constants that interoperate with raw C ABI
# integer values.
class WriterResultCode(enum.IntEnum):
    # Mirrors NVTXW_RESULT_* in nvtxw3.h.
    SUCCESS = 0
    FAILED = 1
    INVALID_ARGUMENT = 2
    LIBRARY_NOT_FOUND = 3
    LIBRARY_LOAD_FAILED = 4
    LIBRARY_SYMBOL_MISSING = 5
    INTERFACE_VERSION_NOT_SUPPORTED = 6
    NOT_SUPPORTED = 7


class BackendControls:
    """ctypes wrapper around the test backend's control hooks."""

    def __init__(self, backend_path: str) -> None:
        self._module = ctypes.CDLL(backend_path)
        self._module.writer_set_next_result.argtypes = [ctypes.c_int32]
        self._module.writer_set_next_result.restype = None
        self._module.writer_reset_next_result.argtypes = []
        self._module.writer_reset_next_result.restype = None
        self._module.writer_finalize_count.argtypes = []
        self._module.writer_finalize_count.restype = ctypes.c_uint64
        self._module.writer_reset_finalize_count.argtypes = []
        self._module.writer_reset_finalize_count.restype = None

    def set_next_result(self, result_code: WriterResultCode) -> None:
        self._module.writer_set_next_result(result_code)

    def reset_next_result(self) -> None:
        self._module.writer_reset_next_result()

    @property
    def finalize_count(self) -> int:
        return self._module.writer_finalize_count()

    def reset_finalize_count(self) -> None:
        self._module.writer_reset_finalize_count()


class _SemanticsBuffer(ctypes.Structure):
    _fields_ = [
        ("count", ctypes.c_uint64),
        ("ids", ctypes.c_uint16 * SEMANTICS_BUFFER_COUNT),
        ("versions", ctypes.c_uint16 * SEMANTICS_BUFFER_COUNT),
        ("struct_sizes", ctypes.c_uint32 * SEMANTICS_BUFFER_COUNT),
        ("has_counter", ctypes.c_uint8),
        ("has_time", ctypes.c_uint8),
        ("time_domain_id", ctypes.c_uint64),
        ("counter_flags", ctypes.c_uint64),
        ("counter_unit", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("counter_unit_scale_numerator", ctypes.c_uint64),
        ("counter_unit_scale_denominator", ctypes.c_uint64),
        ("counter_limit_type", ctypes.c_int64),
        ("counter_min_i64", ctypes.c_int64),
        ("counter_min_u64", ctypes.c_uint64),
        ("counter_min_f64", ctypes.c_double),
        ("counter_max_i64", ctypes.c_int64),
        ("counter_max_u64", ctypes.c_uint64),
        ("counter_max_f64", ctypes.c_double),
    ]


class _SchemaEntryBuffer(ctypes.Structure):
    _fields_ = [
        ("flags", ctypes.c_uint64),
        ("type", ctypes.c_uint64),
        ("name", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("description", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("array_or_union_detail", ctypes.c_uint64),
        ("offset", ctypes.c_uint64),
        ("semantics", _SemanticsBuffer),
        ("has_reserved", ctypes.c_uint8),
    ]


class _WriterEventBuffer(ctypes.Structure):
    # Fields must match WriterEventRecord in NvtxWTestBackend.cpp.
    _fields_ = [
        ("kind", ctypes.c_uint32),
        ("session_id", ctypes.c_uint64),
        ("domain_id", ctypes.c_uint64),
        ("stream_id", ctypes.c_uint64),
        ("session_name", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("session_config", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("domain_name", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("stream_name", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("stream_domain_id", ctypes.c_uint64),
        ("scope_id", ctypes.c_uint64),
        ("time_domain_id", ctypes.c_uint64),
        ("order_interleaving", ctypes.c_int16),
        ("ordering_type", ctypes.c_int16),
        ("ordering_skid", ctypes.c_int32),
        ("ordering_skid_amount", ctypes.c_int64),
        ("category_id", ctypes.c_uint32),
        ("category_name", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("string_value", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("string_handle", ctypes.c_uint64),
        ("scope_path", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("scope_parent", ctypes.c_uint64),
        ("scope_requested_id", ctypes.c_uint64),
        ("registered_scope_id", ctypes.c_uint64),
        ("schema_id", ctypes.c_uint64),
        ("schema_field_mask", ctypes.c_uint64),
        ("schema_name", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("schema_type", ctypes.c_uint64),
        ("schema_flags", ctypes.c_uint64),
        ("num_entries", ctypes.c_uint64),
        ("schema_static_size", ctypes.c_uint64),
        ("schema_pack_align", ctypes.c_uint64),
        ("schema_requested_id", ctypes.c_uint64),
        ("schema_has_extension", ctypes.c_uint8),
        (
            "schema_entries",
            _SchemaEntryBuffer * SCHEMA_ENTRY_BUFFER_COUNT,
        ),
        ("payload_count", ctypes.c_uint64),
        ("payload0_schema_id", ctypes.c_uint64),
        ("payload0_size", ctypes.c_uint64),
        ("payload0", ctypes.c_uint8 * EVENT_BUFFER_SIZE),
        ("payload1_schema_id", ctypes.c_uint64),
        ("payload1_size", ctypes.c_uint64),
        ("payload1", ctypes.c_uint8 * EVENT_BUFFER_SIZE),
        ("event_batch_schema_id", ctypes.c_uint64),
        ("event_batch_size", ctypes.c_uint64),
        ("event_batch_data", ctypes.c_uint8 * EVENT_BUFFER_SIZE),
        ("event_batch_scope", ctypes.c_uint64),
        ("event_batch_flags", ctypes.c_uint64),
        ("event_batch_flex_size", ctypes.c_uint64),
        ("event_batch_flex_offset", ctypes.c_uint64),
        ("event_batch_flex_data", ctypes.c_uint8 * EVENT_BUFFER_SIZE),
        ("counter_id", ctypes.c_uint64),
        ("counter_requested_id", ctypes.c_uint64),
        ("counter_schema_id", ctypes.c_uint64),
        ("counter_name", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("counter_description", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("counter_scope_id", ctypes.c_uint64),
        ("counter_semantics", _SemanticsBuffer),
        ("counter_timestamp", ctypes.c_int64),
        ("counter_no_value_reason", ctypes.c_uint8),
        ("counter_data_size", ctypes.c_uint64),
        ("counter_data", ctypes.c_uint8 * EVENT_BUFFER_SIZE),
        ("counter_batch_flags", ctypes.c_uint64),
        ("counter_timestamps_size", ctypes.c_uint64),
        (
            "counter_timestamps",
            ctypes.c_int64 * TIMESTAMP_BUFFER_COUNT,
        ),
    ]


@dataclass(frozen=True)
class RecordedSemantics:
    ids: Tuple[int, ...]
    versions: Tuple[int, ...]
    struct_sizes: Tuple[int, ...]
    has_counter: bool
    has_time: bool
    time_domain_id: int
    counter_flags: int
    counter_unit: str
    counter_unit_scale_numerator: int
    counter_unit_scale_denominator: int
    counter_limit_type: int
    counter_min_i64: int
    counter_min_u64: int
    counter_min_f64: float
    counter_max_i64: int
    counter_max_u64: int
    counter_max_f64: float


@dataclass(frozen=True)
class RecordedSchemaEntry:
    flags: int
    type: int
    name: str
    description: str
    array_or_union_detail: int
    offset: int
    semantics: RecordedSemantics
    has_reserved: bool


@dataclass(frozen=True)
class RecordedWriterEvent:
    kind: WriterEventKind
    session_name: str
    session_config: Optional[str]
    session_id: int
    domain_name: str
    domain_id: int
    stream_name: str
    stream_id: int
    stream_domain_id: int
    scope_id: int
    time_domain_id: int
    order_interleaving: int
    ordering_type: int
    ordering_skid: int
    ordering_skid_amount: int
    category_id: int
    category_name: str
    string_value: str
    string_handle: int
    scope_path: str
    scope_parent: int
    scope_requested_id: int
    registered_scope_id: int
    schema_id: int
    schema_field_mask: int
    schema_name: str
    schema_type: int
    schema_flags: int
    num_entries: int
    schema_static_size: int
    schema_pack_align: int
    schema_requested_id: int
    schema_has_extension: bool
    schema_entries: Tuple[RecordedSchemaEntry, ...]
    payload_count: int
    payload0_schema_id: int
    payload0_size: int
    payload0: bytes
    payload1_schema_id: int
    payload1_size: int
    payload1: bytes
    event_batch_schema_id: int
    event_batch_size: int
    event_batch_data: bytes
    event_batch_scope: int
    event_batch_flags: int
    event_batch_flex_size: int
    event_batch_flex_offset: int
    event_batch_flex_data: bytes
    counter_id: int
    counter_requested_id: int
    counter_schema_id: int
    counter_name: str
    counter_description: str
    counter_scope_id: int
    counter_semantics: RecordedSemantics
    counter_timestamp: int
    counter_no_value_reason: int
    counter_data_size: int
    counter_data: bytes
    counter_batch_flags: int
    counter_timestamps_size: int
    counter_timestamps: Tuple[int, ...]


def _read_bytes(value: Sequence[int], size: int, label: str) -> bytes:
    if size > len(value):
        raise RuntimeError(
            f"{label} has size {size}, but the recorder stores "
            f"at most {len(value)} bytes"
        )
    return bytes(value[:size])


def _read_semantics(value: _SemanticsBuffer) -> RecordedSemantics:
    if value.count > SEMANTICS_BUFFER_COUNT:
        raise RuntimeError(
            f"semantics chain has {value.count} entries, but the recorder "
            f"stores at most {SEMANTICS_BUFFER_COUNT}"
        )
    count = value.count
    return RecordedSemantics(
        ids=tuple(value.ids[:count]),
        versions=tuple(value.versions[:count]),
        struct_sizes=tuple(value.struct_sizes[:count]),
        has_counter=bool(value.has_counter),
        has_time=bool(value.has_time),
        time_domain_id=value.time_domain_id,
        counter_flags=value.counter_flags,
        counter_unit=value.counter_unit.decode(),
        counter_unit_scale_numerator=value.counter_unit_scale_numerator,
        counter_unit_scale_denominator=value.counter_unit_scale_denominator,
        counter_limit_type=value.counter_limit_type,
        counter_min_i64=value.counter_min_i64,
        counter_min_u64=value.counter_min_u64,
        counter_min_f64=value.counter_min_f64,
        counter_max_i64=value.counter_max_i64,
        counter_max_u64=value.counter_max_u64,
        counter_max_f64=value.counter_max_f64,
    )


def _read_schema_entry(value: _SchemaEntryBuffer) -> RecordedSchemaEntry:
    return RecordedSchemaEntry(
        flags=value.flags,
        type=value.type,
        name=value.name.decode(),
        description=value.description.decode(),
        array_or_union_detail=value.array_or_union_detail,
        offset=value.offset,
        semantics=_read_semantics(value.semantics),
        has_reserved=bool(value.has_reserved),
    )


class NvtxwEventsReader:
    def __init__(self, backend_path: str) -> None:
        self._module = ctypes.CDLL(backend_path)
        self._module.writer_event_record_size.argtypes = []
        self._module.writer_event_record_size.restype = ctypes.c_size_t

        c_size = self._module.writer_event_record_size()
        python_size = ctypes.sizeof(_WriterEventBuffer)
        if c_size != python_size:
            raise RuntimeError(
                "writer event record ABI size mismatch: "
                f"C++ backend={c_size}, Python reader={python_size}"
            )

        self._module.read_event.argtypes = [
            ctypes.POINTER(_WriterEventBuffer),
            ctypes.c_size_t,
        ]
        self._module.read_event.restype = ctypes.c_bool

    def __iter__(self) -> Iterator[RecordedWriterEvent]:
        return self

    def __next__(self) -> RecordedWriterEvent:
        buffer = _WriterEventBuffer()
        if not self._module.read_event(
            ctypes.byref(buffer),
            ctypes.sizeof(buffer),
        ):
            raise StopIteration
        if buffer.num_entries > SCHEMA_ENTRY_BUFFER_COUNT:
            raise RuntimeError(
                f"schema has {buffer.num_entries} entries, but the recorder "
                f"stores at most {SCHEMA_ENTRY_BUFFER_COUNT}"
            )
        if buffer.counter_timestamps_size > TIMESTAMP_BUFFER_COUNT * ctypes.sizeof(
            ctypes.c_int64
        ) or buffer.counter_timestamps_size % ctypes.sizeof(ctypes.c_int64):
            raise RuntimeError(
                "recorded counter timestamp size is invalid: "
                f"{buffer.counter_timestamps_size}"
            )

        return RecordedWriterEvent(
            kind=WriterEventKind(buffer.kind),
            session_name=buffer.session_name.decode(),
            session_config=buffer.session_config.decode(),
            session_id=buffer.session_id,
            domain_name=buffer.domain_name.decode(),
            domain_id=buffer.domain_id,
            stream_name=buffer.stream_name.decode(),
            stream_id=buffer.stream_id,
            stream_domain_id=buffer.stream_domain_id,
            scope_id=buffer.scope_id,
            time_domain_id=buffer.time_domain_id,
            order_interleaving=buffer.order_interleaving,
            ordering_type=buffer.ordering_type,
            ordering_skid=buffer.ordering_skid,
            ordering_skid_amount=buffer.ordering_skid_amount,
            category_id=buffer.category_id,
            category_name=buffer.category_name.decode(),
            string_value=buffer.string_value.decode(),
            string_handle=buffer.string_handle,
            scope_path=buffer.scope_path.decode(),
            scope_parent=buffer.scope_parent,
            scope_requested_id=buffer.scope_requested_id,
            registered_scope_id=buffer.registered_scope_id,
            schema_id=buffer.schema_id,
            schema_field_mask=buffer.schema_field_mask,
            schema_name=buffer.schema_name.decode(),
            schema_type=buffer.schema_type,
            schema_flags=buffer.schema_flags,
            num_entries=buffer.num_entries,
            schema_static_size=buffer.schema_static_size,
            schema_pack_align=buffer.schema_pack_align,
            schema_requested_id=buffer.schema_requested_id,
            schema_has_extension=bool(buffer.schema_has_extension),
            schema_entries=tuple(
                _read_schema_entry(buffer.schema_entries[i])
                for i in range(buffer.num_entries)
            ),
            payload_count=buffer.payload_count,
            payload0_schema_id=buffer.payload0_schema_id,
            payload0_size=buffer.payload0_size,
            payload0=_read_bytes(buffer.payload0, buffer.payload0_size, "payload 0"),
            payload1_schema_id=buffer.payload1_schema_id,
            payload1_size=buffer.payload1_size,
            payload1=_read_bytes(buffer.payload1, buffer.payload1_size, "payload 1"),
            event_batch_schema_id=buffer.event_batch_schema_id,
            event_batch_size=buffer.event_batch_size,
            event_batch_data=_read_bytes(
                buffer.event_batch_data,
                buffer.event_batch_size,
                "event batch",
            ),
            event_batch_scope=buffer.event_batch_scope,
            event_batch_flags=buffer.event_batch_flags,
            event_batch_flex_size=buffer.event_batch_flex_size,
            event_batch_flex_offset=buffer.event_batch_flex_offset,
            event_batch_flex_data=_read_bytes(
                buffer.event_batch_flex_data,
                buffer.event_batch_flex_size,
                "event batch flexible data",
            ),
            counter_id=buffer.counter_id,
            counter_requested_id=buffer.counter_requested_id,
            counter_schema_id=buffer.counter_schema_id,
            counter_name=buffer.counter_name.decode(),
            counter_description=buffer.counter_description.decode(),
            counter_scope_id=buffer.counter_scope_id,
            counter_semantics=_read_semantics(buffer.counter_semantics),
            counter_timestamp=buffer.counter_timestamp,
            counter_no_value_reason=buffer.counter_no_value_reason,
            counter_data_size=buffer.counter_data_size,
            counter_data=_read_bytes(
                buffer.counter_data,
                buffer.counter_data_size,
                "counter data",
            ),
            counter_batch_flags=buffer.counter_batch_flags,
            counter_timestamps_size=buffer.counter_timestamps_size,
            counter_timestamps=tuple(
                buffer.counter_timestamps[
                    : buffer.counter_timestamps_size // ctypes.sizeof(ctypes.c_int64)
                ]
            ),
        )


@pytest.fixture(scope="session")
def backend_path(tmp_path_factory: pytest.TempPathFactory) -> str:
    build_dir = tmp_path_factory.mktemp("nvtxw_test_backend")
    return build_backend(build_dir)


@pytest.fixture(scope="session")
def nvtxw_events(backend_path: str) -> NvtxwEventsReader:
    """Drain calls from the same loaded library used by the backend fixture."""
    return NvtxwEventsReader(backend_path)


@pytest.fixture
def backend_controls(backend_path: str) -> Iterator[BackendControls]:
    controls = BackendControls(backend_path)
    controls.reset_next_result()
    controls.reset_finalize_count()
    try:
        yield controls
    finally:
        controls.reset_next_result()


@pytest.fixture
def backend(
    backend_path: str,
) -> Iterator[nvtx.writer.Backend]:
    backend = nvtx.writer.load_backend(backend_path)
    yield backend
    backend.close()


@pytest.fixture(autouse=True)
def verify_all_writer_events_consumed(
    nvtxw_events: NvtxwEventsReader,
):
    """Assert no recorded backend call slips through unverified."""
    yield
    assert not list(nvtxw_events)


def assert_failed(excinfo, operation: str, result_code: WriterResultCode) -> None:
    assert excinfo.value.result_code == result_code
    assert excinfo.value.operation == operation


def verify_session_begin(
    events: Iterator[RecordedWriterEvent],
    name: Optional[str],
    config: Optional[str] = None,
) -> int:
    event = next(events)
    assert event.kind == WriterEventKind.SESSION_BEGIN
    assert event.session_name == (name or "")
    assert event.session_config == (config or "")
    return event.session_id


def verify_session_end(
    events: Iterator[RecordedWriterEvent], session_id: Optional[int] = None
) -> int:
    event = next(events)
    assert event.kind == WriterEventKind.SESSION_END
    if session_id is not None:
        assert event.session_id == session_id
    return event.session_id


def verify_domain_register(
    events: Iterator[RecordedWriterEvent],
    name: Optional[str] = None,
    session_id: Optional[int] = None,
) -> int:
    event = next(events)
    assert event.kind == WriterEventKind.DOMAIN_REGISTER
    assert event.domain_name == (name or "")
    if session_id is not None:
        assert event.session_id == session_id
    assert event.domain_id != 0
    return event.domain_id


def verify_stream_open(
    events: Iterator[RecordedWriterEvent],
    name: Optional[str],
    domain_id: int = 0,
    scope_id: int = 0,
    time_domain_id: int = 0,
    interleaving: int = 0,
    ordering: int = 0,
    skid: int = 0,
    skid_amount: int = 0,
    session_id: Optional[int] = None,
) -> int:
    event = next(events)
    assert event.kind == WriterEventKind.STREAM_OPEN
    assert event.stream_name == (name or "")
    assert event.stream_domain_id == domain_id
    assert event.domain_id == domain_id
    assert event.scope_id == scope_id
    assert event.time_domain_id == time_domain_id
    assert event.order_interleaving == interleaving
    assert event.ordering_type == ordering
    assert event.ordering_skid == skid
    assert event.ordering_skid_amount == skid_amount
    if session_id is not None:
        assert event.session_id == session_id
    return event.stream_id


def verify_stream_close(
    events: Iterator[RecordedWriterEvent], stream_id: Optional[int] = None
) -> int:
    event = next(events)
    assert event.kind == WriterEventKind.STREAM_CLOSE
    if stream_id is not None:
        assert event.stream_id == stream_id
    return event.stream_id


def verify_category_register(
    events: Iterator[RecordedWriterEvent],
    name: str,
    category_id: Optional[int] = None,
    domain_id: Optional[int] = None,
) -> int:
    event = next(events)
    assert event.kind == WriterEventKind.CATEGORY_REGISTER
    assert event.category_name == name
    if category_id is not None:
        assert event.category_id == category_id
    if domain_id is not None:
        assert event.domain_id == domain_id
    return event.category_id


def verify_string_register(
    events: Iterator[RecordedWriterEvent],
    string: str,
    domain_id: Optional[int] = None,
) -> int:
    event = next(events)
    assert event.kind == WriterEventKind.STRING_REGISTER
    assert event.string_value == string
    if domain_id is not None:
        assert event.domain_id == domain_id
    return event.string_handle


# The backend's default parent scope value (NVTX_SCOPE_ROOT) for get_scope.
SCOPE_ROOT = 1


def verify_scope_register(
    events: Iterator[RecordedWriterEvent],
    path: Optional[str],
    parent: int = SCOPE_ROOT,
    domain_id: Optional[int] = None,
) -> int:
    event = next(events)
    assert event.kind == WriterEventKind.SCOPE_REGISTER
    assert event.scope_path == (path or "")
    assert event.scope_parent == parent
    assert event.scope_requested_id == 0
    if domain_id is not None:
        assert event.domain_id == domain_id
    return event.registered_scope_id


def verify_schema_register(
    events: Iterator[RecordedWriterEvent],
    *,
    num_entries: Optional[int] = None,
    schema_flags: Optional[int] = None,
    domain_id: Optional[int] = None,
    schema_type: Optional[int] = None,
    static_size: Optional[int] = None,
    entries: Optional[Iterable["ExpectedSchemaEntry"]] = None,
) -> int:
    event = next(events)
    assert event.kind == WriterEventKind.SCHEMA_REGISTER
    assert event.schema_requested_id == 0
    if num_entries is not None:
        assert event.num_entries == num_entries
    if schema_flags is not None:
        assert event.schema_flags == schema_flags
    if domain_id is not None:
        assert event.domain_id == domain_id
    if schema_type is not None:
        assert event.schema_type == schema_type
    if static_size is not None:
        assert event.schema_static_size == static_size
    if entries is not None:
        assert _recorded_schema_entries(event) == tuple(entries)
    assert all(not entry.has_reserved for entry in event.schema_entries)
    return event.schema_id


# These independent mirrors of C constants are IntEnums so tests can compare
# and combine them directly with raw values captured from the backend.
class StreamInterleaving(enum.IntEnum):
    SCOPE = 1


class StreamOrdering(enum.IntEnum):
    STRICT = 1


class StreamSkid(enum.IntEnum):
    TIME_NS = 1


class TimestampType(enum.IntEnum):
    CPU_TSC = 10


class CounterFlag(enum.IntEnum):
    LIMIT_MIN = 1 << 2
    LIMIT_MAX = 1 << 3
    VALUE_TYPE_DELTA = 2 << 4
    INTERPOLATION_LINEAR = 4 << 8


class CounterLimitType(enum.IntEnum):
    I64 = 1
    U64 = 2
    F64 = 3


class CounterNoValueReason(enum.IntEnum):
    UNAVAILABLE = 2


class BatchFlag(enum.IntEnum):
    SORTED = 0
    PARTIALLY_SORTED = 1 << 1
    SORTED_PER_SCOPE = 2 << 1
    UNSORTED = 3 << 1


class PayloadEntryFlag(enum.IntEnum):
    NONE = 0
    ARRAY_ZERO_TERMINATED = 2 << 4
    MESSAGE = 1 << 10
    TIMESTAMP = 2 << 10
    RANGE_BEGIN = 1 << 12
    RANGE_END = 2 << 12
    MARK = 3 << 12
    COUNTER = 4 << 12


class PayloadEntryType(enum.IntEnum):
    UINT32 = 16
    INT64 = 17
    UINT64 = 18
    FLOAT64 = 44
    RANGE_ID = 67
    CATEGORY = 68
    COLOR = 69
    CSTRING_UTF8 = 76
    REGISTERED_STRING = 80


class PayloadSchemaType(enum.IntEnum):
    STATIC = 1
    DYNAMIC = 2


class PayloadSchemaFlag(enum.IntEnum):
    COUNTER_GROUP = 1 << 3
    RANGE_PUSHPOP = 2 << 3
    RANGE_STARTEND = 3 << 3
    MARK = 4 << 3
    RANGE_PUSH = 5 << 3
    RANGE_POP = 6 << 3
    RANGE_START = 7 << 3
    RANGE_END = 8 << 3


@dataclass(frozen=True)
class ExpectedSchemaEntry:
    name: str
    entry_type: int
    flags: int = 0
    offset: int = 0


def expected_timestamp_entry(
    name: str, role: PayloadEntryFlag = PayloadEntryFlag.NONE, offset: int = 0
) -> ExpectedSchemaEntry:
    """Build an expected timestamp entry with an optional event role."""
    return ExpectedSchemaEntry(
        name=name,
        entry_type=PayloadEntryType.INT64,
        flags=PayloadEntryFlag.TIMESTAMP | role,
        offset=offset,
    )


@dataclass(frozen=True)
class ExpectedEventSchema:
    name: str
    flags: int
    schema_type: int
    static_size: int
    entries: Tuple[ExpectedSchemaEntry, ...]


def _recorded_schema_entries(
    event: RecordedWriterEvent,
) -> Tuple[ExpectedSchemaEntry, ...]:
    return tuple(
        ExpectedSchemaEntry(
            name=entry.name,
            entry_type=entry.type,
            flags=entry.flags,
            offset=entry.offset,
        )
        for entry in event.schema_entries
    )


_TIMESTAMP_MARK = ExpectedSchemaEntry(
    name="timestamp",
    entry_type=PayloadEntryType.INT64,
    flags=PayloadEntryFlag.TIMESTAMP | PayloadEntryFlag.MARK,
)
_TIMESTAMP_BEGIN = ExpectedSchemaEntry(
    name="timestampBegin",
    entry_type=PayloadEntryType.INT64,
    flags=PayloadEntryFlag.TIMESTAMP | PayloadEntryFlag.RANGE_BEGIN,
)
_TIMESTAMP_END = ExpectedSchemaEntry(
    name="timestampEnd",
    entry_type=PayloadEntryType.INT64,
    flags=PayloadEntryFlag.TIMESTAMP | PayloadEntryFlag.RANGE_END,
    offset=8,
)
_PUSH_TIMESTAMP = ExpectedSchemaEntry(
    name="timestamp",
    entry_type=PayloadEntryType.INT64,
    flags=PayloadEntryFlag.TIMESTAMP | PayloadEntryFlag.RANGE_BEGIN,
)
_START_TIMESTAMP = ExpectedSchemaEntry(
    name="timestamp",
    entry_type=PayloadEntryType.INT64,
    flags=PayloadEntryFlag.TIMESTAMP | PayloadEntryFlag.RANGE_BEGIN,
    offset=8,
)
_POP_TIMESTAMP = ExpectedSchemaEntry(
    name="timestamp",
    entry_type=PayloadEntryType.INT64,
    flags=PayloadEntryFlag.TIMESTAMP | PayloadEntryFlag.RANGE_END,
)
_END_TIMESTAMP = ExpectedSchemaEntry(
    name="timestamp",
    entry_type=PayloadEntryType.INT64,
    flags=PayloadEntryFlag.TIMESTAMP | PayloadEntryFlag.RANGE_END,
    offset=8,
)
_COLOR_AT_8 = ExpectedSchemaEntry(
    name="color", entry_type=PayloadEntryType.COLOR, offset=8
)
_CATEGORY_AT_12 = ExpectedSchemaEntry(
    name="category", entry_type=PayloadEntryType.CATEGORY, offset=12
)
_COLOR_AT_16 = ExpectedSchemaEntry(
    name="color", entry_type=PayloadEntryType.COLOR, offset=16
)
_CATEGORY_AT_20 = ExpectedSchemaEntry(
    name="category", entry_type=PayloadEntryType.CATEGORY, offset=20
)
_RANGE_ID = ExpectedSchemaEntry(name="rangeId", entry_type=PayloadEntryType.RANGE_ID)
_REG_MESSAGE_AT_16 = ExpectedSchemaEntry(
    name="message",
    entry_type=PayloadEntryType.REGISTERED_STRING,
    flags=PayloadEntryFlag.MESSAGE,
    offset=16,
)
_REG_MESSAGE_AT_24 = ExpectedSchemaEntry(
    name="message",
    entry_type=PayloadEntryType.REGISTERED_STRING,
    flags=PayloadEntryFlag.MESSAGE,
    offset=24,
)


_EVENT_SCHEMAS = (
    ExpectedEventSchema(
        name="mark_utf8",
        flags=PayloadSchemaFlag.MARK,
        schema_type=PayloadSchemaType.STATIC,
        static_size=16,
        entries=(_TIMESTAMP_MARK, _COLOR_AT_8, _CATEGORY_AT_12),
    ),
    ExpectedEventSchema(
        name="mark_reg_string",
        flags=PayloadSchemaFlag.MARK,
        schema_type=PayloadSchemaType.STATIC,
        static_size=24,
        entries=(
            _TIMESTAMP_MARK,
            _COLOR_AT_8,
            _CATEGORY_AT_12,
            _REG_MESSAGE_AT_16,
        ),
    ),
    ExpectedEventSchema(
        name="range_pushpop_utf8",
        flags=PayloadSchemaFlag.RANGE_PUSHPOP,
        schema_type=PayloadSchemaType.STATIC,
        static_size=24,
        entries=(
            _TIMESTAMP_BEGIN,
            _TIMESTAMP_END,
            _COLOR_AT_16,
            _CATEGORY_AT_20,
        ),
    ),
    ExpectedEventSchema(
        name="range_pushpop_reg_string",
        flags=PayloadSchemaFlag.RANGE_PUSHPOP,
        schema_type=PayloadSchemaType.STATIC,
        static_size=32,
        entries=(
            _TIMESTAMP_BEGIN,
            _TIMESTAMP_END,
            _COLOR_AT_16,
            _CATEGORY_AT_20,
            _REG_MESSAGE_AT_24,
        ),
    ),
    ExpectedEventSchema(
        name="range_startend_utf8",
        flags=PayloadSchemaFlag.RANGE_STARTEND,
        schema_type=PayloadSchemaType.STATIC,
        static_size=24,
        entries=(
            _TIMESTAMP_BEGIN,
            _TIMESTAMP_END,
            _COLOR_AT_16,
            _CATEGORY_AT_20,
        ),
    ),
    ExpectedEventSchema(
        name="range_startend_reg_string",
        flags=PayloadSchemaFlag.RANGE_STARTEND,
        schema_type=PayloadSchemaType.STATIC,
        static_size=32,
        entries=(
            _TIMESTAMP_BEGIN,
            _TIMESTAMP_END,
            _COLOR_AT_16,
            _CATEGORY_AT_20,
            _REG_MESSAGE_AT_24,
        ),
    ),
    ExpectedEventSchema(
        name="range_push_utf8",
        flags=PayloadSchemaFlag.RANGE_PUSH,
        schema_type=PayloadSchemaType.STATIC,
        static_size=16,
        entries=(_PUSH_TIMESTAMP, _COLOR_AT_8, _CATEGORY_AT_12),
    ),
    ExpectedEventSchema(
        name="range_push_reg_string",
        flags=PayloadSchemaFlag.RANGE_PUSH,
        schema_type=PayloadSchemaType.STATIC,
        static_size=24,
        entries=(
            _PUSH_TIMESTAMP,
            _COLOR_AT_8,
            _CATEGORY_AT_12,
            _REG_MESSAGE_AT_16,
        ),
    ),
    ExpectedEventSchema(
        name="range_pop",
        flags=PayloadSchemaFlag.RANGE_POP,
        schema_type=PayloadSchemaType.STATIC,
        static_size=8,
        entries=(_POP_TIMESTAMP,),
    ),
    ExpectedEventSchema(
        name="range_start_utf8",
        flags=PayloadSchemaFlag.RANGE_START,
        schema_type=PayloadSchemaType.STATIC,
        static_size=24,
        entries=(
            _RANGE_ID,
            _START_TIMESTAMP,
            _COLOR_AT_16,
            _CATEGORY_AT_20,
        ),
    ),
    ExpectedEventSchema(
        name="range_start_reg_string",
        flags=PayloadSchemaFlag.RANGE_START,
        schema_type=PayloadSchemaType.STATIC,
        static_size=32,
        entries=(
            _RANGE_ID,
            _START_TIMESTAMP,
            _COLOR_AT_16,
            _CATEGORY_AT_20,
            _REG_MESSAGE_AT_24,
        ),
    ),
    ExpectedEventSchema(
        name="range_end",
        flags=PayloadSchemaFlag.RANGE_END,
        schema_type=PayloadSchemaType.STATIC,
        static_size=16,
        entries=(_RANGE_ID, _END_TIMESTAMP),
    ),
    ExpectedEventSchema(
        name="event_message_utf8",
        flags=0,
        schema_type=PayloadSchemaType.DYNAMIC,
        static_size=0,
        entries=(
            ExpectedSchemaEntry(
                name="message",
                entry_type=PayloadEntryType.CSTRING_UTF8,
                flags=(
                    PayloadEntryFlag.MESSAGE | PayloadEntryFlag.ARRAY_ZERO_TERMINATED
                ),
            ),
        ),
    ),
)

EVENT_SCHEMA_ORDER = tuple(schema.name for schema in _EVENT_SCHEMAS)


def verify_event_schemas_registered(
    events: Iterator[RecordedWriterEvent],
) -> Dict[str, int]:
    """Verify every fixed helper schema and return its assigned ID by name."""
    schemas = {}
    for schema in _EVENT_SCHEMAS:
        event = next(events)
        assert event.kind == WriterEventKind.SCHEMA_REGISTER
        assert event.schema_field_mask == 124
        assert event.schema_name == ""
        assert event.schema_flags == schema.flags
        assert event.schema_type == schema.schema_type
        assert event.schema_static_size == schema.static_size
        assert event.schema_pack_align == 0
        assert event.schema_requested_id == 0
        assert not event.schema_has_extension
        assert event.num_entries == len(schema.entries)
        assert _recorded_schema_entries(event) == schema.entries
        assert all(
            entry.description == ""
            and entry.array_or_union_detail == 0
            and not entry.semantics.ids
            and not entry.has_reserved
            for entry in event.schema_entries
        )
        schemas[schema.name] = event.schema_id
    return schemas


def verify_event_write(
    events: Iterator[RecordedWriterEvent],
    schema_id: int,
    message: Optional[bytes] = None,
    message_schema_id: Optional[int] = None,
    stream_id: Optional[int] = None,
) -> bytes:
    """Verify EventWrite and return its first payload's raw bytes."""
    event = next(events)
    assert event.kind == WriterEventKind.EVENT_WRITE
    assert event.payload0_schema_id == schema_id
    if stream_id is not None:
        assert event.stream_id == stream_id
    if message is None:
        assert event.payload_count == 1
    else:
        assert event.payload_count == 2
        assert event.payload1_schema_id == message_schema_id
        assert event.payload1 == message
    return event.payload0


def verify_event_batch_write(
    events: Iterator[RecordedWriterEvent],
    schema_id: int,
    *,
    stream_id: Optional[int] = None,
    scope_id: Optional[int] = None,
    flags: Optional[int] = None,
) -> bytes:
    event = next(events)
    assert event.kind == WriterEventKind.EVENT_BATCH_WRITE
    assert event.event_batch_schema_id == schema_id
    if stream_id is not None:
        assert event.stream_id == stream_id
    if scope_id is not None:
        assert event.event_batch_scope == scope_id
    if flags is not None:
        assert event.event_batch_flags == flags
    assert event.event_batch_flex_size == 0
    assert event.event_batch_flex_offset == 0
    return event.event_batch_data


def verify_counter_register(
    events: Iterator[RecordedWriterEvent],
    *,
    name: str,
    schema_id: Optional[int] = None,
    description: Optional[str] = None,
    scope_id: Optional[int] = None,
    domain_id: Optional[int] = None,
) -> int:
    event = next(events)
    assert event.kind == WriterEventKind.COUNTER_REGISTER
    assert event.counter_name == name
    assert event.counter_description == (description or "")
    assert event.counter_requested_id == 0
    if schema_id is not None:
        assert event.counter_schema_id == schema_id
    if scope_id is not None:
        assert event.counter_scope_id == scope_id
    if domain_id is not None:
        assert event.domain_id == domain_id
    return event.counter_id


def verify_counter_write(
    events: Iterator[RecordedWriterEvent],
    counter_id: int,
    timestamp: int,
    *,
    stream_id: Optional[int] = None,
) -> bytes:
    event = next(events)
    assert event.kind == WriterEventKind.COUNTER_WRITE
    assert event.counter_id == counter_id
    assert event.counter_timestamp == timestamp
    if stream_id is not None:
        assert event.stream_id == stream_id
    return event.counter_data


def verify_counter_no_value_write(
    events: Iterator[RecordedWriterEvent],
    counter_id: int,
    timestamp: int,
    reason: int,
    *,
    stream_id: Optional[int] = None,
) -> None:
    event = next(events)
    assert event.kind == WriterEventKind.COUNTER_NO_VALUE_WRITE
    assert event.counter_id == counter_id
    assert event.counter_timestamp == timestamp
    assert event.counter_no_value_reason == reason
    if stream_id is not None:
        assert event.stream_id == stream_id


def verify_counter_batch_write(
    events: Iterator[RecordedWriterEvent],
    counter_id: int,
    *,
    timestamps: Optional[Iterable[int]] = None,
    stream_id: Optional[int] = None,
    flags: int = BatchFlag.SORTED,
) -> bytes:
    event = next(events)
    assert event.kind == WriterEventKind.COUNTER_BATCH_WRITE
    assert event.counter_id == counter_id
    assert event.counter_batch_flags == flags
    if timestamps is not None:
        assert event.counter_timestamps == tuple(timestamps)
    if stream_id is not None:
        assert event.stream_id == stream_id
    return event.counter_data


# Decoders for fixed event-helper payload layouts. Handles are 64-bit.
def decode_utf8_header(payload: bytes) -> Tuple[int, int, int]:
    """nvtxwEventUtf8Payload_t: int64 timestamp, uint32 color, category."""
    timestamp, color, category = struct.unpack("<qII", payload)
    return timestamp, color, category


def decode_reg_string(payload: bytes) -> Tuple[int, int, int, int]:
    """nvtxwEventRegStringPayload_t: header + uint64 message handle."""
    timestamp, color, category, message = struct.unpack("<qIIQ", payload)
    return timestamp, color, category, message


def decode_range_utf8_header(payload: bytes) -> Tuple[int, int, int, int]:
    """nvtxwRangeEventUtf8Payload_t: int64 begin, end, color, category."""
    begin, end, color, category = struct.unpack("<qqII", payload)
    return begin, end, color, category


def decode_range_reg_string(
    payload: bytes,
) -> Tuple[int, int, int, int, int]:
    """nvtxwRangeEventRegStringPayload_t: range header + message handle."""
    begin, end, color, category, message = struct.unpack("<qqIIQ", payload)
    return begin, end, color, category, message


def decode_range_id_utf8_header(
    payload: bytes,
) -> Tuple[int, int, int, int]:
    """nvtxwRangeIdEventUtf8Payload_t: range ID, timestamp, color, category."""
    range_id, timestamp, color, category = struct.unpack("<QqII", payload)
    return range_id, timestamp, color, category


def decode_range_id_reg_string(
    payload: bytes,
) -> Tuple[int, int, int, int, int]:
    """Registered-string range-start payload."""
    range_id, timestamp, color, category, message = struct.unpack("<QqIIQ", payload)
    return range_id, timestamp, color, category, message


def decode_pop(payload: bytes) -> int:
    """nvtxwRangePopPayload_t: int64 timestamp."""
    (timestamp,) = struct.unpack("<q", payload)
    return timestamp


def decode_end(payload: bytes) -> Tuple[int, int]:
    """nvtxwRangeEndPayload_t: uint64 rangeId, int64 timestamp."""
    range_id, timestamp = struct.unpack("<Qq", payload)
    return range_id, timestamp


# Test-authoring helpers: verified session/domain/stream environments and
# role-annotated dtypes shared by the writer tests.

SESSION_NAME = "test session"
STREAM_NAME = "test stream"

# Schema-level flag recorded for each nvtx.EventKind, from the independent
# copies of the NVTX_PAYLOAD_SCHEMA_FLAG_* constants above.
EVENT_KIND_FLAGS = {
    nvtx.EventKind.MARK: PayloadSchemaFlag.MARK,
    nvtx.EventKind.RANGE_PUSHPOP: PayloadSchemaFlag.RANGE_PUSHPOP,
    nvtx.EventKind.RANGE_STARTEND: PayloadSchemaFlag.RANGE_STARTEND,
    nvtx.EventKind.RANGE_PUSH: PayloadSchemaFlag.RANGE_PUSH,
    nvtx.EventKind.RANGE_POP: PayloadSchemaFlag.RANGE_POP,
    nvtx.EventKind.RANGE_START: PayloadSchemaFlag.RANGE_START,
    nvtx.EventKind.RANGE_END: PayloadSchemaFlag.RANGE_END,
}

# Role-annotated field dtypes for building event and counter layouts.
if np is not None:
    TS_MARK = nvtx.numpy_dtype("i8", entry_kind=nvtx.EntryKind.MARK)
    TS_BEGIN = nvtx.numpy_dtype("i8", entry_kind=nvtx.EntryKind.RANGE_BEGIN)
    TS_END = nvtx.numpy_dtype("i8", entry_kind=nvtx.EntryKind.RANGE_END)
    TS_COUNTER = nvtx.numpy_dtype("i8", entry_kind=nvtx.EntryKind.COUNTER_TIMESTAMP)
    RANGE_ID = nvtx.numpy_dtype("u8", entry_type=nvtx.PayloadEntryType.RANGE_ID)
else:
    TS_MARK = TS_BEGIN = TS_END = TS_COUNTER = RANGE_ID = None


@contextlib.contextmanager
def begin_session(
    backend: nvtx.writer.Backend,
    events: Iterator[RecordedWriterEvent],
    name: Optional[str] = SESSION_NAME,
    config: Optional[str] = None,
) -> Iterator[SimpleNamespace]:
    """An active session whose begin/end events are verified."""
    session_id = None
    try:
        with nvtx.writer.Session(name, backend=backend, config=config) as session:
            session_id = verify_session_begin(events, name, config)
            yield SimpleNamespace(session=session, session_id=session_id)
    finally:
        if session_id is not None:
            verify_session_end(events, session_id)


@contextlib.contextmanager
def open_stream(
    backend: nvtx.writer.Backend,
    events: Iterator[RecordedWriterEvent],
    domain_name: Optional[str] = None,
) -> Iterator[SimpleNamespace]:
    """An open default-configuration stream plus its session and domain."""
    with begin_session(backend, events) as env:
        env.domain = env.session.get_domain(domain_name)
        env.domain_id = verify_domain_register(
            events, domain_name, session_id=env.session_id
        )
        with env.session.create_stream(STREAM_NAME, domain=env.domain) as stream:
            env.stream = stream
            env.stream_id = verify_stream_open(
                events,
                STREAM_NAME,
                domain_id=env.domain_id,
                session_id=env.session_id,
            )
            yield env
        verify_stream_close(events, env.stream_id)
