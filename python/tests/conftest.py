# SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import ctypes
import enum
import os
import pytest
import setuptools
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Hashable, Optional, Set, Tuple, Union

import nvtx
from nvtx.colors import _NVTX_COLORS
from nvtx.nvtx import NVTX_DTYPE_METADATA_KEY_NAME, PayloadTypeAlias

try:
    import numpy as np
except ImportError:
    np = None

DtypeMetadataKey = Tuple[
    Optional[Hashable],  # NVTX metadata stored directly on this dtype, or None.
    Optional["DtypeMetadataKey"],  # Metadata key for dtype.subdtype, or None.
    Tuple[Tuple[str, "DtypeMetadataKey"], ...],  # (field name, key) pairs.
]

SchemaRegistrationKey = Tuple[
    "np.dtype",
    DtypeMetadataKey,
    bool,  # is_array
    int,  # schema_flags
]

CounterRegistrationKey = Tuple[
    str,  # Counter name.
    Hashable,  # Normalized dtype: int, float, or np.dtype.
    Optional[DtypeMetadataKey],
    Optional[str],  # Counter description.
    Optional[str],  # Scope path.
    Optional[nvtx.CounterSemantics],
    int,  # Timestamp domain ID.
]

# Mirrors nvtx3/nvToolsExtSemanticsCounters.h; these flags are not
# exported by nvtx._lib.counters.
COUNTER_FLAG_LIMIT_MIN = 1 << 2
COUNTER_FLAG_LIMIT_MAX = 1 << 3

# Mirrors nvtx3/nvToolsExtSemanticsCounters.h; these limit-type tags are
# intentionally kept independent from the production Cython module so tests
# catch schema mistakes.
COUNTER_LIMIT_UNDEFINED = 0
COUNTER_LIMIT_I64 = 1
COUNTER_LIMIT_U64 = 2
COUNTER_LIMIT_F64 = 3

# Mirrors nvtx3/nvToolsExtPayload.h; these values are intentionally kept
# independent from the production Cython module so tests catch schema mistakes.
PAYLOAD_ENTRY_TYPE_INT8 = 11
PAYLOAD_ENTRY_TYPE_UINT8 = 12
PAYLOAD_ENTRY_TYPE_INT16 = 13
PAYLOAD_ENTRY_TYPE_UINT16 = 14
PAYLOAD_ENTRY_TYPE_INT32 = 15
PAYLOAD_ENTRY_TYPE_UINT32 = 16
PAYLOAD_ENTRY_TYPE_INT64 = 17
PAYLOAD_ENTRY_TYPE_UINT64 = 18
PAYLOAD_ENTRY_TYPE_BYTE = 32
PAYLOAD_ENTRY_TYPE_FLOAT16 = 42
PAYLOAD_ENTRY_TYPE_FLOAT32 = 43
PAYLOAD_ENTRY_TYPE_FLOAT64 = 44
PAYLOAD_ENTRY_TYPE_CSTRING_UTF32 = 78
PAYLOAD_SCHEMA_FLAG_COUNTER_GROUP = 1 << 3

SCHEMA_ENTRY_BUFFER_COUNT = 16

TESTS_DIR = Path(__file__).parent
INCLUDE_DIR = TESTS_DIR.parent.parent / "c" / "include"
INJECTION_DIR = TESTS_DIR / "injection"
INJECTION_MODULE_NAME = "nvtx_test_injection"


def build_injection():
    """
    Compile the injection module to be used in tests and return the path to the compiled module.
    """
    owd = os.getcwd()
    try:
        os.chdir(INJECTION_DIR)
        setuptools.setup(
            script_args=["build_ext", "-i"],
            ext_modules=[
                setuptools.Extension(
                    INJECTION_MODULE_NAME,
                    sources=[str(INJECTION_DIR / "NvtxTestInjection.cpp")],
                    include_dirs=[str(INCLUDE_DIR)],
                )
            ],
        )
    finally:
        os.chdir(owd)
    return str(next(INJECTION_DIR.glob(f"{INJECTION_MODULE_NAME}.*")))


def pytest_addoption(parser):
    parser.addoption("--enable-injection", action="store_true", default=False)


@pytest.fixture(scope="session")
def enable_injection(request):
    return request.config.getoption("--enable-injection")


