# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Backend loading and lifecycle tests for nvtx.writer."""

import os
import pytest
from pathlib import Path

import nvtx._lib.lib
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


def test_load_backend_returns_usable_backend(nvtxw_events, backend_path):
    backend = nvtx.writer.load_backend(backend_path)
    try:
        with begin_session(backend, nvtxw_events, "smoke"):
            pass
    finally:
        backend.close()


def test_load_backend_missing_file(tmp_path):
    with pytest.raises(OSError):
        nvtx.writer.load_backend(tmp_path / "does_not_exist.so")


def test_load_backend_missing_symbol():
    # Any loadable library without nvtxwGetInterface works; use the nvtx
    # Cython extension module itself.
    with pytest.raises(WriterError) as excinfo:
        nvtx.writer.load_backend(nvtx._lib.lib.__file__)
    assert excinfo.value.result_code == WriterResultCode.LIBRARY_SYMBOL_MISSING
    assert excinfo.value.operation == "load_backend"


def test_backend_constructor_rejects_invalid_module():
    with pytest.raises(ValueError, match="load_backend"):
        nvtx.writer.Backend(None)


def test_close_is_idempotent(backend):
    backend.close()
    backend.close()


def test_backend_module_is_shared_until_last_wrapper_closes(
    monkeypatch, backend_path, backend_controls
):
    path = Path(backend_path)
    monkeypatch.chdir(path.parent)

    first = nvtx.writer.load_backend(path.name)
    second = nvtx.writer.load_backend(path)
    third = nvtx.writer.load_backend(os.fsencode(path))

    first.close()
    second.close()
    assert backend_controls.finalize_count == 0

    third.close()
    assert backend_controls.finalize_count == 1

    reloaded = nvtx.writer.load_backend(path)
    reloaded.close()
    assert backend_controls.finalize_count == 2


def test_close_ends_active_sessions(nvtxw_events, backend):
    session = nvtx.writer.Session("open at close", backend=backend)
    session.begin()
    session_id = verify_session_begin(nvtxw_events, "open at close")
    stream = session.create_stream("s")
    stream.open()
    domain_id = verify_domain_register(nvtxw_events, session_id=session_id)
    stream_id = verify_stream_open(
        nvtxw_events, "s", domain_id=domain_id, session_id=session_id
    )

    backend.close()
    verify_stream_close(nvtxw_events, stream_id)
    verify_session_end(nvtxw_events, session_id)


def test_operations_after_close_raise(backend):
    backend.close()
    session = nvtx.writer.Session("late", backend=backend)
    with pytest.raises(RuntimeError, match="backend is closed"):
        session.begin()


def test_get_interface_failure_is_one_shot(backend_path, backend_controls):
    backend_controls.set_next_result(WriterResultCode.INTERFACE_VERSION_NOT_SUPPORTED)
    with pytest.raises(WriterError) as excinfo:
        nvtx.writer.load_backend(backend_path)
    assert_failed(
        excinfo,
        "nvtxwGetInterface",
        WriterResultCode.INTERFACE_VERSION_NOT_SUPPORTED,
    )

    recovered = nvtx.writer.load_backend(backend_path)
    recovered.close()


def test_backend_close_skips_finalize_after_session_end_failure(
    nvtxw_events, backend, backend_controls
):
    session = nvtx.writer.Session("close failure", backend=backend)
    session.begin()
    verify_session_begin(nvtxw_events, "close failure")

    backend_controls.set_next_result(WriterResultCode.FAILED)
    with pytest.raises(WriterError) as excinfo:
        backend.close()
    assert_failed(excinfo, "SessionEnd", WriterResultCode.FAILED)
    assert backend_controls.finalize_count == 0

    backend.close()
    assert backend_controls.finalize_count == 1
