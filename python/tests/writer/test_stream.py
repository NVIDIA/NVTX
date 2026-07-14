# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Stream configuration and lifecycle tests for nvtx.writer."""

import pytest

import nvtx
import nvtx.writer

from .conftest import (
    StreamInterleaving,
    StreamOrdering,
    StreamSkid,
    TimestampType,
    begin_session,
    verify_domain_register,
    verify_event_schemas_registered,
    verify_event_write,
    verify_scope_register,
    verify_session_begin,
    verify_session_end,
    verify_stream_close,
    verify_stream_open,
)


def test_stream_defaults(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        with env.session.create_stream("main", domain=None):
            domain_id = verify_domain_register(nvtxw_events, session_id=env.session_id)
            stream_id = verify_stream_open(
                nvtxw_events,
                "main",
                domain_id=domain_id,
                session_id=env.session_id,
            )
        verify_stream_close(nvtxw_events, stream_id)


def test_stream_scope_uses_default_domain(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        scope = domain.get_scope("node")
        verify_scope_register(nvtxw_events, "node")
        with env.session.create_stream("scoped", scope=scope):
            stream_id = verify_stream_open(
                nvtxw_events,
                "scoped",
                domain_id=domain_id,
                scope_id=scope.scope_id,
            )
        verify_stream_close(nvtxw_events, stream_id)


def test_stream_event_helpers_use_default_domain(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        with env.session.create_stream("domainless") as stream:
            domain_id = verify_domain_register(nvtxw_events, session_id=env.session_id)
            stream_id = verify_stream_open(
                nvtxw_events, "domainless", domain_id=domain_id
            )
            stream.write_mark(1)
            schemas = verify_event_schemas_registered(nvtxw_events)
            verify_event_write(
                nvtxw_events,
                schemas["mark_utf8"],
                message=b"\x00",
                message_schema_id=schemas["event_message_utf8"],
                stream_id=stream_id,
            )
        verify_stream_close(nvtxw_events, stream_id)


def test_stream_full_configuration(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        scope = domain.get_scope("machine")
        verify_scope_register(nvtxw_events, "machine")
        with env.session.create_stream(
            "configured",
            domain=domain,
            scope=scope,
            time_domain_id=nvtx.TimestampType.CPU_TSC,
            interleaving=nvtx.writer.StreamInterleaving.SCOPE,
            ordering=nvtx.writer.StreamOrdering.STRICT,
            skid=nvtx.writer.StreamSkid.TIME_NS,
            skid_amount=1000,
        ):
            stream_id = verify_stream_open(
                nvtxw_events,
                "configured",
                domain_id=domain_id,
                scope_id=scope.scope_id,
                time_domain_id=TimestampType.CPU_TSC,
                interleaving=StreamInterleaving.SCOPE,
                ordering=StreamOrdering.STRICT,
                skid=StreamSkid.TIME_NS,
                skid_amount=1000,
            )
        verify_stream_close(nvtxw_events, stream_id)


def test_stream_scope_from_registered_scope(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        scope = domain.get_scope("node")
        scope_id = verify_scope_register(nvtxw_events, "node", domain_id=domain_id)
        stream = env.session.create_stream("scoped", domain=domain, scope=scope)
        with stream:
            verify_stream_open(
                nvtxw_events,
                "scoped",
                domain_id=domain_id,
                scope_id=scope_id,
            )
        verify_stream_close(nvtxw_events)


@pytest.mark.parametrize(
    "scope,scope_id",
    [
        (nvtx.PredefinedScope.ROOT, 1),
        (1 << 24, 1 << 24),
    ],
    ids=["predefined", "static"],
)
def test_stream_predefined_and_static_scopes(nvtxw_events, backend, scope, scope_id):
    with begin_session(backend, nvtxw_events) as env:
        with env.session.create_stream("scoped", scope=scope):
            domain_id = verify_domain_register(nvtxw_events, session_id=env.session_id)
            stream_id = verify_stream_open(
                nvtxw_events,
                "scoped",
                domain_id=domain_id,
                session_id=env.session_id,
                scope_id=scope_id,
            )
        verify_stream_close(nvtxw_events, stream_id)


def test_stream_rejects_runtime_resolved_scope(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        with pytest.raises(ValueError, match="runtime-resolved"):
            env.session.create_stream(
                "scoped", scope=nvtx.PredefinedScope.CURRENT_SW_THREAD
            )
        verify_domain_register(nvtxw_events, session_id=env.session_id)


def test_stream_integer_time_domain(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        with env.session.create_stream("timed", domain=domain, time_domain_id=42):
            verify_stream_open(
                nvtxw_events,
                "timed",
                domain_id=domain_id,
                time_domain_id=42,
            )
        verify_stream_close(nvtxw_events)


def test_stream_domain_by_name(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        stream = env.session.create_stream("named", domain="D")
        domain_id = verify_domain_register(nvtxw_events, "D")
        with stream:
            verify_stream_open(nvtxw_events, "named", domain_id=domain_id)
        verify_stream_close(nvtxw_events)


def test_stream_domain_from_other_session(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        with begin_session(backend, nvtxw_events, "other") as other_env:
            domain = other_env.session.get_domain("X")
            verify_domain_register(nvtxw_events, "X", session_id=other_env.session_id)
            with pytest.raises(ValueError, match="different session"):
                env.session.create_stream("s", domain=domain)


@pytest.mark.parametrize(
    "kwargs",
    [
        {"interleaving": 0},
        {"ordering": 0},
        {"skid": 0},
        {"time_domain_id": "tsc"},
    ],
    ids=lambda kwargs: next(iter(kwargs)),
)
def test_stream_option_type_errors(nvtxw_events, backend, kwargs):
    with begin_session(backend, nvtxw_events) as env:
        with pytest.raises(TypeError):
            env.session.create_stream("s", **kwargs)


def test_stream_lifecycle_errors(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        stream = env.session.create_stream("s", domain=domain)
        with pytest.raises(RuntimeError, match="not open"):
            stream.close()
        with pytest.raises(RuntimeError, match="not open"):
            stream.write_mark(1)
        stream.open()
        stream_id = verify_stream_open(nvtxw_events, "s", domain_id=domain_id)
        with pytest.raises(RuntimeError, match="already open"):
            stream.open()
        stream.close()
        verify_stream_close(nvtxw_events, stream_id)
        with pytest.raises(RuntimeError, match="not open"):
            stream.write_mark(1)


def test_stream_reopen(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        stream = env.session.create_stream("s", domain=domain)
        for _ in range(2):
            with stream:
                verify_stream_open(nvtxw_events, "s", domain_id=domain_id)
            verify_stream_close(nvtxw_events)


def test_stream_exit_tolerates_session_end(nvtxw_events, backend):
    session = nvtx.writer.Session("s", backend=backend)
    session.begin()
    session_id = verify_session_begin(nvtxw_events, "s")
    with session.create_stream("st"):
        domain_id = verify_domain_register(nvtxw_events, session_id=session_id)
        stream_id = verify_stream_open(nvtxw_events, "st", domain_id=domain_id)
        session.end()
        verify_stream_close(nvtxw_events, stream_id)
        verify_session_end(nvtxw_events, session_id)
