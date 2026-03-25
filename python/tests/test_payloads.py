# SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://nvidia.github.io/NVTX/LICENSE.txt for license information.

import pytest

import nvtx
from .conftest import (
    verify_registration_events,
    verify_push,
    verify_pop,
    verify_mark,
    verify_start,
    verify_end,
)

try:
    import numpy as np
except ImportError:
    np = None

if np is not None:
    # Global test data
    UINT32_ARRAY_DTYPE = np.dtype((np.uint32, 4))
    STRUCTURED_DTYPE_INNER = np.dtype([("x", int), ("y", int)])
    STRUCTURED_DTYPE = np.dtype(
        [
            ("x", np.float64),
            ("arr", UINT32_ARRAY_DTYPE),
            ("sub", STRUCTURED_DTYPE_INNER),
        ],
        # Used by `verify_payload_schema_registration()` (conftest.py)
        metadata={
            "inner_dtypes": [UINT32_ARRAY_DTYPE, STRUCTURED_DTYPE_INNER],
        },
    )

    STRUCTURED_PAYLOAD = np.array((3.14, [1, 2, 3, 4], (1, 2)), dtype=STRUCTURED_DTYPE)

    def do_test_payload(nvtx_events, payload):
        """
        Calls all NVTX functions with the given payload.
        Verifies the payload is registered and consumed correctly.
        """
        nvtx.mark(payload=payload)
        nvtx.push_range(payload=payload)
        nvtx.pop_range()
        range_id_1 = nvtx.start_range(payload=payload)
        nvtx.end_range(range_id_1)

        domain = nvtx.get_domain()
        domain.mark(payload=payload)
        domain.push_range(payload=payload)
        domain.pop_range()
        range_id_2 = domain.start_range(payload=payload)
        domain.end_range(range_id_2)

        if nvtx_events:
            verify_registration_events(
                nvtx_events, domain=None, message=None, payload=payload
            )
            for range_id in (range_id_1, range_id_2):
                verify_mark(
                    nvtx_events, None, None, color=None, category=None, payload=payload
                )
                verify_push(
                    nvtx_events, None, None, color=None, category=None, payload=payload
                )
                verify_pop(nvtx_events, None)
                verify_start(
                    nvtx_events, None, None, color=None, category=None, payload=payload
                )
                verify_end(nvtx_events, None, range_id)

    @pytest.mark.parametrize(
        "dtype",
        [
            np.int32,
            np.float64,
            np.uint16,
            np.float32,
            np.int8,
            np.uint8,
            np.int16,
            np.int64,
            np.uint32,
            np.uint64,
        ],
    )
    def test_1d_array(nvtx_events, dtype):
        payload = np.array([1, 2, 3, 4, 5], dtype=dtype)
        do_test_payload(nvtx_events, payload)
        do_test_payload(nvtx_events, payload[::2])  # [1, 3] -- non-contiguous

    @pytest.mark.parametrize("iterable", (list, tuple))
    @pytest.mark.parametrize(
        "values",
        (
            (),
            (1, 2),
            (1.5, 2.5),
            (STRUCTURED_PAYLOAD, STRUCTURED_PAYLOAD),
            (b"", b"hello"),
        ),
    )
    def test_list_tuple(nvtx_events, iterable, values):
        do_test_payload(nvtx_events, iterable(values))

    def test_range(nvtx_events):
        do_test_payload(nvtx_events, range(5))

    @pytest.mark.parametrize("payload", (b"", b"hello"))
    def test_bytes(nvtx_events, payload):
        do_test_payload(nvtx_events, payload)

    def test_structured_dtype(nvtx_events):
        do_test_payload(nvtx_events, STRUCTURED_PAYLOAD)

    def test_annotate_context_manager_mutable_list(nvtx_events):
        payload = [1, 2, 3]
        ann = nvtx.annotate(payload=payload)

        with ann:
            pass

        if nvtx_events:
            verify_registration_events(
                nvtx_events, domain=None, message=None, payload=payload
            )
            verify_push(
                nvtx_events,
                None,
                None,
                color=None,
                category=None,
                payload=payload,
            )
            verify_pop(nvtx_events, None)

        payload[0] = 0
        with ann:
            pass

        if nvtx_events:
            verify_registration_events(
                nvtx_events, domain=None, message=None, payload=payload
            )
            verify_push(
                nvtx_events,
                None,
                None,
                color=None,
                category=None,
                payload=payload,
            )
            verify_pop(nvtx_events, None)

    def test_annotate_decorator_mutable_ndarray(nvtx_events):
        payload = np.array([1, 2, 3], dtype=np.int32)

        @nvtx.annotate(payload=payload)
        def work():
            pass

        work()
        if nvtx_events:
            verify_registration_events(
                nvtx_events, domain=None, message="work", payload=payload
            )
            verify_push(
                nvtx_events,
                None,
                "work",
                color=None,
                category=None,
                payload=payload,
            )
            verify_pop(nvtx_events, None)

        payload[0] = 0
        work()
        if nvtx_events:
            verify_registration_events(nvtx_events, None, "work", payload=payload)
            verify_push(
                nvtx_events,
                None,
                "work",
                color=None,
                category=None,
                payload=payload,
            )
            verify_pop(nvtx_events, None)
else:

    def test_payloads_no_numpy(nvtx_events):
        if nvtx_events:
            with pytest.warns(
                nvtx._lib.lib.NvtxWarning,
                match=" Install numpy for extended payload support.",
            ):
                nvtx.mark(payload=[1, 2, 3])

            verify_mark(
                nvtx_events, None, None, color=None, category=None, payload=None
            )
