# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

import array

import pytest

import nvtx
from nvtx._lib.counters import dummy_counter
from .conftest import (
    verify_counter_batch_submit,
    verify_counter_register,
    verify_counter_sample,
    verify_counter_sample_float64,
    verify_counter_sample_int64,
    verify_counter_sample_no_value,
    verify_registration_events,
    verify_timestamp_get,
)

try:
    import numpy as np
except ImportError:
    np = None


@pytest.fixture(scope="session", params=[None, "", "descr"])
def description(request):
    return request.param


@pytest.fixture(scope="session", params=[None, "", "scope"])
def scope(request):
    return request.param


@pytest.fixture(
    scope="session",
    params=[
        pytest.param(None, id="none"),
        pytest.param(
            nvtx.CounterSemantics(unit="objects", min=0),
            id="u64-min-str-unit",
        ),
        pytest.param(
            nvtx.CounterSemantics(max=1 << 63),
            id="u64-int64-boundary-max",
        ),
        pytest.param(
            nvtx.CounterSemantics(
                value_type=nvtx.CounterValueType.DELTA,
                interpolation=nvtx.CounterInterpolation.LINEAR,
                min=-5,
                max=5,
            ),
            id="i64-bounds-flags",
        ),
        pytest.param(
            nvtx.CounterSemantics(
                unit=b"bytes",
                max=1024.0,
                unit_scale_numerator=1,
                unit_scale_denominator=1024,
            ),
            id="f64-max-bytes-scale",
        ),
    ],
)
def semantics(request):
    return request.param


@pytest.fixture(
    scope="session",
    params=[
        nvtx.TimestampType.TOOL_PROVIDED,
        nvtx.TimestampType.CPU_TSC,
    ],
)
def time_domain(request):
    return request.param


@pytest.mark.parametrize(
    "dtype",
    [int, *([np.int64, "int", "int64"] if np is not None else ())],
)
def test_get_counter_int(nvtx_events, domain, dtype, description, scope, semantics):
    counter_name = "counter_int"
    counter_value = 7
    counter = nvtx.get_domain(domain).get_counter(
        counter_name,
        dtype,
        description=description,
        scope=scope,
        semantics=semantics,
    )
    counter.sample(counter_value)

    if nvtx_events:
        counter_id = verify_counter_register(
            nvtx_events,
            domain,
            counter_name,
            dtype,
            description=description,
            scope=scope,
            semantics=semantics,
        )
        verify_counter_sample_int64(nvtx_events, domain, counter_id, counter_value)


@pytest.mark.parametrize(
    "dtype",
    [float, *([np.float64, "float", "float64"] if np is not None else ())],
)
def test_get_counter_float(nvtx_events, domain, dtype):
    counter_name = "counter_float"
    counter_value = 2.718
    counter = nvtx.get_domain(domain).get_counter(
        counter_name,
        dtype,
    )
    counter.sample(counter_value)

    if nvtx_events:
        counter_id = verify_counter_register(
            nvtx_events,
            domain,
            counter_name,
            dtype,
        )
        verify_counter_sample_float64(nvtx_events, domain, counter_id, counter_value)


@pytest.mark.skipif(np is None, reason="NumPy is required for counter groups")
def test_counter_group(nvtx_events, domain, description, scope, semantics):
    dtype = np.dtype([("gen", int), ("collected", int), ("uncollectable", int)])
    payload = (2, 150, 3)
    counter_name = "counter_group"
    counter = nvtx.get_domain(domain).get_counter(
        counter_name,
        dtype,
        description=description,
        scope=scope,
        semantics=semantics,
    )
    counter.sample(payload)

    if nvtx_events:
        counter_id = verify_counter_register(
            nvtx_events,
            domain,
            counter_name,
            dtype,
            description=description,
            scope=scope,
            semantics=semantics,
        )
        verify_counter_sample(
            nvtx_events,
            domain,
            counter_id,
            np.asarray(payload, dtype=dtype).tobytes(),
        )


@pytest.mark.skipif(np is None, reason="NumPy is required for counter groups")
def test_counter_group_per_field_semantics(nvtx_events, domain):
    gen_dtype = nvtx.numpy_dtype(
        int,
        counter_semantics=nvtx.CounterSemantics(unit="generation", min=0, max=2),
    )
    objects_dtype = nvtx.numpy_dtype(
        int,
        counter_semantics=nvtx.CounterSemantics(unit="objects", min=0),
    )
    dtype = np.dtype(
        [
            ("generation", gen_dtype),
            ("collected", objects_dtype),
            ("uncollectable", objects_dtype),
        ]
    )
    payload = (1, 200, 5)
    counter_name = "counter_group_field_semantics"
    counter = nvtx.get_domain(domain).get_counter(counter_name, dtype)
    counter.sample(payload)

    if nvtx_events:
        counter_id = verify_counter_register(nvtx_events, domain, counter_name, dtype)
        verify_counter_sample(
            nvtx_events,
            domain,
            counter_id,
            np.asarray(payload, dtype=dtype).tobytes(),
        )


