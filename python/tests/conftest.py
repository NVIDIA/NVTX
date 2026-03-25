import ctypes
import enum
import os
import pytest
import setuptools
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional, Set, Union

from nvtx.colors import _NVTX_COLORS
from nvtx.nvtx import PayloadTypeAlias

try:
    import numpy as np
except ImportError:
    np = None


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
    payload_ext_data: bytes


class DomainData:
    def __init__(self):
        self.registered_strings: Set[str] = set()
        self.registered_categories: Dict[str, int] = {}
        if np is not None:
            self.registered_schemas: Dict[tuple[np.dtype, bool], int] = {}


registered_domains: Dict[str, DomainData] = {}


class NvtxEventsReader:
    class Buffer(ctypes.Structure):
        # Fields must match the EventRecord struct in NvtxTestInjection.cpp
        _fields_ = [
            ("kind", ctypes.c_uint32),
            ("domain", ctypes.c_char_p),
            ("message_type", ctypes.c_int32),
            ("message", ctypes.c_char_p),
            ("category", ctypes.c_uint32),
            ("range_id", ctypes.c_uint64),
            ("color", ctypes.c_uint32),
            ("payload_type", ctypes.c_int32),
            ("payload_i64", ctypes.c_int64),
            ("payload_f64", ctypes.c_double),
            ("payload_ext_schema_id", ctypes.c_uint64),
            ("payload_ext_size", ctypes.c_uint64),
            ("payload_ext_data", ctypes.c_uint8 * 256),
        ]

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
        return RecordedEvent(
            kind=EventKind(buffer.kind),
            domain=buffer.domain.decode(),
            message_type=buffer.message_type,
            message=buffer.message.decode()
            if buffer.message_type == MessageType.REGISTERED
            else "",
            category=buffer.category,
            range_id=buffer.range_id,
            color=buffer.color,
            payload_type=buffer.payload_type,
            payload_i64=buffer.payload_i64,
            payload_f64=buffer.payload_f64,
            payload_ext_schema_id=buffer.payload_ext_schema_id,
            payload_ext_data=bytes(buffer.payload_ext_data[: buffer.payload_ext_size]),
        )


def verify_registration_events(
    events: NvtxEventsReader,
    domain: Optional[str],
    message: Optional[str] = None,
    category: Optional[Union[str, int]] = None,
    payload: Optional[PayloadTypeAlias] = None,
):
    if domain is None:
        domain = DEFAULT_DOMAIN

    domain_data = registered_domains.get(domain)
    if domain_data is None:
        domain_data = registered_domains[domain] = DomainData()
        if domain is not DEFAULT_DOMAIN:
            event = next(events)
            assert event.kind == EventKind.DOMAIN_CREATE
            assert event.domain == domain

    if isinstance(category, str):
        if category not in domain_data.registered_categories:
            event = next(events)
            assert event.kind == EventKind.DOMAIN_NAME_CATEGORY
            assert event.domain == domain
            assert event.message == category
            domain_data.registered_categories[category] = event.category
        category = domain_data.registered_categories[category]

    if message is not None:
        if message not in domain_data.registered_strings:
            event = next(events)
            assert event.kind == EventKind.DOMAIN_REGISTER_STRING
            assert event.domain == domain
            assert event.message == message
            domain_data.registered_strings.add(message)

    if payload is not None and np is not None:
        verify_payload_schema_registration(events, domain, np.array(payload))


if np is not None:

    def verify_payload_schema_registration(
        events: NvtxEventsReader, domain: str, payload: np.ndarray
    ):
        def handle_event(dtype, is_array):
            if dtype.metadata is not None:
                for inner_dtype in dtype.metadata.get("inner_dtypes", ()):
                    inner_key = inner_dtype, False
                    if inner_key not in registered_schemas:
                        handle_event(*inner_key)
            event = next(events)
            assert event.kind == EventKind.PAYLOAD_SCHEMA_REGISTER
            assert event.domain == domain
            registered_schemas[(dtype, is_array)] = event.payload_ext_schema_id

        dtype = payload.dtype
        is_array = bool(payload.ndim)
        schema_key = dtype, is_array
        registered_schemas = registered_domains.get(domain).registered_schemas
        if schema_key in registered_schemas:
            return
        if is_array:
            scalar_key = dtype, False
            if scalar_key not in registered_schemas:
                handle_event(*scalar_key)
        handle_event(*schema_key)

    def verify_ext_payload(event: RecordedEvent, payload, domain: str):
        if not isinstance(payload, np.ndarray):
            payload = np.array(payload)

        if payload.nbytes == 0:
            assert event.payload_type == PayloadType.UNKNOWN
            return

        assert event.payload_type == PayloadType.EXT

        is_array = bool(payload.ndim)
        expected_schema_id = registered_domains[domain].registered_schemas[
            (payload.dtype, is_array)
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
    if domain is None:
        domain = DEFAULT_DOMAIN
    assert event.domain == domain
    if message is None:
        assert event.message_type == MessageType.UNKNOWN
    else:
        assert event.message_type == MessageType.REGISTERED
        assert event.message == message
    if category is None:
        category = 0
    if isinstance(category, str):
        category = registered_domains[domain].registered_categories[category]
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
        verify_ext_payload(event, payload, domain)


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
    if domain is None:
        domain = DEFAULT_DOMAIN
    assert pop.domain == domain


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
    if domain is None:
        domain = DEFAULT_DOMAIN
    assert end.domain == domain
    if isinstance(range_id, tuple):
        range_id = range_id[0]
    assert end.range_id == range_id
