# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Tests for the fixed-layout mark/range write methods of nvtx.writer."""

import pytest

from nvtx.writer import WriterError

from .conftest import (
    WriterResultCode,
    assert_failed,
    begin_session,
    open_stream,
    decode_end,
    decode_pop,
    decode_range_id_reg_string,
    decode_range_id_utf8_header,
    decode_range_reg_string,
    decode_range_utf8_header,
    decode_reg_string,
    decode_utf8_header,
    verify_category_register,
    verify_domain_register,
    verify_event_schemas_registered,
    verify_event_write,
    verify_stream_close,
    verify_stream_open,
    verify_string_register,
)

# ARGB value colors.py assigns to the named color "green".
_COLOR_GREEN = 0x008000


@pytest.mark.parametrize(
    "method,utf8_schema,reg_schema",
    [
        ("write_mark", "mark_utf8", "mark_reg_string"),
        ("write_push", "range_push_utf8", "range_push_reg_string"),
    ],
)
def test_point_event_message_forms(
    nvtxw_events, backend, method, utf8_schema, reg_schema
):
    with open_stream(backend, nvtxw_events) as env:
        write = getattr(env.stream, method)

        # No message: the UTF-8 layout with an empty message payload.
        write(11)
        schemas = verify_event_schemas_registered(nvtxw_events)
        payload = verify_event_write(
            nvtxw_events,
            schemas[utf8_schema],
            message=b"\x00",
            message_schema_id=schemas["event_message_utf8"],
            stream_id=env.stream_id,
        )
        assert decode_utf8_header(payload) == (11, 0, 0)

        # A str message: UTF-8 header plus a separate message payload.
        write(12, message="hello")
        payload = verify_event_write(
            nvtxw_events,
            schemas[utf8_schema],
            message=b"hello",
            message_schema_id=schemas["event_message_utf8"],
        )
        assert decode_utf8_header(payload) == (12, 0, 0)

        # A registered string: its handle rides in the payload.
        registered = env.domain.get_registered_string("reg")
        handle = verify_string_register(nvtxw_events, "reg")
        write(13, message=registered)
        payload = verify_event_write(nvtxw_events, schemas[reg_schema])
        assert decode_reg_string(payload) == (13, 0, 0, handle)


@pytest.mark.parametrize(
    "method,utf8_schema,reg_schema",
    [
        (
            "write_pushpop",
            "range_pushpop_utf8",
            "range_pushpop_reg_string",
        ),
        (
            "write_startend",
            "range_startend_utf8",
            "range_startend_reg_string",
        ),
    ],
)
def test_span_event_message_forms(
    nvtxw_events, backend, method, utf8_schema, reg_schema
):
    with open_stream(backend, nvtxw_events) as env:
        write = getattr(env.stream, method)

        write(11, 21)
        schemas = verify_event_schemas_registered(nvtxw_events)
        payload = verify_event_write(
            nvtxw_events,
            schemas[utf8_schema],
            message=b"\x00",
            message_schema_id=schemas["event_message_utf8"],
            stream_id=env.stream_id,
        )
        assert decode_range_utf8_header(payload) == (11, 21, 0, 0)

        write(12, 22, message="hello")
        payload = verify_event_write(
            nvtxw_events,
            schemas[utf8_schema],
            message=b"hello",
            message_schema_id=schemas["event_message_utf8"],
        )
        assert decode_range_utf8_header(payload) == (12, 22, 0, 0)

        registered = env.domain.get_registered_string("reg")
        handle = verify_string_register(nvtxw_events, "reg")
        write(13, 23, message=registered)
        payload = verify_event_write(nvtxw_events, schemas[reg_schema])
        assert decode_range_reg_string(payload) == (13, 23, 0, 0, handle)


