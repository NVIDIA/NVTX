# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Payload schema registration tests for nvtx.writer."""

import pytest

import nvtx

from .conftest import (
    PayloadEntryFlag,
    PayloadEntryType,
    PayloadSchemaType,
    EVENT_KIND_FLAGS,
    ExpectedSchemaEntry,
    expected_timestamp_entry,
    RANGE_ID,
    TS_BEGIN,
    TS_COUNTER,
    TS_END,
    TS_MARK,
    begin_session,
    verify_domain_register,
    verify_schema_register,
)

np = pytest.importorskip("numpy")

_MARK = nvtx.EventKind.MARK

# A minimal valid layout for every event kind.
MINIMAL_EVENT_FIELDS = {
    nvtx.EventKind.MARK: [("timestamp", TS_MARK)],
    nvtx.EventKind.RANGE_PUSH: [("timestamp", TS_BEGIN)],
    nvtx.EventKind.RANGE_POP: [("timestamp", TS_END)],
    nvtx.EventKind.RANGE_PUSHPOP: [("begin", TS_BEGIN), ("end", TS_END)],
    nvtx.EventKind.RANGE_STARTEND: [("begin", TS_BEGIN), ("end", TS_END)],
    nvtx.EventKind.RANGE_START: [
        ("range_id", RANGE_ID),
        ("timestamp", TS_BEGIN),
    ],
    nvtx.EventKind.RANGE_END: [
        ("range_id", RANGE_ID),
        ("timestamp", TS_END),
    ],
}

MINIMAL_EVENT_ENTRIES = {
    nvtx.EventKind.MARK: [expected_timestamp_entry("timestamp", PayloadEntryFlag.MARK)],
    nvtx.EventKind.RANGE_PUSH: [
        expected_timestamp_entry("timestamp", PayloadEntryFlag.RANGE_BEGIN)
    ],
    nvtx.EventKind.RANGE_POP: [
        expected_timestamp_entry("timestamp", PayloadEntryFlag.RANGE_END)
    ],
    nvtx.EventKind.RANGE_PUSHPOP: [
        expected_timestamp_entry("begin", PayloadEntryFlag.RANGE_BEGIN),
        expected_timestamp_entry("end", PayloadEntryFlag.RANGE_END, offset=8),
    ],
    nvtx.EventKind.RANGE_STARTEND: [
        expected_timestamp_entry("begin", PayloadEntryFlag.RANGE_BEGIN),
        expected_timestamp_entry("end", PayloadEntryFlag.RANGE_END, offset=8),
    ],
    nvtx.EventKind.RANGE_START: [
        ExpectedSchemaEntry(
            name="range_id",
            entry_type=PayloadEntryType.RANGE_ID,
        ),
        expected_timestamp_entry(
            "timestamp",
            PayloadEntryFlag.RANGE_BEGIN,
            offset=8,
        ),
    ],
    nvtx.EventKind.RANGE_END: [
        ExpectedSchemaEntry(
            name="range_id",
            entry_type=PayloadEntryType.RANGE_ID,
        ),
        expected_timestamp_entry(
            "timestamp",
            PayloadEntryFlag.RANGE_END,
            offset=8,
        ),
    ],
}


