# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Session lifecycle tests for nvtx.writer."""

import pytest

import nvtx.writer
from nvtx.writer import WriterError

from .conftest import (
    WriterResultCode,
    assert_failed,
    begin_session,
    verify_domain_register,
    verify_session_begin,
    verify_session_end,
    verify_stream_close,
    verify_stream_open,
)


@pytest.mark.parametrize(
    "name,expected_name",
    [("sess", "sess"), (b"sess", "sess"), (None, None)],
)
@pytest.mark.parametrize("config", [None, "option=value\n"])
def test_begin_end(nvtxw_events, backend, name, expected_name, config):
    session = nvtx.writer.Session(name, backend=backend, config=config)
    session.begin()
    session_id = verify_session_begin(nvtxw_events, expected_name, config)
    session.end()
    verify_session_end(nvtxw_events, session_id)


def test_context_manager(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events, "ctx") as env:
        pass  # Assertions are in ``begin_session`` fixture.


def test_context_manager_ends_session_on_error(nvtxw_events, backend):
    with pytest.raises(RuntimeError, match="boom"):
        with begin_session(backend, nvtxw_events, "err"):
            raise RuntimeError("boom")


def test_double_begin(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events, "s") as env:
        with pytest.raises(RuntimeError, match="already active"):
            env.session.begin()


def test_session_begin_failure_recovers_cleanly(
    nvtxw_events, backend, backend_controls
):
    session = nvtx.writer.Session("retry", backend=backend)
    backend_controls.set_next_result(WriterResultCode.FAILED)
    with pytest.raises(WriterError) as excinfo:
        session.begin()
    assert_failed(excinfo, "SessionBegin", WriterResultCode.FAILED)

    session.begin()
    session_id = verify_session_begin(nvtxw_events, "retry")
    session.end()
    verify_session_end(nvtxw_events, session_id)


def test_end_requires_active_session(backend):
    session = nvtx.writer.Session("s", backend=backend)
    with pytest.raises(RuntimeError, match="not active"):
        session.end()


def test_end_twice(nvtxw_events, backend):
    session = nvtx.writer.Session("s", backend=backend)
    session.begin()
    session_id = verify_session_begin(nvtxw_events, "s")
    session.end()
    verify_session_end(nvtxw_events, session_id)
    with pytest.raises(RuntimeError, match="not active"):
        session.end()


def test_operations_require_active_session(backend):
    session = nvtx.writer.Session("s", backend=backend)
    with pytest.raises(RuntimeError, match="not active"):
        session.get_domain()
    with pytest.raises(RuntimeError, match="not active"):
        session.create_stream("stream")


def test_session_is_reusable(nvtxw_events, backend):
    session = nvtx.writer.Session("again", backend=backend)
    for _ in range(2):
        session.begin()
        session_id = verify_session_begin(nvtxw_events, "again")
        session.end()
        verify_session_end(nvtxw_events, session_id)


def test_end_closes_open_streams(nvtxw_events, backend):
    session = nvtx.writer.Session("s", backend=backend)
    session.begin()
    session_id = verify_session_begin(nvtxw_events, "s")
    domain = session.get_domain()
    domain_id = verify_domain_register(nvtxw_events, None)
    stream_ids = set()
    for name in ("one", "two"):
        stream = session.create_stream(name, domain=domain)
        stream.open()
        stream_ids.add(verify_stream_open(nvtxw_events, name, domain_id=domain_id))
    session.end()
    # Streams close in unspecified order, before the session ends.
    closed = {
        verify_stream_close(nvtxw_events),
        verify_stream_close(nvtxw_events),
    }
    assert closed == stream_ids
    verify_session_end(nvtxw_events, session_id)


def test_session_end_continues_after_stream_close_failure(
    nvtxw_events, backend, backend_controls
):
    session = nvtx.writer.Session("best effort", backend=backend)
    session.begin()
    session_id = verify_session_begin(nvtxw_events, "best effort")

    stream_ids = set()
    domain_id = None
    for name in ("one", "two"):
        stream = session.create_stream(name)
        stream.open()
        if domain_id is None:
            domain_id = verify_domain_register(nvtxw_events, session_id=session_id)
        stream_ids.add(
            verify_stream_open(
                nvtxw_events,
                name,
                domain_id=domain_id,
                session_id=session_id,
            )
        )

    backend_controls.set_next_result(WriterResultCode.FAILED)
    with pytest.raises(WriterError) as excinfo:
        session.end()
    assert_failed(excinfo, "StreamClose", WriterResultCode.FAILED)

    assert verify_stream_close(nvtxw_events) in stream_ids
    verify_session_end(nvtxw_events, session_id)
    with pytest.raises(RuntimeError, match="not active"):
        session.end()


def test_end_invalidates_domains(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events, "s") as env:
        domain = env.session.get_domain("D")
        verify_domain_register(nvtxw_events, "D")
    with pytest.raises(RuntimeError, match="no longer valid"):
        domain.get_registered_string("x")
    with pytest.raises(RuntimeError, match="no longer valid"):
        domain.get_category_id("x")
    with pytest.raises(RuntimeError, match="no longer valid"):
        domain.get_scope("x")
    with pytest.raises(RuntimeError, match="no longer valid"):
        domain.get_counter("x", int)


def test_exit_tolerates_backend_close(nvtxw_events, backend):
    with nvtx.writer.Session("s", backend=backend):
        session_id = verify_session_begin(nvtxw_events, "s")
        backend.close()
    verify_session_end(nvtxw_events, session_id)
