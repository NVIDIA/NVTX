# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Counter registration and write tests for nvtx.writer."""

import struct

import pytest

import nvtx
from nvtx import (
    CounterInterpolation,
    CounterSemantics,
    CounterValueType,
)

from .conftest import (
    BatchFlag,
    CounterFlag,
    CounterLimitType,
    CounterNoValueReason,
    ExpectedSchemaEntry,
    PayloadEntryFlag,
    PayloadEntryType,
    PayloadSchemaFlag,
    PayloadSchemaType,
    TS_COUNTER,
    TS_MARK,
    WriterEventKind,
    begin_session,
    expected_timestamp_entry,
    np,
    open_stream,
    verify_counter_batch_write,
    verify_counter_no_value_write,
    verify_counter_register,
    verify_counter_write,
    verify_domain_register,
    verify_schema_register,
    verify_scope_register,
)

requires_numpy = pytest.mark.skipif(np is None, reason="requires numpy")

SCALAR_CASES = [
    pytest.param(int, PayloadEntryType.INT64, "<q", 42, id="int"),
    pytest.param(float, PayloadEntryType.FLOAT64, "<d", 2.5, id="float"),
]


@pytest.mark.parametrize("dtype,schema_id,fmt,value", SCALAR_CASES)
def test_scalar_counter_uses_predefined_schema(
    nvtxw_events, backend, dtype, schema_id, fmt, value
):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        counter = domain.get_counter("scalar", dtype)
        # No SchemaRegister call: the predefined entry-type ID is used.
        counter_id = verify_counter_register(
            nvtxw_events,
            name="scalar",
            schema_id=schema_id,
            scope_id=0,
            domain_id=domain_id,
        )
        assert counter.counter_id == counter_id
        assert counter.name == "scalar"
        assert counter.description is None
        assert domain.get_counter("scalar", dtype) is counter


@pytest.mark.parametrize("dtype,schema_id,fmt,value", SCALAR_CASES)
def test_write_counter_sample_scalar(
    nvtxw_events, backend, dtype, schema_id, fmt, value
):
    with open_stream(backend, nvtxw_events) as env:
        counter = env.domain.get_counter("c", dtype)
        counter_id = verify_counter_register(
            nvtxw_events, name="c", schema_id=schema_id
        )
        env.stream.write_counter_sample(counter, 100, value)
        data = verify_counter_write(
            nvtxw_events, counter_id, 100, stream_id=env.stream_id
        )
        assert data == struct.pack(fmt, value)