@pytest.fixture(scope="session", autouse=True)
def nvtx_events(enable_injection):
    if enable_injection:
        return NvtxEventsReader(build_injection())


@pytest.fixture(autouse=True)
def verify_all_events_consumed(nvtx_events):
    yield
    if nvtx_events is not None:
        assert not list(nvtx_events)


@pytest.fixture(scope="session", params=[None, "", "abc"])
def domain(request):
    return request.param


@pytest.fixture(scope="session", params=[None, "", "abc"])
def message(request):
    return request.param


@pytest.fixture(scope="session", params=[None, 0xFFA500, "red"])
def color(request):
    return request.param


@pytest.fixture(scope="session", params=[None, 0, "", "abc", 1])
def category(request):
    return request.param


@pytest.fixture(scope="session", params=[None, 1, 1.0])
def payload(request):
    return request.param


DEFAULT_DOMAIN = "Default"
EVENT_BUFFER_SIZE = 256
TIMESTAMP_BUFFER_COUNT = 16
MAX_PAYLOAD_SIZE = EVENT_BUFFER_SIZE
MAX_TIMESTAMPS_SIZE = TIMESTAMP_BUFFER_COUNT * ctypes.sizeof(ctypes.c_int64)


def _domain_name(domain: Optional[str]) -> str:
    return DEFAULT_DOMAIN if domain is None else domain


class MessageType(enum.IntEnum):
    # Subset of nvtxMessageType_t (nvToolsExt.h)
    UNKNOWN = 0
    REGISTERED = 3


class EventKind(enum.IntEnum):
    # Must match EventKind enum in NvtxTestInjection.cpp
    DOMAIN_CREATE = 1
    DOMAIN_REGISTER_STRING = 2
    DOMAIN_NAME_CATEGORY = 3
    MARK = 4
    RANGE_PUSH = 5
    RANGE_POP = 6
    RANGE_START = 7
    RANGE_END = 8
    PAYLOAD_SCHEMA_REGISTER = 9
    COUNTER_REGISTER = 10
    COUNTER_SAMPLE = 11
    COUNTER_SAMPLE_INT64 = 12
    COUNTER_SAMPLE_FLOAT64 = 13
    COUNTER_SAMPLE_NO_VALUE = 14
    COUNTER_BATCH_SUBMIT = 15
    TIMESTAMP_GET = 16
    SCOPE_REGISTER = 17


class PayloadType(enum.IntEnum):
    # Subset of nvtxPayloadType_t (nvToolsExt.h)
    UNKNOWN = 0
    INT64 = 2
    DOUBLE = 3
    EXT = ctypes.c_int32(0xDFBD0009).value


@dataclass(frozen=True)
class RecordedEvent:
    kind: EventKind
    domain: str
    message_type: MessageType
    message: str
    category: int
    range_id: int
    color: int
    payload_type: int
    payload_i64: int
    payload_f64: float
    payload_ext_schema_id: int
    payload_schema_type: int
    payload_schema_flags: int
    payload_schema_num_entries: int
    payload_schema_static_size: int
    payload_schema_entry_types: tuple
    payload_ext_data: bytes
    counter_id: int
    counter_sample_i64: int
    counter_sample_f64: float
    counter_sample_no_value_reason: int
    counter_sample_data: bytes
    timestamps: tuple
    counter_schema_id: int
    counter_name: str
    counter_description: Optional[str]
    counter_scope_id: int
    counter_has_semantics: bool
    counter_has_time_semantics: bool
    counter_time_domain_id: int
    counter_semantics_flags: int
    counter_semantics_unit: str
    counter_semantics_unit_scale_numerator: int
    counter_semantics_unit_scale_denominator: int
    counter_semantics_limit_type: int
    counter_semantics_min_i64: int
    counter_semantics_min_u64: int
    counter_semantics_min_f64: float
    counter_semantics_max_i64: int
    counter_semantics_max_u64: int
    counter_semantics_max_f64: float
    timestamp_value: int
    scope_id: int
    scope_path: str


class DomainData:
    def __init__(self, name: str):
        self.name = name
        self.registered_strings: Set[str] = set()
        self.registered_categories: Dict[str, int] = {}
        self.registered_scopes: Dict[str, int] = {}
        self.registered_counters: Dict[CounterRegistrationKey, int] = {}
        if np is not None:
            self.registered_schemas: Dict[SchemaRegistrationKey, int] = {}