def test_write_start_message_forms(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        env.stream.write_start(11, 7)
        schemas = verify_event_schemas_registered(nvtxw_events)
        payload = verify_event_write(
            nvtxw_events,
            schemas["range_start_utf8"],
            message=b"\x00",
            message_schema_id=schemas["event_message_utf8"],
            stream_id=env.stream_id,
        )
        assert decode_range_id_utf8_header(payload) == (7, 11, 0, 0)

        env.stream.write_start(12, 8, message="hello")
        payload = verify_event_write(
            nvtxw_events,
            schemas["range_start_utf8"],
            message=b"hello",
            message_schema_id=schemas["event_message_utf8"],
        )
        assert decode_range_id_utf8_header(payload) == (8, 12, 0, 0)

        registered = env.domain.get_registered_string("reg")
        handle = verify_string_register(nvtxw_events, "reg")
        env.stream.write_start(13, 9, message=registered)
        payload = verify_event_write(nvtxw_events, schemas["range_start_reg_string"])
        assert decode_range_id_reg_string(payload) == (9, 13, 0, 0, handle)


def test_write_pop_and_end(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        env.stream.write_pop(5)
        schemas = verify_event_schemas_registered(nvtxw_events)
        payload = verify_event_write(
            nvtxw_events, schemas["range_pop"], stream_id=env.stream_id
        )
        assert decode_pop(payload) == 5

        env.stream.write_end(6, 42)
        payload = verify_event_write(nvtxw_events, schemas["range_end"])
        assert decode_end(payload) == (42, 6)


def test_event_colors(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        env.stream.write_mark(1, color=0x11223344)
        schemas = verify_event_schemas_registered(nvtxw_events)
        payload = verify_event_write(
            nvtxw_events,
            schemas["mark_utf8"],
            message=b"\x00",
            message_schema_id=schemas["event_message_utf8"],
        )
        assert decode_utf8_header(payload) == (1, 0x11223344, 0)

        env.stream.write_mark(2, color="green")
        payload = verify_event_write(
            nvtxw_events,
            schemas["mark_utf8"],
            message=b"\x00",
            message_schema_id=schemas["event_message_utf8"],
        )
        assert decode_utf8_header(payload) == (2, _COLOR_GREEN, 0)


def test_event_categories(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        # A named category registers lazily on first use.
        env.stream.write_mark(1, category="io")
        schemas = verify_event_schemas_registered(nvtxw_events)
        verify_category_register(
            nvtxw_events, "io", category_id=1, domain_id=env.domain_id
        )
        payload = verify_event_write(
            nvtxw_events,
            schemas["mark_utf8"],
            message=b"\x00",
            message_schema_id=schemas["event_message_utf8"],
        )
        assert decode_utf8_header(payload) == (1, 0, 1)

        # A raw integer category passes through without registration...
        env.stream.write_mark(2, category=2)
        payload = verify_event_write(
            nvtxw_events,
            schemas["mark_utf8"],
            message=b"\x00",
            message_schema_id=schemas["event_message_utf8"],
        )
        assert decode_utf8_header(payload) == (2, 0, 2)

        # ...and the named-category allocator skips the claimed ID.
        assert env.domain.get_category_id("net") == 3
        verify_category_register(nvtxw_events, "net", category_id=3)

        with pytest.raises(ValueError):
            env.stream.write_mark(3, category=-1)
        with pytest.raises(ValueError):
            env.stream.write_mark(3, category=2**32)
        with pytest.raises(TypeError):
            env.stream.write_mark(3, category=1.5)


def test_message_encodings(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        env.stream.write_mark(1, message=b"raw bytes")
        schemas = verify_event_schemas_registered(nvtxw_events)
        verify_event_write(
            nvtxw_events,
            schemas["mark_utf8"],
            message=b"raw bytes",
            message_schema_id=schemas["event_message_utf8"],
        )

        # An empty message is sent as a single NUL so it stays present.
        env.stream.write_mark(2, message="")
        verify_event_write(
            nvtxw_events,
            schemas["mark_utf8"],
            message=b"\x00",
            message_schema_id=schemas["event_message_utf8"],
        )

        # Non-ASCII text is UTF-8 encoded.
        env.stream.write_mark(3, message="héllo")
        verify_event_write(
            nvtxw_events,
            schemas["mark_utf8"],
            message="héllo".encode(),
            message_schema_id=schemas["event_message_utf8"],
        )


def test_helper_schemas_register_once_per_domain(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        stream_a = env.session.create_stream("a", domain=domain)
        stream_b = env.session.create_stream("b", domain=domain)
        with stream_a, stream_b:
            id_a = verify_stream_open(nvtxw_events, "a", domain_id=domain_id)
            id_b = verify_stream_open(nvtxw_events, "b", domain_id=domain_id)
            stream_a.write_pop(1)
            schemas = verify_event_schemas_registered(nvtxw_events)
            verify_event_write(nvtxw_events, schemas["range_pop"], stream_id=id_a)
            # Registration is per domain, not per write or per stream.
            stream_a.write_pop(2)
            verify_event_write(nvtxw_events, schemas["range_pop"], stream_id=id_a)
            stream_b.write_pop(3)
            verify_event_write(nvtxw_events, schemas["range_pop"], stream_id=id_b)
        verify_stream_close(nvtxw_events, id_b)
        verify_stream_close(nvtxw_events, id_a)

        # A second domain registers its own helper schemas.
        with env.session.create_stream("c", domain="D2") as stream_c:
            domain2_id = verify_domain_register(nvtxw_events, "D2")
            id_c = verify_stream_open(nvtxw_events, "c", domain_id=domain2_id)
            stream_c.write_pop(4)
            schemas2 = verify_event_schemas_registered(nvtxw_events)
            verify_event_write(nvtxw_events, schemas2["range_pop"], stream_id=id_c)
        verify_stream_close(nvtxw_events, id_c)


def test_event_write_failure_preserves_lazy_schemas_for_retry(
    nvtxw_events, backend, backend_controls
):
    with open_stream(backend, nvtxw_events) as env:
        env.stream.write_mark(0)
        schemas = verify_event_schemas_registered(nvtxw_events)
        verify_event_write(
            nvtxw_events,
            schemas["mark_utf8"],
            message=b"\x00",
            message_schema_id=schemas["event_message_utf8"],
            stream_id=env.stream_id,
        )

        backend_controls.set_next_result(WriterResultCode.FAILED)
        with pytest.raises(WriterError) as excinfo:
            env.stream.write_mark(1)
        assert_failed(excinfo, "nvtxwMarkWriteUtf8", WriterResultCode.FAILED)

        env.stream.write_mark(2)
        verify_event_write(
            nvtxw_events,
            schemas["mark_utf8"],
            message=b"\x00",
            message_schema_id=schemas["event_message_utf8"],
            stream_id=env.stream_id,
        )


def test_write_start_rejects_zero_range_id(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        with pytest.raises(ValueError, match="non-zero"):
            env.stream.write_start(1, 0)


def test_message_errors(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        other = env.session.get_domain("other")
        verify_domain_register(nvtxw_events, "other")
        registered = other.get_registered_string("s")
        verify_string_register(nvtxw_events, "s")
        # Consume the lazy helper-schema registration with a valid write.
        env.stream.write_mark(1)
        schemas = verify_event_schemas_registered(nvtxw_events)
        verify_event_write(
            nvtxw_events,
            schemas["mark_utf8"],
            message=b"\x00",
            message_schema_id=schemas["event_message_utf8"],
        )

        with pytest.raises(ValueError, match="different domain"):
            env.stream.write_mark(2, message=registered)
        with pytest.raises(TypeError):
            env.stream.write_mark(2, message=123)
