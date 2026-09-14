# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Schema-based write_event / write_event_batch tests for nvtx.writer."""

import struct

import pytest

import nvtx

from .conftest import (
    BatchFlag,
    EVENT_KIND_FLAGS,
    RANGE_ID,
    TS_BEGIN,
    TS_END,
    TS_MARK,
    begin_session,
    open_stream,
    verify_domain_register,
    verify_event_batch_write,
    verify_event_write,
    verify_schema_register,
    verify_scope_register,
    verify_stream_close,
    verify_stream_open,
    verify_string_register,
)

np = pytest.importorskip("numpy")

_MARK = nvtx.EventKind.MARK

# One minimal payload per event kind: (fields, row, struct format).
EVENT_CASES = {
    nvtx.EventKind.MARK: (
        [("t", TS_MARK), ("value", "u4")],
        (100, 7),
        "<qI",
    ),
    nvtx.EventKind.RANGE_PUSH: ([("t", TS_BEGIN)], (100,), "<q"),
    nvtx.EventKind.RANGE_POP: ([("t", TS_END)], (200,), "<q"),
    nvtx.EventKind.RANGE_PUSHPOP: (
        [("b", TS_BEGIN), ("e", TS_END)],
        (100, 200),
        "<qq",
    ),
    nvtx.EventKind.RANGE_STARTEND: (
        [("b", TS_BEGIN), ("e", TS_END)],
        (100, 200),
        "<qq",
    ),
    nvtx.EventKind.RANGE_START: (
        [("rid", RANGE_ID), ("t", TS_BEGIN)],
        (5, 100),
        "<Qq",
    ),
    nvtx.EventKind.RANGE_END: (
        [("rid", RANGE_ID), ("t", TS_END)],
        (5, 200),
        "<Qq",
    ),
}


def _event_schema(env, events, kind):
    """Register the kind's schema and return (schema, row, packed row)."""
    fields, row, fmt = EVENT_CASES[kind]
    schema = env.domain.get_schema(np.dtype(fields), kind=kind)
    verify_schema_register(
        events,
        schema_flags=EVENT_KIND_FLAGS[kind],
        domain_id=env.domain_id,
    )
    return schema, row, struct.pack(fmt, *row)


@pytest.mark.parametrize("kind", list(EVENT_CASES), ids=lambda kind: kind.name)
def test_write_event_per_kind(nvtxw_events, backend, kind):
    with open_stream(backend, nvtxw_events) as env:
        schema, row, packed = _event_schema(env, nvtxw_events, kind)
        env.stream.write_event(schema, row)
        payload = verify_event_write(
            nvtxw_events, schema.schema_id, stream_id=env.stream_id
        )
        assert payload == packed