registered_domains: Dict[str, DomainData] = {}


class _EventAttributeBuffer(ctypes.Structure):
    _fields_ = [
        ("message_type", ctypes.c_int32),
        ("message", ctypes.c_char_p),
        ("category", ctypes.c_uint32),
        ("range_id", ctypes.c_uint64),
        ("color", ctypes.c_uint32),
    ]


class _EventPayloadBuffer(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int32),
        ("i64", ctypes.c_int64),
        ("f64", ctypes.c_double),
        ("ext_schema_id", ctypes.c_uint64),
        ("ext_size", ctypes.c_uint64),
        ("schema_type", ctypes.c_uint64),
        ("schema_flags", ctypes.c_uint64),
        ("schema_num_entries", ctypes.c_uint64),
        ("schema_static_size", ctypes.c_uint64),
        ("schema_entry_types", ctypes.c_uint64 * SCHEMA_ENTRY_BUFFER_COUNT),
        ("ext_data", ctypes.c_uint8 * EVENT_BUFFER_SIZE),
    ]


class _EventCounterSemanticsBuffer(ctypes.Structure):
    _fields_ = [
        ("has_semantics", ctypes.c_uint8),
        ("has_time_semantics", ctypes.c_uint8),
        ("time_domain_id", ctypes.c_uint64),
        ("flags", ctypes.c_uint64),
        ("unit", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("unit_scale_numerator", ctypes.c_uint64),
        ("unit_scale_denominator", ctypes.c_uint64),
        ("limit_type", ctypes.c_int64),
        ("min_i64", ctypes.c_int64),
        ("min_u64", ctypes.c_uint64),
        ("min_f64", ctypes.c_double),
        ("max_i64", ctypes.c_int64),
        ("max_u64", ctypes.c_uint64),
        ("max_f64", ctypes.c_double),
    ]


class _EventCounterRegistrationBuffer(ctypes.Structure):
    _fields_ = [
        ("schema_id", ctypes.c_uint64),
        ("name", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("description", ctypes.c_char * EVENT_BUFFER_SIZE),
        ("scope_id", ctypes.c_uint64),
        ("semantics", _EventCounterSemanticsBuffer),
    ]


class _EventCounterSampleBuffer(ctypes.Structure):
    _fields_ = [
        ("i64", ctypes.c_int64),
        ("f64", ctypes.c_double),
        ("no_value_reason", ctypes.c_uint8),
        ("data_size", ctypes.c_uint64),
        ("data", ctypes.c_uint8 * EVENT_BUFFER_SIZE),
        ("timestamps_size", ctypes.c_uint64),
        ("timestamps", ctypes.c_int64 * TIMESTAMP_BUFFER_COUNT),
    ]


class _EventCounterBuffer(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_uint64),
        ("registration", _EventCounterRegistrationBuffer),
        ("sample", _EventCounterSampleBuffer),
    ]


class _EventTimestampBuffer(ctypes.Structure):
    _fields_ = [
        ("value", ctypes.c_int64),
    ]


class _EventScopeBuffer(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_uint64),
        ("path", ctypes.c_char * EVENT_BUFFER_SIZE),
    ]


class _EventBuffer(ctypes.Structure):
    # Fields must match the EventRecord struct in NvtxTestInjection.cpp
    _fields_ = [
        ("kind", ctypes.c_uint32),
        ("domain", ctypes.c_char_p),
        ("attributes", _EventAttributeBuffer),
        ("payload", _EventPayloadBuffer),
        ("counter", _EventCounterBuffer),
        ("timestamp", _EventTimestampBuffer),
        ("scope", _EventScopeBuffer),
    ]