@pytest.mark.skipif(np is None, reason="NumPy is required for counter groups")
@pytest.mark.parametrize(
    "dtype,counter_name,error_match",
    [
        pytest.param(
            np.dtype([("outer", [("x", int), ("y", int)])]),
            "nested_counter_group",
            "Nested counter groups are not supported",
            id="nested-group",
        ),
        pytest.param(
            np.dtype(([("x", int), ("y", int)], (2,))),
            "structured_array_counter_group",
            "fixed-size array of a structured dtype",
            id="structured-array",
        ),
        pytest.param(
            np.dtype([("values", int, (2,))]),
            "array_field_counter_group",
            "array fields are not supported",
            id="int-array-field",
        ),
        pytest.param(
            np.dtype([("values", float, (4,))]),
            "array_field_counter_group",
            "array fields are not supported",
            id="float-array-field",
        ),
        pytest.param(
            np.dtype([("outer", [("x", int), ("y", int)], (2,))]),
            "array_field_counter_group",
            "array fields are not supported",
            id="nested-array-field",
        ),
    ]
    if np is not None
    else [],
)
def test_counter_group_unsupported_dtype_rejected(
    nvtx_events, domain, dtype, counter_name, error_match
):
    with pytest.raises(TypeError, match=error_match):
        nvtx.get_domain(domain).get_counter(counter_name, dtype)

    if nvtx_events:
        verify_registration_events(nvtx_events, domain)


@pytest.mark.parametrize(
    "dtype,values,typecode",
    [
        (int, [1, 2, 3, 4, 5], "q"),
        (float, [1.25, 2.5, 3.75, 5.0, 6.25], "d"),
    ],
)
def test_scalar_counter_batch_submit(
    nvtx_events, domain, semantics, time_domain, dtype, values, typecode
):
    counter_name = "counter_batch"
    domain_obj = nvtx.get_domain(domain)
    counter = domain_obj.get_counter(
        counter_name,
        dtype,
        semantics=semantics,
        time_domain=time_domain,
    )

    timestamps = [domain_obj.get_timestamp() for _ in values]
    counter.batch_submit(values, timestamps=timestamps)

    if nvtx_events:
        counter_id = verify_counter_register(
            nvtx_events,
            domain,
            counter_name,
            dtype,
            semantics=semantics,
            time_domain=time_domain,
        )
        for timestamp in timestamps:
            verify_timestamp_get(nvtx_events, timestamp)
        verify_counter_batch_submit(
            nvtx_events,
            domain,
            counter_id,
            array.array(typecode, values).tobytes(),
            timestamps,
        )


@pytest.mark.skipif(np is None, reason="NumPy is required for counter groups")
def test_counter_batch_submit(nvtx_events, domain, time_domain):
    dtype = nvtx.numpy_dtype(
        [
            ("loss", float),
            ("lr", float),
            ("grad_norm", float),
        ]
    )
    counter_name = "counter_batch"
    domain_obj = nvtx.get_domain(domain)
    counter = domain_obj.get_counter(counter_name, dtype, time_domain=time_domain)

    values = [(1.1 * n, 2.2 * n, 3.3 * n) for n in range(1, 6)]
    timestamps = [domain_obj.get_timestamp() for _ in values]
    counter.batch_submit(values, timestamps=timestamps)

    if nvtx_events:
        counter_id = verify_counter_register(
            nvtx_events,
            domain,
            counter_name,
            dtype,
            time_domain=time_domain,
        )
        for timestamp in timestamps:
            verify_timestamp_get(nvtx_events, timestamp)
        verify_counter_batch_submit(
            nvtx_events,
            domain,
            counter_id,
            np.asarray(values, dtype=dtype).tobytes(),
            timestamps,
        )


@pytest.mark.parametrize(
    "reason",
    [
        nvtx.CounterNoValueReason.ZERO,
        nvtx.CounterNoValueReason.UNCHANGED,
        nvtx.CounterNoValueReason.UNAVAILABLE,
    ],
)
def test_sample_no_value(nvtx_events, domain, reason):
    counter_name = "counter_no_value"
    counter = nvtx.get_domain(domain).get_counter(counter_name, int)
    counter.sample_no_value(reason)

    if nvtx_events:
        counter_id = verify_counter_register(nvtx_events, domain, counter_name, int)
        verify_counter_sample_no_value(nvtx_events, domain, counter_id, reason.value)


@pytest.mark.parametrize(
    "kwargs,expected_error",
    [
        ({"min": 2, "max": 1}, ValueError),
        ({"min": -1, "max": 1 << 63}, ValueError),
        ({"max": 1 << 64}, ValueError),
        ({"min": 1, "max": 2.0}, ValueError),
        ({"min": "low"}, TypeError),
        ({"unit_scale_numerator": 0}, ValueError),
        ({"unit_scale_denominator": 0}, ValueError),
        ({"unit": 1}, TypeError),
        ({"value_type": nvtx.CounterValueType.ABSOLUTE.value}, TypeError),
        ({"interpolation": nvtx.CounterInterpolation.POINT.value}, TypeError),
    ],
)
def test_counter_semantics_validation(kwargs, expected_error):
    with pytest.raises(expected_error):
        nvtx.CounterSemantics(**kwargs)


def test_get_counter_duplicate_same_params(nvtx_events, domain):
    domain_obj = nvtx.get_domain(domain)
    counter_name = "counter_duplicate"
    first = domain_obj.get_counter(counter_name, int)
    second = domain_obj.get_counter(counter_name, int)
    third = domain_obj.get_counter(
        counter_name,
        int,
        semantics=nvtx.CounterSemantics(unit="items"),
    )

    assert first is second
    if nvtx_events:
        assert third is not first
        verify_counter_register(nvtx_events, domain, counter_name, int)
        verify_counter_register(
            nvtx_events,
            domain,
            counter_name,
            int,
            semantics=nvtx.CounterSemantics(unit="items"),
        )
    else:
        assert first is dummy_counter
        assert second is dummy_counter
        assert third is dummy_counter
