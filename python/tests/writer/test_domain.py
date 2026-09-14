# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Domain, registered-string, category, and scope tests for nvtx.writer."""

import pytest

from nvtx.writer import WriterError

from .conftest import (
    WriterResultCode,
    assert_failed,
    begin_session,
    verify_category_register,
    verify_domain_register,
    verify_scope_register,
    verify_string_register,
)


def test_default_domain(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        # None and the empty name both select the cached default domain.
        assert env.session.get_domain(None) is domain
        assert env.session.get_domain("") is domain


def test_named_domains(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain_a = env.session.get_domain("A")
        verify_domain_register(nvtxw_events, "A")
        assert env.session.get_domain(b"A") is domain_a
        domain_b = env.session.get_domain("B")
        verify_domain_register(nvtxw_events, "B")
        assert domain_b is not domain_a


def test_domain_register_failure_does_not_cache_domain(
    nvtxw_events, backend, backend_controls
):
    with begin_session(backend, nvtxw_events, "domain retry") as env:
        backend_controls.set_next_result(WriterResultCode.FAILED)
        with pytest.raises(WriterError) as excinfo:
            env.session.get_domain("D")
        assert_failed(excinfo, "DomainRegister", WriterResultCode.FAILED)

        env.session.get_domain("D")
        verify_domain_register(nvtxw_events, "D", session_id=env.session_id)


def test_get_domain_type_error(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        with pytest.raises(TypeError):
            env.session.get_domain(123)


def test_registered_string(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        registered = domain.get_registered_string("hello")
        handle = verify_string_register(nvtxw_events, "hello", domain_id=domain_id)
        assert registered.handle == handle
        assert "hello" in repr(registered)
        # str and bytes spellings share one registration.
        assert domain.get_registered_string(b"hello") is registered
        domain.get_registered_string("other")
        verify_string_register(nvtxw_events, "other")


def test_registered_string_type_error(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        with pytest.raises(TypeError):
            domain.get_registered_string(123)


def test_category_ids_are_sequential(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        assert domain.get_category_id("first") == 1
        verify_category_register(
            nvtxw_events, "first", category_id=1, domain_id=domain_id
        )
        assert domain.get_category_id("second") == 2
        verify_category_register(nvtxw_events, "second", category_id=2)
        # Cached: no further backend calls.
        assert domain.get_category_id("first") == 1
        assert domain.get_category_id(b"second") == 2


def test_category_name_type_error(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        with pytest.raises(TypeError):
            domain.get_category_id(3)


def test_scope_at_root(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        domain_id = verify_domain_register(
            nvtxw_events, None, session_id=env.session_id
        )
        scope = domain.get_scope("node/a")
        scope_id = verify_scope_register(nvtxw_events, "node/a", domain_id=domain_id)
        assert scope.scope_id == scope_id
        assert scope.path == "node/a"


def test_scope_nesting(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        parent = domain.get_scope("parent")
        parent_id = verify_scope_register(nvtxw_events, "parent")
        child = domain.get_scope("child", parent=parent)
        child_id = verify_scope_register(nvtxw_events, "child", parent=parent_id)
        assert child.scope_id == child_id


def test_scope_caching(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        scope = domain.get_scope("a")
        verify_scope_register(nvtxw_events, "a")
        assert domain.get_scope(b"a") is scope
        # The same path under a different parent is a distinct scope.
        nested = domain.get_scope("a", parent=scope)
        verify_scope_register(nvtxw_events, "a", parent=scope.scope_id)
        assert nested is not scope


def test_scope_parent_errors(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain_a = env.session.get_domain("A")
        domain_a_id = verify_domain_register(nvtxw_events, "A")
        domain_b = env.session.get_domain("B")
        verify_domain_register(nvtxw_events, "B")
        parent = domain_a.get_scope("p")
        parent_id = verify_scope_register(nvtxw_events, "p", domain_id=domain_a_id)
        assert parent.scope_id == parent_id
        with pytest.raises(ValueError, match="different domain"):
            domain_b.get_scope("c", parent=parent)
        with pytest.raises(TypeError):
            domain_a.get_scope("c", parent="p")