class NvtxEventsReader:
    Buffer = _EventBuffer

    def __init__(self, injection_path: Path):
        self._injection_module = ctypes.CDLL(injection_path)
        self._injection_module.read_event.argtypes = [ctypes.POINTER(self.Buffer)]
        self._injection_module.read_event.restype = ctypes.c_bool
        os.environ["NVTX_INJECTION64_PATH"] = injection_path

    def __iter__(self):
        return self

    def __next__(self) -> RecordedEvent:
        buffer = self.Buffer()
        if not self._injection_module.read_event(ctypes.byref(buffer)):
            # `read_event()` returns `false` if the buffer is empty.
            # (On error, it raises a Python exception.)
            raise StopIteration
        attributes = buffer.attributes
        payload = buffer.payload
        counter = buffer.counter
        counter_registration = counter.registration
        counter_semantics = counter_registration.semantics
        counter_sample = counter.sample
        if payload.schema_num_entries > SCHEMA_ENTRY_BUFFER_COUNT:
            raise RuntimeError(
                "Payload schema registration has "
                f"{payload.schema_num_entries} entries, but the test event "
                f"buffer only stores {SCHEMA_ENTRY_BUFFER_COUNT}."
            )

        # Defensive bounds checks on counter sample sizes to fail fast on
        # malformed/overflowing payloads.
        data_size = counter_sample.data_size
        if data_size < 0 or data_size > MAX_PAYLOAD_SIZE:
            raise ValueError(
                f"Counter sample data_size {data_size} is out of range "
                f"[0, {MAX_PAYLOAD_SIZE}]."
            )
        timestamps_size = counter_sample.timestamps_size
        if timestamps_size < 0 or timestamps_size > MAX_TIMESTAMPS_SIZE:
            raise ValueError(
                f"Counter sample timestamps_size {timestamps_size} is out of "
                f"range [0, {MAX_TIMESTAMPS_SIZE}]."
            )
        if timestamps_size % ctypes.sizeof(ctypes.c_int64) != 0:
            raise ValueError(
                f"Counter sample timestamps_size {timestamps_size} is not a "
                f"multiple of {ctypes.sizeof(ctypes.c_int64)} "
                "(int64 alignment)."
            )
        return RecordedEvent(
            kind=EventKind(buffer.kind),
            domain=buffer.domain.decode(),
            message_type=attributes.message_type,
            message=attributes.message.decode()
            if attributes.message_type == MessageType.REGISTERED
            else "",
            category=attributes.category,
            range_id=attributes.range_id,
            color=attributes.color,
            payload_type=payload.type,
            payload_i64=payload.i64,
            payload_f64=payload.f64,
            payload_ext_schema_id=payload.ext_schema_id,
            payload_schema_type=payload.schema_type,
            payload_schema_flags=payload.schema_flags,
            payload_schema_num_entries=payload.schema_num_entries,
            payload_schema_static_size=payload.schema_static_size,
            payload_schema_entry_types=tuple(
                payload.schema_entry_types[: payload.schema_num_entries]
            ),
            payload_ext_data=bytes(payload.ext_data[: payload.ext_size]),
            counter_id=counter.id,
            counter_sample_i64=counter_sample.i64,
            counter_sample_f64=counter_sample.f64,
            counter_sample_no_value_reason=counter_sample.no_value_reason,
            counter_sample_data=bytes(counter_sample.data[:data_size]),
            timestamps=tuple(
                counter_sample.timestamps[
                    : timestamps_size // ctypes.sizeof(ctypes.c_int64)
                ]
            ),
            counter_schema_id=counter_registration.schema_id,
            counter_name=counter_registration.name.decode(),
            counter_description=counter_registration.description.decode(),
            counter_scope_id=counter_registration.scope_id,
            counter_has_semantics=bool(counter_semantics.has_semantics),
            counter_has_time_semantics=bool(counter_semantics.has_time_semantics),
            counter_time_domain_id=counter_semantics.time_domain_id,
            counter_semantics_flags=counter_semantics.flags,
            counter_semantics_unit=counter_semantics.unit.decode(),
            counter_semantics_unit_scale_numerator=(
                counter_semantics.unit_scale_numerator
            ),
            counter_semantics_unit_scale_denominator=(
                counter_semantics.unit_scale_denominator
            ),
            counter_semantics_limit_type=counter_semantics.limit_type,
            counter_semantics_min_i64=counter_semantics.min_i64,
            counter_semantics_min_u64=counter_semantics.min_u64,
            counter_semantics_min_f64=counter_semantics.min_f64,
            counter_semantics_max_i64=counter_semantics.max_i64,
            counter_semantics_max_u64=counter_semantics.max_u64,
            counter_semantics_max_f64=counter_semantics.max_f64,
            timestamp_value=buffer.timestamp.value,
            scope_id=buffer.scope.id,
            scope_path=buffer.scope.path.decode(),
        )


