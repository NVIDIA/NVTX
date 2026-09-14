# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Graceful degradation of nvtx.writer when numpy is not installed.

Scalar int/float counters must keep working without numpy; that positive
path is covered by the unconditional tests in test_counters.py, which run
in both halves of the numpy test matrix.
"""

import pytest

from .conftest import (
    begin_session,
    np,
    open_stream,
    verify_counter_register,
    verify_domain_register,
)

pytestmark = pytest.mark.skipif(
    np is not None, reason="exercises behavior without numpy"
)


def test_get_schema_requires_numpy(nvtxw_events, backend):
    with begin_session(backend, nvtxw_events) as env:
        domain = env.session.get_domain()
        verify_domain_register(nvtxw_events, None, session_id=env.session_id)
        with pytest.raises(RuntimeError, match="numpy"):
            domain.get_schema([("v", "i8")])


def test_write_counter_batch_requires_numpy(nvtxw_events, backend):
    with open_stream(backend, nvtxw_events) as env:
        counter = env.domain.get_counter("c", int)
        verify_counter_register(nvtxw_events, name="c")
        with pytest.raises(RuntimeError, match="numpy"):
            env.stream.write_counter_batch(counter, [1], timestamps=[1])