def test_generic_schema(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        dt = np.dtype([("a", "i8"), ("b", "f8")])
        schema = domain.get_schema(dt)
        # Generic structured schemas nest: each field dtype registers as a
        # single-entry sub-schema, which the struct's entries reference by
        # schema ID in place of a predefined entry type.
        int64_schema = verify_schema_register(
            nvtxw_events,
            num_entries=1,
            domain_id=domain_id,
            static_size=8,
            entries=[
                ExpectedSchemaEntry(name="int64", entry_type=PayloadEntryType.INT64)
            ],
        )
        float64_schema = verify_schema_register(
            nvtxw_events,
            num_entries=1,
            static_size=8,
            entries=[
                ExpectedSchemaEntry(name="float64", entry_type=PayloadEntryType.FLOAT64)
            ],
        )
        schema_id = verify_schema_register(
            nvtxw_events,
            num_entries=2,
            schema_flags=0,
            domain_id=domain_id,
            schema_type=PayloadSchemaType.STATIC,
            static_size=16,
            entries=[
                ExpectedSchemaEntry(name="a", entry_type=int64_schema),
                ExpectedSchemaEntry(name="b", entry_type=float64_schema, offset=8),
            ],
        )
        assert schema.schema_id == schema_id
        assert schema.dtype == dt
        assert schema.kind is None


@pytest.mark.parametrize("kind", list(MINIMAL_EVENT_FIELDS), ids=lambda kind: kind.name)
def test_event_schema_per_kind(nvtxw_events, backend, kind):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        fields = MINIMAL_EVENT_FIELDS[kind]
        dt = np.dtype(fields)
        schema = domain.get_schema(dt, kind=kind)
        schema_id = verify_schema_register(
            nvtxw_events,
            num_entries=len(fields),
            schema_flags=EVENT_KIND_FLAGS[kind],
            domain_id=domain_id,
            schema_type=PayloadSchemaType.STATIC,
            static_size=dt.itemsize,
            entries=MINIMAL_EVENT_ENTRIES[kind],
        )
        assert schema.schema_id == schema_id
        assert schema.kind is kind


def test_registered_string_entry_type(nvtxw_events, backend):
    message_dtype = nvtx.numpy_dtype(
        np.uintp,
        entry_kind=nvtx.EntryKind.MESSAGE,
        entry_type=nvtx.PayloadEntryType.REGISTERED_STRING,
    )
    dt = np.dtype([("timestamp", TS_MARK), ("message", message_dtype)])

    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        schema = domain.get_schema(dt, kind=_MARK)
        schema_id = verify_schema_register(
            nvtxw_events,
            num_entries=2,
            schema_flags=EVENT_KIND_FLAGS[_MARK],
            domain_id=domain_id,
            schema_type=PayloadSchemaType.STATIC,
            static_size=dt.itemsize,
            entries=[
                expected_timestamp_entry("timestamp", PayloadEntryFlag.MARK),
                ExpectedSchemaEntry(
                    name="message",
                    entry_type=PayloadEntryType.REGISTERED_STRING,
                    flags=PayloadEntryFlag.MESSAGE,
                    offset=dt.fields["message"][1],
                ),
            ],
        )
        assert schema.schema_id == schema_id


def test_registered_string_entry_type_requires_uintp():
    with pytest.raises(TypeError, match="pointer-sized unsigned integer"):
        nvtx.numpy_dtype(
            np.intp,
            entry_type=nvtx.PayloadEntryType.REGISTERED_STRING,
        )


def test_schema_caching(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        dt = np.dtype(MINIMAL_EVENT_FIELDS[_MARK])
        schema = domain.get_schema(dt, kind=_MARK)
        verify_schema_register(nvtxw_events)
        assert domain.get_schema(dt, kind=_MARK) is schema
        # The same dtype without an event kind is a distinct generic schema
        # (registered nested: a field sub-schema plus the struct itself).
        generic = domain.get_schema(dt)
        verify_schema_register(nvtxw_events, num_entries=1)
        verify_schema_register(nvtxw_events, schema_flags=0)
        assert generic is not schema


def test_kind_type_error(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        with pytest.raises(TypeError, match="EventKind"):
            domain.get_schema("i8", kind=1)


@pytest.mark.parametrize(
    "kind,dtype_arg,exception,match",
    [
        pytest.param(_MARK, "i8", TypeError, "structured", id="not-structured"),
        pytest.param(
            _MARK,
            [("t", TS_MARK), ("obj", object)],
            TypeError,
            "managed storage",
            id="object-field",
        ),
        pytest.param(
            _MARK,
            [("t", nvtx.numpy_dtype(">i8", entry_kind=nvtx.EntryKind.MARK))],
            TypeError,
            "native byte order",
            id="non-native-role",
        ),
        pytest.param(
            _MARK,
            [("t", TS_MARK), ("arr", ">f4", (2,))],
            TypeError,
            "native byte order",
            id="non-native-subarray",
        ),
        pytest.param(
            _MARK, [("v", "i8")], ValueError, "exactly one", id="missing-role"
        ),
        pytest.param(
            _MARK,
            [("t", TS_MARK), ("t2", TS_MARK)],
            ValueError,
            "exactly one",
            id="duplicate-role",
        ),
        pytest.param(
            _MARK,
            [("t", TS_MARK), ("b", TS_BEGIN)],
            ValueError,
            "not valid",
            id="role-invalid-for-kind",
        ),
        pytest.param(
            _MARK,
            [("t", TS_MARK), ("ct", TS_COUNTER)],
            ValueError,
            "not valid",
            id="counter-timestamp-in-event",
        ),
        pytest.param(
            nvtx.EventKind.RANGE_PUSHPOP,
            [("b", TS_BEGIN)],
            ValueError,
            "exactly one",
            id="missing-end-role",
        ),
        pytest.param(
            nvtx.EventKind.RANGE_START,
            [("t", TS_BEGIN)],
            ValueError,
            "RANGE_ID",
            id="missing-range-id",
        ),
        pytest.param(
            nvtx.EventKind.RANGE_START,
            [
                (
                    "range_id",
                    nvtx.numpy_dtype(
                        np.uint64,
                        entry_kind=nvtx.EntryKind.RANGE_BEGIN,
                        entry_type=nvtx.PayloadEntryType.RANGE_ID,
                    ),
                )
            ],
            ValueError,
            r"cannot combine the EntryKind\.RANGE_BEGIN timestamp role "
            r"with PayloadEntryType\.RANGE_ID",
            id="range-id-is-also-timestamp",
        ),
        pytest.param(
            _MARK,
            [("t", TS_MARK), ("rid", RANGE_ID)],
            ValueError,
            "RANGE_ID",
            id="range-id-invalid-for-kind",
        ),
        pytest.param(
            nvtx.EventKind.RANGE_END,
            [("r1", RANGE_ID), ("r2", RANGE_ID), ("t", TS_END)],
            ValueError,
            "exactly one",
            id="duplicate-range-id",
        ),
    ],
)
def test_event_schema_validation(
    nvtxw_events, backend, kind, dtype_arg, exception, match
):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        with pytest.raises(exception, match=match):
            domain.get_schema(dtype_arg, kind=kind)