def verify_registration_events(
    events: NvtxEventsReader,
    domain: Optional[str],
    message: Optional[str] = None,
    category: Optional[Union[str, int]] = None,
    payload: Optional[PayloadTypeAlias] = None,
):
    domain_data = _ensure_domain(events, domain)

    if isinstance(category, str):
        if category not in domain_data.registered_categories:
            event = next(events)
            assert event.kind == EventKind.DOMAIN_NAME_CATEGORY
            assert event.domain == domain_data.name
            assert event.message == category
            domain_data.registered_categories[category] = event.category
        category = domain_data.registered_categories[category]

    if message is not None:
        if message not in domain_data.registered_strings:
            event = next(events)
            assert event.kind == EventKind.DOMAIN_REGISTER_STRING
            assert event.domain == domain_data.name
            assert event.message == message
            domain_data.registered_strings.add(message)

    if payload is not None and np is not None:
        verify_payload_schema_registration(events, domain_data.name, np.array(payload))


if np is not None:
    PAYLOAD_ENTRY_TYPES_BY_DTYPE = {
        np.int8: PAYLOAD_ENTRY_TYPE_INT8,
        np.uint8: PAYLOAD_ENTRY_TYPE_UINT8,
        np.int16: PAYLOAD_ENTRY_TYPE_INT16,
        np.uint16: PAYLOAD_ENTRY_TYPE_UINT16,
        np.int32: PAYLOAD_ENTRY_TYPE_INT32,
        np.uint32: PAYLOAD_ENTRY_TYPE_UINT32,
        np.int64: PAYLOAD_ENTRY_TYPE_INT64,
        np.uint64: PAYLOAD_ENTRY_TYPE_UINT64,
        np.float16: PAYLOAD_ENTRY_TYPE_FLOAT16,
        np.float32: PAYLOAD_ENTRY_TYPE_FLOAT32,
        np.float64: PAYLOAD_ENTRY_TYPE_FLOAT64,
        np.bytes_: PAYLOAD_ENTRY_TYPE_BYTE,
        np.str_: PAYLOAD_ENTRY_TYPE_CSTRING_UTF32,
    }

    # Keep this local so tests validate the expected schema key independently
    # from the production _dtype_metadata_key implementation.
    def dtype_metadata_key(dtype) -> DtypeMetadataKey:
        metadata = dtype.metadata
        payload_metadata = None
        if metadata is not None:
            payload_metadata = metadata.get(NVTX_DTYPE_METADATA_KEY_NAME)

        subdtype_key = None
        if dtype.subdtype:
            subdtype_key = dtype_metadata_key(dtype.subdtype[0])

        field_keys = ()
        if dtype.fields:
            field_keys = tuple(
                (field_name, dtype_metadata_key(field[0]))
                for field_name, field in dtype.fields.items()
            )
        return payload_metadata, subdtype_key, field_keys

    def verify_payload_dtype_schema_registration(
        events: NvtxEventsReader,
        domain: str,
        dtype: np.dtype,
        is_array: bool,
        schema_flags: int = 0,
    ):
        def counter_group_direct_entry_type(dtype):
            if dtype.subdtype:
                dtype = dtype.subdtype[0]
            return PAYLOAD_ENTRY_TYPES_BY_DTYPE.get(dtype.type)

        def counter_group_embeds_field(dtype, expected_schema_flags):
            # Counter groups encode builtin scalar/fixed-size array fields directly
            # in the group schema rather than registering child schemas for them.
            return bool(
                expected_schema_flags & PAYLOAD_SCHEMA_FLAG_COUNTER_GROUP
                and counter_group_direct_entry_type(dtype) is not None
            )

        def registered_schema_entry_type(dtype):
            return registered_schemas[
                dtype, dtype_metadata_key(dtype), False, 0
            ]

        def child_schema_dtypes(dtype, expected_schema_flags):
            if dtype.subdtype:
                return (dtype.subdtype[0],)
            if dtype.fields:
                return tuple(
                    field_type
                    for field_type, *_ in dtype.fields.values()
                    if not counter_group_embeds_field(field_type, expected_schema_flags)
                )
            return ()

        def field_entry_type(field_type, expected_schema_flags):
            if counter_group_embeds_field(field_type, expected_schema_flags):
                return counter_group_direct_entry_type(field_type)
            return registered_schema_entry_type(field_type)

        def expected_schema_entry_types(dtype, is_array, expected_schema_flags):
            if is_array:
                return (
                    PAYLOAD_ENTRY_TYPE_UINT64,
                    registered_schema_entry_type(dtype),
                )

            if dtype.subdtype:
                return (registered_schema_entry_type(dtype.subdtype[0]),)

            if dtype.fields:
                return tuple(
                    field_entry_type(field_type, expected_schema_flags)
                    for field_type, *_ in dtype.fields.values()
                )

            return (PAYLOAD_ENTRY_TYPES_BY_DTYPE.get(dtype.type),)

        def handle_event(dtype, is_array, expected_schema_flags=0):
            current_schema_key = (
                dtype,
                dtype_metadata_key(dtype),
                is_array,
                expected_schema_flags,
            )
            if current_schema_key in registered_schemas:
                return

            for child_dtype in child_schema_dtypes(dtype, expected_schema_flags):
                handle_event(child_dtype, False)

            event = next(events)

            if is_array:
                expected_num_entries = 2
                expected_static_size = 0
            elif dtype.fields and not dtype.subdtype:
                expected_num_entries = len(dtype.fields)
                expected_static_size = dtype.itemsize
            else:
                expected_num_entries = 1
                expected_static_size = dtype.itemsize

            expected_entry_types = expected_schema_entry_types(
                dtype, is_array, expected_schema_flags
            )
            assert event.kind == EventKind.PAYLOAD_SCHEMA_REGISTER
            assert event.domain == domain
            assert event.payload_schema_flags == expected_schema_flags
            assert event.payload_schema_num_entries == expected_num_entries
            assert event.payload_schema_static_size == expected_static_size
            assert event.payload_schema_entry_types == expected_entry_types
            registered_schemas[current_schema_key] = event.payload_ext_schema_id

        registered_schemas = registered_domains.get(domain).registered_schemas
        if (
            dtype,
            dtype_metadata_key(dtype),
            is_array,
            schema_flags,
        ) in registered_schemas:
            return
        if is_array:
            scalar_key = dtype, dtype_metadata_key(dtype), False, 0
            if scalar_key not in registered_schemas:
                handle_event(dtype, False)
        handle_event(dtype, is_array, schema_flags)

    def verify_payload_schema_registration(
        events: NvtxEventsReader, domain: str, payload: np.ndarray
    ):
        verify_payload_dtype_schema_registration(
            events, domain, payload.dtype, bool(payload.ndim)
        )

    def verify_counter_schema_registration(
        events: NvtxEventsReader,
        domain: str,
        dtype: np.dtype,
    ):
        # Counter groups must set the flag on the top-level schema.
        schema_flags = (
            PAYLOAD_SCHEMA_FLAG_COUNTER_GROUP
            if dtype.fields is not None
            else 0
        )
        verify_payload_dtype_schema_registration(
            events,
            domain,
            dtype,
            False,
            schema_flags,
        )

    def verify_ext_payload(event: RecordedEvent, payload, domain: str):
        if not isinstance(payload, np.ndarray):
            payload = np.array(payload)

        if payload.nbytes == 0:
            assert event.payload_type == PayloadType.UNKNOWN
            return

        assert event.payload_type == PayloadType.EXT

        is_array = bool(payload.ndim)
        expected_schema_id = registered_domains[domain].registered_schemas[
            (payload.dtype, dtype_metadata_key(payload.dtype), is_array, 0)
        ]
        assert event.payload_ext_schema_id == expected_schema_id

        expected_bytes = payload.tobytes()
        if is_array:
            expected_bytes = payload.size.to_bytes(8, "little") + expected_bytes
        assert event.payload_ext_data == expected_bytes