def test_write_event_row_forms(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        schema, row, packed = _event_schema(env, nvtxw_events, _MARK)
        rows = np.array([row], dtype=schema.dtype)
        for form in (row, rows[0], rows[:1]):
            env.stream.write_event(schema, form)
            payload = verify_event_write(nvtxw_events, schema.schema_id)
            assert payload == packed


def test_write_event_registered_string(nvtxw_events, backend):
    message_dtype = nvtx.numpy_dtype(
        np.uintp,
        entry_kind=nvtx.EntryKind.MESSAGE,
        entry_type=nvtx.PayloadEntryType.REGISTERED_STRING,
    )
    dt = np.dtype([("timestamp", TS_MARK), ("message", message_dtype)])

    with open_stream(backend, nvtxw_events) as env:
        registered = env.domain.get_registered_string("hello")
        handle = verify_string_register(nvtxw_events, "hello")
        assert registered.handle == handle

        schema = env.domain.get_schema(dt, kind=_MARK)
        verify_schema_register(
            nvtxw_events,
            schema_flags=EVENT_KIND_FLAGS[_MARK],
            domain_id=env.domain_id,
        )
        row = (100, registered.handle)
        env.stream.write_event(schema, row)
        payload = verify_event_write(
            nvtxw_events, schema.schema_id, stream_id=env.stream_id
        )
        assert payload == np.asarray(row, dtype=dt).tobytes()


def test_write_event_errors(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        generic = env.domain.get_schema(np.dtype([("v", "i8")]))
        # Generic schemas register nested: field sub-schema plus the struct.
        verify_schema_register(nvtxw_events, num_entries=1)
        verify_schema_register(nvtxw_events, schema_flags=0)
        with pytest.raises(ValueError, match="event schema"):
            env.stream.write_event(generic, (1,))

        schema, row, _ = _event_schema(env, nvtxw_events, _MARK)
        with pytest.raises(ValueError, match="one row"):
            env.stream.write_event(schema, [row, row])
        with pytest.raises(TypeError):
            env.stream.write_event(None, row)

        other = env.session.get_domain("other")
        verify_domain_register(nvtxw_events, "other")
        foreign = other.get_schema(schema.dtype, kind=_MARK)
        verify_schema_register(nvtxw_events)
        with pytest.raises(ValueError, match="domain"):
            env.stream.write_event(foreign, row)


def test_write_event_batch_ndarray(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        schema, _, _ = _event_schema(env, nvtxw_events, nvtx.EventKind.RANGE_PUSHPOP)
        rows = np.array([(10, 20), (30, 40)], dtype=schema.dtype)
        env.stream.write_event_batch(schema, rows)
        data = verify_event_batch_write(
            nvtxw_events,
            schema.schema_id,
            stream_id=env.stream_id,
            scope_id=0,
            flags=BatchFlag.SORTED,
        )
        assert data == rows.tobytes()


def test_write_event_batch_iterable(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        schema, _, _ = _event_schema(env, nvtxw_events, nvtx.EventKind.RANGE_PUSHPOP)
        env.stream.write_event_batch(schema, [(10, 20), (30, 40)])
        data = verify_event_batch_write(nvtxw_events, schema.schema_id)
        assert data == struct.pack("<4q", 10, 20, 30, 40)


@pytest.mark.parametrize(
    "ordering,flags",
    [
        (nvtx.BatchOrdering.SORTED, BatchFlag.SORTED),
        (nvtx.BatchOrdering.PARTIALLY_SORTED, BatchFlag.PARTIALLY_SORTED),
        (nvtx.BatchOrdering.SORTED_PER_SCOPE, BatchFlag.SORTED_PER_SCOPE),
        (nvtx.BatchOrdering.UNSORTED, BatchFlag.UNSORTED),
    ],
    ids=lambda value: value if isinstance(value, int) else value.name,
)
def test_write_event_batch_ordering(nvtxw_events, backend, ordering, flags):
    with open_stream(backend, nvtxw_events) as env:
        schema, row, _ = _event_schema(env, nvtxw_events, _MARK)
        env.stream.write_event_batch(schema, [row], ordering=ordering)
        verify_event_batch_write(nvtxw_events, schema.schema_id, flags=flags)


def test_write_event_batch_uses_stream_scope(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        scope = domain.get_scope("node")
        scope_id = verify_scope_register(nvtxw_events, "node")
        stream = env.session.create_stream("scoped", domain=domain, scope=scope)
        with stream:
            stream_id = verify_stream_open(
                nvtxw_events,
                "scoped",
                domain_id=domain_id,
                scope_id=scope_id,
            )
            schema = domain.get_schema(np.dtype(EVENT_CASES[_MARK][0]), kind=_MARK)
            verify_schema_register(nvtxw_events)
            stream.write_event_batch(schema, [EVENT_CASES[_MARK][1]])
            verify_event_batch_write(
                nvtxw_events,
                schema.schema_id,
                stream_id=stream_id,
                scope_id=scope_id,
            )
        verify_stream_close(nvtxw_events, stream_id)


def test_write_event_batch_dataframe(nvtxw_events, backend):
    pd = pytest.importorskip("pandas")
    with open_stream(backend, nvtxw_events) as env:
        schema, _, _ = _event_schema(env, nvtxw_events, nvtx.EventKind.RANGE_PUSHPOP)
        frame = pd.DataFrame({"b": [10, 30], "e": [20, 40]})
        env.stream.write_event_batch(schema, frame)
        data = verify_event_batch_write(nvtxw_events, schema.schema_id)
        assert data == struct.pack("<4q", 10, 20, 30, 40)

        with pytest.raises(ValueError, match="missing a column"):
            env.stream.write_event_batch(schema, frame[["b"]])


def test_write_event_batch_errors(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        schema, row, _ = _event_schema(env, nvtxw_events, _MARK)
        with pytest.raises(ValueError, match="at least one"):
            env.stream.write_event_batch(schema, [])
        with pytest.raises(ValueError, match="one-dimensional"):
            env.stream.write_event_batch(schema, np.zeros((2, 2), dtype=schema.dtype))
        with pytest.raises(TypeError, match="BatchOrdering"):
            env.stream.write_event_batch(schema, [row], ordering=0)

        push_schema, push_row, _ = _event_schema(
            env, nvtxw_events, nvtx.EventKind.RANGE_PUSH
        )
        with pytest.raises(ValueError, match="complete-event"):
            env.stream.write_event_batch(push_schema, [push_row])