@requires_numpy
def test_numpy_scalar_dtypes_use_predefined_schemas(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        int_counter = domain.get_counter("i", np.int64)
        verify_counter_register(
            nvtxw_events, name="i", schema_id=PayloadEntryType.INT64
        )
        float_counter = domain.get_counter("f", "f8")
        verify_counter_register(
            nvtxw_events, name="f", schema_id=PayloadEntryType.FLOAT64
        )
        # Plain int/float normalize to the same registrations.
        assert domain.get_counter("i", int) is int_counter
        assert domain.get_counter("f", float) is float_counter


@requires_numpy
def test_other_scalar_dtype_registers_schema(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        counter = domain.get_counter("u32", np.uint32)
        schema_id = verify_schema_register(
            nvtxw_events,
            num_entries=1,
            schema_flags=0,
            domain_id=domain_id,
            schema_type=PayloadSchemaType.STATIC,
            static_size=4,
            entries=[
                ExpectedSchemaEntry(name="uint32", entry_type=PayloadEntryType.UINT32)
            ],
        )
        counter_id = verify_counter_register(
            nvtxw_events, name="u32", schema_id=schema_id
        )
        assert counter.counter_id == counter_id


@requires_numpy
def test_counter_group(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        scope = domain.get_scope("gpu0")
        scope_id = verify_scope_register(nvtxw_events, "gpu0")
        dt = np.dtype([("used", "u8"), ("free", "u8")])
        counter = domain.get_counter("mem", dt, description="memory pools", scope=scope)
        schema_id = verify_schema_register(
            nvtxw_events,
            num_entries=2,
            schema_flags=PayloadSchemaFlag.COUNTER_GROUP,
            domain_id=domain_id,
            schema_type=PayloadSchemaType.STATIC,
            static_size=16,
            entries=[
                ExpectedSchemaEntry(
                    name="used",
                    entry_type=PayloadEntryType.UINT64,
                    flags=PayloadEntryFlag.COUNTER,
                ),
                ExpectedSchemaEntry(
                    name="free",
                    entry_type=PayloadEntryType.UINT64,
                    flags=PayloadEntryFlag.COUNTER,
                    offset=8,
                ),
            ],
        )
        verify_counter_register(
            nvtxw_events,
            name="mem",
            schema_id=schema_id,
            description="memory pools",
            scope_id=scope_id,
        )
        assert counter.dtype == dt
        assert counter.description == "memory pools"


def test_counter_semantics(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        semantics = CounterSemantics(
            unit="bytes",
            value_type=CounterValueType.DELTA,
            interpolation=CounterInterpolation.LINEAR,
            min=0,
            max=1024,
            unit_scale_numerator=3,
            unit_scale_denominator=4,
        )
        counter = domain.get_counter("sem", int, semantics=semantics)
        event = next(nvtxw_events)
        assert event.kind == WriterEventKind.COUNTER_REGISTER
        recorded = event.counter_semantics
        assert recorded.has_counter
        assert recorded.counter_flags == (
            CounterFlag.VALUE_TYPE_DELTA
            | CounterFlag.INTERPOLATION_LINEAR
            | CounterFlag.LIMIT_MIN
            | CounterFlag.LIMIT_MAX
        )
        assert recorded.counter_unit == "bytes"
        assert recorded.counter_unit_scale_numerator == 3
        assert recorded.counter_unit_scale_denominator == 4
        assert recorded.counter_limit_type == CounterLimitType.U64
        assert recorded.counter_min_u64 == 0
        assert recorded.counter_max_u64 == 1024
        assert counter.semantics is semantics


def test_counter_semantics_limit_types(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        domain.get_counter("signed", int, semantics=CounterSemantics(min=-5, max=5))
        event = next(nvtxw_events)
        assert event.kind == WriterEventKind.COUNTER_REGISTER
        assert event.counter_semantics.counter_limit_type == CounterLimitType.I64
        assert event.counter_semantics.counter_min_i64 == -5
        assert event.counter_semantics.counter_max_i64 == 5

        domain.get_counter(
            "floaty", float, semantics=CounterSemantics(min=0.5, max=2.5)
        )
        event = next(nvtxw_events)
        assert event.kind == WriterEventKind.COUNTER_REGISTER
        assert event.counter_semantics.counter_limit_type == CounterLimitType.F64
        assert event.counter_semantics.counter_min_f64 == 0.5
        assert event.counter_semantics.counter_max_f64 == 2.5


@requires_numpy
def test_counter_group_field_semantics(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        dt = np.dtype(
            [
                (
                    "time",
                    nvtx.numpy_dtype(
                        "i8", counter_semantics=CounterSemantics(unit="ms")
                    ),
                ),
            ]
        )
        domain.get_counter("g", dt)
        event = next(nvtxw_events)
        assert event.kind == WriterEventKind.SCHEMA_REGISTER
        entry = event.schema_entries[0]
        assert entry.semantics.has_counter
        assert entry.semantics.counter_unit == "ms"
        verify_counter_register(nvtxw_events, name="g", schema_id=event.schema_id)


@requires_numpy
def test_counter_layout_errors(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        with pytest.raises(ValueError, match="not valid in a counter"):
            domain.get_counter("bad", np.dtype([("t", TS_MARK)]))
        with pytest.raises(ValueError, match="at most one"):
            domain.get_counter("bad", np.dtype([("a", TS_COUNTER), ("b", TS_COUNTER)]))
        with pytest.raises(TypeError, match="native byte order"):
            domain.get_counter("bad", np.dtype([("v", ">i8")]))
        with pytest.raises(TypeError, match="native byte order"):
            domain.get_counter("bad", np.dtype(">i8"))


@requires_numpy
def test_top_level_fixed_size_array_counter_is_rejected(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        dtype = np.dtype((np.float64, (1,)))

        with pytest.raises(
            TypeError,
            match="Top-level fixed-size array counter dtypes are not supported",
        ):
            domain.get_counter("bad", dtype)


def test_counter_scope_errors(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain_a = env.session.get_domain("A")
        verify_domain_register(nvtxw_events, "A")
        domain_b = env.session.get_domain("B")
        verify_domain_register(nvtxw_events, "B")
        scope = domain_b.get_scope("s")
        verify_scope_register(nvtxw_events, "s")
        with pytest.raises(ValueError, match="not registered in this"):
            domain_a.get_counter("c", int, scope=scope)
        with pytest.raises(ValueError, match="static scope ID range"):
            domain_a.get_counter("c", int, scope=7)
        with pytest.raises(TypeError, match="PredefinedScope"):
            domain_a.get_counter("c", int, scope="s")


@pytest.mark.parametrize(
    "scope,scope_id",
    [
        (nvtx.PredefinedScope.CURRENT_SW_THREAD, 22),
        (1 << 24, 1 << 24),
    ],
    ids=["predefined", "static"],
)
def test_counter_predefined_and_static_scopes(nvtxw_events, backend, scope, scope_id):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        domain.get_counter("c", int, scope=scope)
        verify_counter_register(
            nvtxw_events,
            name="c",
            schema_id=PayloadEntryType.INT64,
            scope_id=scope_id,
            domain_id=domain_id,
        )


@requires_numpy
def test_write_counter_sample_group(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        dt = np.dtype([("a", "i8"), ("b", "f8")])
        counter = env.domain.get_counter("g", dt)
        schema_id = verify_schema_register(nvtxw_events)
        counter_id = verify_counter_register(
            nvtxw_events, name="g", schema_id=schema_id
        )
        env.stream.write_counter_sample(counter, 50, (7, 1.5))
        data = verify_counter_write(
            nvtxw_events, counter_id, 50, stream_id=env.stream_id
        )
        assert data == struct.pack("<qd", 7, 1.5)
        with pytest.raises(ValueError, match="one sample"):
            env.stream.write_counter_sample(counter, 51, [(1, 1.0), (2, 2.0)])


def test_write_counter_sample_domain_mismatch(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        other = env.session.get_domain("other")
        verify_domain_register(nvtxw_events, "other")
        counter = other.get_counter("c", int)
        verify_counter_register(nvtxw_events, name="c")
        with pytest.raises(ValueError, match="stream's domain"):
            env.stream.write_counter_sample(counter, 1, 2)


@requires_numpy
def test_write_counter_sample_rejects_embedded_timestamp(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        dt = np.dtype([("t", TS_COUNTER), ("v", "f8")])
        counter = env.domain.get_counter("e", dt)
        schema_id = verify_schema_register(
            nvtxw_events,
            num_entries=2,
            schema_flags=PayloadSchemaFlag.COUNTER_GROUP,
            domain_id=env.domain_id,
            schema_type=PayloadSchemaType.STATIC,
            static_size=16,
            entries=[
                expected_timestamp_entry("t"),
                ExpectedSchemaEntry(
                    name="v",
                    entry_type=PayloadEntryType.FLOAT64,
                    flags=PayloadEntryFlag.COUNTER,
                    offset=8,
                ),
            ],
        )
        verify_counter_register(nvtxw_events, name="e", schema_id=schema_id)
        with pytest.raises(ValueError, match="write_counter_batch"):
            env.stream.write_counter_sample(counter, 1, (1, 2.0))


def test_write_counter_no_value(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        counter = env.domain.get_counter("c", int)
        counter_id = verify_counter_register(nvtxw_events, name="c")
        env.stream.write_counter_sample_no_value(
            counter, 5, nvtx.CounterNoValueReason.UNAVAILABLE
        )
        verify_counter_no_value_write(
            nvtxw_events,
            counter_id,
            5,
            CounterNoValueReason.UNAVAILABLE,
            stream_id=env.stream_id,
        )
        with pytest.raises(TypeError, match="CounterNoValueReason"):
            env.stream.write_counter_sample_no_value(counter, 6, 2)


@requires_numpy
@pytest.mark.parametrize(
    "iterable_factory",
    [
        pytest.param(list, id="materialized"),
        pytest.param(iter, id="one-shot"),
    ],
)
def test_counter_batch_separate_timestamps(
    nvtxw_events, backend, iterable_factory
):
    with open_stream(backend, nvtxw_events) as env:
        counter = env.domain.get_counter("c", int)
        counter_id = verify_counter_register(nvtxw_events, name="c")
        data = iterable_factory([1, 2, 3])
        timestamps = iterable_factory([10, 20, 30])
        env.stream.write_counter_batch(counter, data, timestamps=timestamps)
        data = verify_counter_batch_write(
            nvtxw_events,
            counter_id,
            timestamps=(10, 20, 30),
            stream_id=env.stream_id,
        )
        assert data == struct.pack("<3q", 1, 2, 3)


@requires_numpy
def test_counter_batch_embedded_timestamps(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        dt = np.dtype([("t", TS_COUNTER), ("v", "f8")])
        counter = env.domain.get_counter("e", dt)
        schema_id = verify_schema_register(nvtxw_events)
        counter_id = verify_counter_register(
            nvtxw_events, name="e", schema_id=schema_id
        )
        env.stream.write_counter_batch(counter, [(10, 1.5), (20, 2.5)])
        data = verify_counter_batch_write(nvtxw_events, counter_id, timestamps=())
        assert data == struct.pack("<qdqd", 10, 1.5, 20, 2.5)


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
@requires_numpy
def test_counter_batch_ordering_flag(nvtxw_events, backend, ordering, flags):
    with open_stream(backend, nvtxw_events) as env:
        counter = env.domain.get_counter("c", int)
        counter_id = verify_counter_register(nvtxw_events, name="c")
        env.stream.write_counter_batch(
            counter,
            [1],
            timestamps=[1],
            ordering=ordering,
        )
        verify_counter_batch_write(nvtxw_events, counter_id, flags=flags)


@requires_numpy
def test_counter_batch_dataframe(nvtxw_events, backend):
    pd = pytest.importorskip("pandas")
    with open_stream(backend, nvtxw_events) as env:
        dt = np.dtype([("a", "i8"), ("b", "f8")])
        counter = env.domain.get_counter("g", dt)
        schema_id = verify_schema_register(nvtxw_events)
        counter_id = verify_counter_register(
            nvtxw_events, name="g", schema_id=schema_id
        )
        frame = pd.DataFrame({"a": [1, 2], "b": [0.5, 1.5]})
        env.stream.write_counter_batch(counter, frame, timestamps=[10, 20])
        data = verify_counter_batch_write(nvtxw_events, counter_id, timestamps=(10, 20))
        assert data == struct.pack("<qdqd", 1, 0.5, 2, 1.5)


@requires_numpy
def test_counter_batch_errors(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        scalar = env.domain.get_counter("s", int)
        verify_counter_register(nvtxw_events, name="s")
        dt = np.dtype([("t", TS_COUNTER), ("v", "f8")])
        embedded = env.domain.get_counter("e", dt)
        verify_schema_register(nvtxw_events)
        verify_counter_register(nvtxw_events, name="e")

        with pytest.raises(ValueError, match="requires timestamps"):
            env.stream.write_counter_batch(scalar, [1, 2])
        with pytest.raises(ValueError, match="mutually exclusive"):
            env.stream.write_counter_batch(embedded, [(1, 1.0)], timestamps=[1])
        with pytest.raises(ValueError, match="same length"):
            env.stream.write_counter_batch(scalar, [1, 2], timestamps=[1])
        with pytest.raises(ValueError, match="at least one"):
            env.stream.write_counter_batch(scalar, [], timestamps=[])
        with pytest.raises(ValueError, match="one-dimensional"):
            env.stream.write_counter_batch(scalar, np.zeros((2, 2)), timestamps=[1, 2])
        with pytest.raises(ValueError, match="one-dimensional"):
            env.stream.write_counter_batch(
                scalar,
                [1, 2],
                timestamps=np.zeros((2, 2), dtype=np.int64),
            )
        with pytest.raises(TypeError, match="BatchOrdering"):
            env.stream.write_counter_batch(scalar, [1], timestamps=[1], ordering=0)