def _verify_attributes(
    event: RecordedEvent,
    domain: Optional[str],
    message: Optional[str],
    color: Optional[int],
    category: Optional[Union[str, int]],
    payload: Optional[PayloadTypeAlias],
):
    domain_name = _domain_name(domain)
    assert event.domain == domain_name
    if message is None:
        assert event.message_type == MessageType.UNKNOWN
    else:
        assert event.message_type == MessageType.REGISTERED
        assert event.message == message
    if category is None:
        category = 0
    if isinstance(category, str):
        category = registered_domains[domain_name].registered_categories[category]
    assert event.category == category
    assert event.color == _NVTX_COLORS.get(color, color)
    if payload is None:
        assert event.payload_type == PayloadType.UNKNOWN
    elif isinstance(payload, int):
        assert event.payload_type == PayloadType.INT64
        assert event.payload_i64 == payload
    elif isinstance(payload, float):
        assert event.payload_type == PayloadType.DOUBLE
        assert event.payload_f64 == payload
    elif np is not None:
        verify_ext_payload(event, payload, domain_name)


def verify_mark(
    events: NvtxEventsReader,
    domain: Optional[str],
    message: Optional[str],
    color: Optional[int],
    category: Optional[Union[str, int]],
    payload: Optional[PayloadTypeAlias],
):
    mark = next(events)
    assert mark.kind == EventKind.MARK
    _verify_attributes(mark, domain, message, color, category, payload)


def verify_push(
    events: NvtxEventsReader,
    domain: Optional[str],
    message: Optional[str],
    color: Optional[int],
    category: Optional[Union[str, int]],
    payload: Optional[PayloadTypeAlias],
):
    push = next(events)
    assert push.kind == EventKind.RANGE_PUSH
    _verify_attributes(push, domain, message, color, category, payload)


def verify_pop(events: NvtxEventsReader, domain: Optional[str]):
    pop = next(events)
    assert pop.kind == EventKind.RANGE_POP
    assert pop.domain == _domain_name(domain)


def verify_start(
    events: NvtxEventsReader,
    domain: Optional[str],
    message: Optional[str],
    color: Optional[int],
    category: Optional[Union[str, int]],
    payload: Optional[PayloadTypeAlias],
):
    start = next(events)
    assert start.kind == EventKind.RANGE_START
    _verify_attributes(start, domain, message, color, category, payload)


def verify_end(events: NvtxEventsReader, domain: Optional[str], range_id: int):
    end = next(events)
    assert end.kind == EventKind.RANGE_END
    assert end.domain == _domain_name(domain)
    if isinstance(range_id, tuple):
        range_id = range_id[0]
    assert end.range_id == range_id


def _ensure_domain(events: NvtxEventsReader, domain: Optional[str]) -> DomainData:
    domain = _domain_name(domain)

    domain_data = registered_domains.get(domain)
    if domain_data is None:
        domain_data = registered_domains[domain] = DomainData(domain)
        if domain != DEFAULT_DOMAIN:
            event = next(events)
            assert event.kind == EventKind.DOMAIN_CREATE
            assert event.domain == domain
    return domain_data


def verify_timestamp_get(events, expected_value: int):
    event = next(events)
    assert event.kind == EventKind.TIMESTAMP_GET
    assert event.timestamp_value == expected_value


def verify_scope_register(events, domain, expected_path: str):
    domain_data = _ensure_domain(events, domain)
    event = next(events)
    assert event.kind == EventKind.SCOPE_REGISTER
    assert event.domain == domain_data.name
    assert event.scope_path == expected_path
    domain_data.registered_scopes[expected_path] = event.scope_id
    return event.scope_id


def _expected_counter_flags(semantics):
    flags = semantics.value_type.value | semantics.interpolation.value
    if semantics.min is not None:
        flags |= COUNTER_FLAG_LIMIT_MIN
    if semantics.max is not None:
        flags |= COUNTER_FLAG_LIMIT_MAX
    return flags


def _verify_counter_semantics(event, semantics):
    assert event.counter_has_semantics == (semantics is not None)
    if semantics is None:
        return

    expected_unit = semantics.unit.decode() if semantics.unit is not None else ""

    assert event.counter_semantics_flags == _expected_counter_flags(semantics)
    assert event.counter_semantics_unit == expected_unit
    assert (
        event.counter_semantics_unit_scale_numerator == semantics.unit_scale_numerator
    )
    assert (
        event.counter_semantics_unit_scale_denominator
        == semantics.unit_scale_denominator
    )

    if semantics.min is None and semantics.max is None:
        assert event.counter_semantics_limit_type == COUNTER_LIMIT_UNDEFINED
    elif isinstance(
        semantics.min if semantics.min is not None else semantics.max, float
    ):
        assert event.counter_semantics_limit_type == COUNTER_LIMIT_F64
        if semantics.min is not None:
            assert event.counter_semantics_min_f64 == semantics.min
        if semantics.max is not None:
            assert event.counter_semantics_max_f64 == semantics.max
    else:
        assert event.counter_semantics_limit_type in (
            COUNTER_LIMIT_I64,
            COUNTER_LIMIT_U64,
        )
        if event.counter_semantics_limit_type == COUNTER_LIMIT_I64:
            if semantics.min is not None:
                assert event.counter_semantics_min_i64 == semantics.min
            if semantics.max is not None:
                assert event.counter_semantics_max_i64 == semantics.max
        else:
            if semantics.min is not None:
                assert event.counter_semantics_min_u64 == semantics.min
            if semantics.max is not None:
                assert event.counter_semantics_max_u64 == semantics.max


def _normalize_counter_dtype_for_cache(dtype):
    # Mirrors nvtx._lib.lib._normalize_counter_dtype
    if np is None:
        return dtype
    if dtype is int:
        return np.dtype(np.int64)
    return np.dtype(dtype)


def _counter_registration_key(
    name,
    dtype,
    description,
    scope,
    semantics,
    time_domain,
):
    dtype_metadata = None
    if np is not None and isinstance(dtype, np.dtype):
        dtype_metadata = dtype_metadata_key(dtype)
    return (
        name,
        dtype,
        dtype_metadata,
        description,
        scope,
        semantics,
        getattr(time_domain, "value", time_domain),
    )


def verify_counter_register(
    events,
    domain,
    name,
    dtype,
    description=None,
    scope=None,
    semantics=None,
    time_domain=nvtx.TimestampType.TOOL_PROVIDED,
):
    domain_data = _ensure_domain(events, domain)

    dtype = _normalize_counter_dtype_for_cache(dtype)
    counter_key = _counter_registration_key(
        name,
        dtype,
        description,
        scope,
        semantics,
        time_domain,
    )
    if counter_key in domain_data.registered_counters:
        return domain_data.registered_counters[counter_key]

    if np is not None and isinstance(dtype, np.dtype):
        if dtype not in (np.dtype(int), np.dtype(float)):
            verify_counter_schema_registration(
                events,
                domain_data.name,
                dtype,
            )

    scope_id = 0
    if scope is not None:
        scope_id = domain_data.registered_scopes.get(scope)
        if scope_id is None:
            scope_id = verify_scope_register(events, domain, scope)

    time_domain_id = getattr(time_domain, "value", time_domain)
    event = next(events)
    assert event.kind == EventKind.COUNTER_REGISTER
    assert event.domain == domain_data.name
    assert event.counter_name == name
    assert event.counter_description == (description or "")
    assert event.counter_scope_id == scope_id
    assert event.counter_has_time_semantics
    assert event.counter_time_domain_id == time_domain_id
    _verify_counter_semantics(event, semantics)
    domain_data.registered_counters[counter_key] = event.counter_id
    return event.counter_id


def verify_counter_sample_int64(events, domain, counter_id, value):
    event = next(events)
    assert event.kind == EventKind.COUNTER_SAMPLE_INT64
    assert event.domain == _domain_name(domain)
    assert event.counter_id == counter_id
    assert event.counter_sample_i64 == value


def verify_counter_sample_float64(events, domain, counter_id, value):
    event = next(events)
    assert event.kind == EventKind.COUNTER_SAMPLE_FLOAT64
    assert event.domain == _domain_name(domain)
    assert event.counter_id == counter_id
    assert event.counter_sample_f64 == value


def verify_counter_sample(events, domain, counter_id, expected_bytes):
    event = next(events)
    assert event.kind == EventKind.COUNTER_SAMPLE
    assert event.domain == _domain_name(domain)
    assert event.counter_id == counter_id
    assert event.counter_sample_data == expected_bytes


def verify_counter_sample_no_value(events, domain, counter_id, reason):
    event = next(events)
    assert event.kind == EventKind.COUNTER_SAMPLE_NO_VALUE
    assert event.domain == _domain_name(domain)
    assert event.counter_id == counter_id
    assert event.counter_sample_no_value_reason == reason


def verify_counter_batch_submit(
    events,
    domain,
    counter_id,
    expected_bytes,
    expected_timestamps,
):
    event = next(events)
    assert event.kind == EventKind.COUNTER_BATCH_SUBMIT
    assert event.domain == _domain_name(domain)
    assert event.counter_id == counter_id
    assert event.counter_sample_data == expected_bytes
    assert event.timestamps == tuple(expected_timestamps)
