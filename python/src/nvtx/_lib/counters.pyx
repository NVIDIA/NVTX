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

import array as pyarray
import dataclasses

from cpython.array cimport array as c_array
from enum import Enum
from typing import Iterable, Optional, Union
from nvtx._lib.lib cimport (
    DomainHandle,
    NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64,
    NVTX_PAYLOAD_ENTRY_TYPE_INT64,
    NVTX_SCOPE_NONE,
    _to_bytes,
)
from nvtx._lib.time cimport _fill_time_semantics, nvtxSemanticsTime_t
from nvtx._metadata import PayloadSchemaKey

try:
    import numpy as np
except ImportError:
    np = None


_INT64_MIN = -(1 << 63)
_INT64_MAX = (1 << 63) - 1
_UINT64_MAX = (1 << 64) - 1


class CounterNoValueReason(Enum):
    """
    Reasons for recording a counter sample without a value.

    This enum represents the ``NVTX_COUNTER_SAMPLE_*`` macros from the
    NVTX C counters header.

    Pass one of these values to :meth:`Counter.sample_no_value` when a
    counter sample is known to be zero, unchanged, or unavailable.

    Attributes
    ----------
    ZERO
        The counter value at this sample is zero.
    UNCHANGED
        The counter value is the same as the previous sample.
    UNAVAILABLE
        A sample could not be obtained; only the timestamp is recorded.
    """

    ZERO = NVTX_COUNTER_SAMPLE_ZERO
    UNCHANGED = NVTX_COUNTER_SAMPLE_UNCHANGED
    UNAVAILABLE = NVTX_COUNTER_SAMPLE_UNAVAILABLE


class CounterValueType(Enum):
    """
    How counter sample values relate to previous samples.

    This enum represents the ``NVTX_COUNTER_FLAG_VALUETYPE_*`` macros
    from the NVTX C counter semantics header.

    Values are used with :class:`CounterSemantics`.

    Attributes
    ----------
    ABSOLUTE
        Each sample is an absolute value.
    DELTA
        Each sample is a delta relative to the previous sample. The value
        for the first sample (with no predecessor) is tool-defined.
    DELTA_SINCE_START
        Each sample is a delta relative to the first sample.
    """

    ABSOLUTE = NVTX_COUNTER_FLAG_VALUETYPE_ABSOLUTE
    DELTA = NVTX_COUNTER_FLAG_VALUETYPE_DELTA
    DELTA_SINCE_START = NVTX_COUNTER_FLAG_VALUETYPE_DELTA_SINCE_START


class CounterInterpolation(Enum):
    """
    How tools should interpolate counter values between samples.

    This enum represents the ``NVTX_COUNTER_FLAG_INTERPOLATION_*`` macros
    from the NVTX C counter semantics header.

    Values are used with :class:`CounterSemantics`.

    Attributes
    ----------
    POINT
        No interpolation between samples.
    SINCE_LAST
        Piecewise constant interpolation between the current and the
        previous sample.
    UNTIL_NEXT
        Piecewise constant interpolation between the current and the
        next sample.
    LINEAR
        Piecewise linear interpolation between samples.
    """

    POINT = NVTX_COUNTER_FLAG_INTERPOLATION_POINT
    SINCE_LAST = NVTX_COUNTER_FLAG_INTERPOLATION_SINCE_LAST
    UNTIL_NEXT = NVTX_COUNTER_FLAG_INTERPOLATION_UNTIL_NEXT
    LINEAR = NVTX_COUNTER_FLAG_INTERPOLATION_LINEAR


@dataclasses.dataclass(frozen=True)
class CounterSemantics:
    """
    Metadata that describes how a counter value should be interpreted.

    This class represents the ``nvtxSemanticsCounter_t`` struct from the
    NVTX C counter semantics header.

    Use this with :meth:`nvtx.Domain.get_counter` for whole-counter
    semantics, or with :func:`nvtx.numpy_dtype` for per-field semantics in a
    flat structured counter group.

    Parameters
    ----------
    unit : str, bytes, optional
        Unit name for the counter value, such as ``"bytes"`` or
        ``"objects"``. String units are encoded and stored as bytes.
    value_type : CounterValueType, optional
        Whether samples represent absolute values or deltas.
    interpolation : CounterInterpolation, optional
        How values should be interpreted between adjacent samples.
    min, max : int, float, optional
        Optional value bounds. If both are provided, they must have the
        same numeric type and ``min`` must not exceed ``max``.
    unit_scale_numerator, unit_scale_denominator : int, optional
        Positive scale factors applied to ``unit``.
    """

    unit: Optional[Union[str, bytes]] = None
    value_type: CounterValueType = CounterValueType.ABSOLUTE
    interpolation: CounterInterpolation = CounterInterpolation.POINT
    min: Optional[Union[int, float]] = None
    max: Optional[Union[int, float]] = None
    unit_scale_numerator: int = 1
    unit_scale_denominator: int = 1
    _limit_type: int = dataclasses.field(init=False, repr=False, compare=False)

    def __post_init__(self):
        """
        Validate and normalize counter semantics after construction.
        """

        if self.unit is not None and not isinstance(self.unit, (str, bytes)):
            raise TypeError("Counter semantic unit must be str, bytes, or None.")
        if not isinstance(self.value_type, CounterValueType):
            raise TypeError("Counter semantic value_type must be a CounterValueType.")
        if not isinstance(self.interpolation, CounterInterpolation):
            raise TypeError(
                "Counter semantic interpolation must be a CounterInterpolation."
            )
        for name in ("unit_scale_numerator", "unit_scale_denominator"):
            value = getattr(self, name)
            if not isinstance(value, int):
                raise TypeError(f"Counter semantic {name} must be an int.")
            if value <= 0 or value > _UINT64_MAX:
                raise ValueError(
                    f"Counter semantic {name} must be in range [1, 2**64 - 1]."
                )

        if isinstance(self.unit, str):
            object.__setattr__(self, "unit", self.unit.encode())

        integer_limits = []
        has_signed_integer_limit = False
        limit_type = NVTX_COUNTER_LIMIT_UNDEFINED
        limits = self.min, self.max
        for limit in limits:
            if limit is not None:
                if isinstance(limit, int):
                    integer_limits.append(limit)
                    if limit < 0:
                        has_signed_integer_limit = True
                elif isinstance(limit, float):
                    limit_type = NVTX_COUNTER_LIMIT_F64
                else:
                    raise TypeError(
                        "Counter semantic limits must be int, float, or None."
                    )

        if integer_limits and limit_type == NVTX_COUNTER_LIMIT_F64:
            raise ValueError("Counter semantic min and max must have the same type.")

        if self.min is not None and self.max is not None:
            if self.min > self.max:
                raise ValueError("Counter semantic min cannot exceed max.")

        if integer_limits:
            if has_signed_integer_limit:
                if any(
                    value < _INT64_MIN or value > _INT64_MAX for value in integer_limits
                ):
                    raise ValueError(
                        "Signed counter semantic limits must fit in int64."
                    )
                limit_type = NVTX_COUNTER_LIMIT_I64
            else:
                if any(value > _UINT64_MAX for value in integer_limits):
                    raise ValueError(
                        "Unsigned counter semantic limits must fit in uint64."
                    )
                limit_type = NVTX_COUNTER_LIMIT_U64

        object.__setattr__(self, "_limit_type", limit_type)


cdef inline void _set_counter_limit(
    nvtxCounterLimit_t* dst, object value, int64_t limit_type
):
    if limit_type == NVTX_COUNTER_LIMIT_F64:
        dst.f64 = <double>value
    elif limit_type == NVTX_COUNTER_LIMIT_U64:
        dst.u64 = <uint64_t>value
    elif limit_type == NVTX_COUNTER_LIMIT_I64:
        dst.i64 = <int64_t>value


cdef void _fill_counter_semantics(
    nvtxSemanticsCounter_t* dst,
    object semantics,
):
    dst.header.structSize = sizeof(nvtxSemanticsCounter_t)
    dst.header.semanticId = NVTX_SEMANTIC_ID_COUNTERS_V1
    dst.header.version = NVTX_COUNTER_SEMANTIC_VERSION
    dst.header.next = NULL
    dst.flags = semantics.value_type.value | semantics.interpolation.value
    dst.unitScaleNumerator = semantics.unit_scale_numerator
    dst.unitScaleDenominator = semantics.unit_scale_denominator
    dst.limitType = semantics._limit_type
    dst.unit = NULL
    if semantics.unit is not None:
        dst.unit = semantics.unit
    if semantics.min is not None:
        dst.flags |= NVTX_COUNTER_FLAG_LIMIT_MIN
        _set_counter_limit(&dst.min, semantics.min, dst.limitType)
    if semantics.max is not None:
        dst.flags |= NVTX_COUNTER_FLAG_LIMIT_MAX
        _set_counter_limit(&dst.max, semantics.max, dst.limitType)


cdef class Counter:
    """
    Base class for :class:`Int64Counter`, :class:`Float64Counter`, and
    :class:`ExtCounter` representing a registered NVTX counter.

    Use :meth:`Domain.get_counter <nvtx.Domain.get_counter>`
    to create a concrete counter.

    Use :meth:`sample` to record one value, :meth:`sample_no_value` to
    record a missing value, or :meth:`batch_submit` to record timestamped
    batches.

    Examples
    --------
    >>> import nvtx
    >>> domain = nvtx.get_domain("Training")
    >>> loss_counter = domain.get_counter("loss", float)
    >>> loss_counter.sample(0.42)

    Optionally add semantics to describe units, bounds,
    and how tools should interpret the recorded values:

    >>> loss_counter = domain.get_counter(
    ...     "loss",
    ...     float,
    ...     semantics=nvtx.CounterSemantics(min=0.0),
    ... )
    >>> loss_counter.sample(0.42)
    """

    def __init__(
        self,
        object domain,
        object name,
        object dtype,
        object description,
        object scope,
        object semantics,
        uint64_t time_domain,
    ):
        self.domain = domain
        self.name = name
        self.dtype = dtype
        self.description = description
        self.scope = scope
        self.semantics = semantics
        self.time_domain = time_domain

        self._set_schema_id()

        cdef DomainHandle handle = self.domain.handle
        cdef nvtxCounterAttr_t attr
        cdef nvtxSemanticsCounter_t counter_semantics
        cdef nvtxSemanticsTime_t time_semantics
        cdef bytes name_bytes
        cdef bytes description_bytes
        cdef const char* description_ptr = NULL

        _fill_time_semantics(&time_semantics, self.time_domain)
        if semantics is not None:
            _fill_counter_semantics(&counter_semantics, semantics)
            time_semantics.header.next = &counter_semantics.header

        scope_id = NVTX_SCOPE_NONE
        if scope is not None:
            scope_id = self.domain._get_scope_id(scope)

        name_bytes = _to_bytes(name)
        if description is not None:
            description_bytes = _to_bytes(description)
            description_ptr = description_bytes
        attr.structSize = sizeof(nvtxCounterAttr_t)
        attr.schemaId = self._schema_id
        attr.name = name_bytes
        attr.description = description_ptr
        attr.scopeId = scope_id
        attr.semantics = &time_semantics.header
        attr.counterId = NVTX_COUNTER_ID_NONE
        self._counter_id = nvtxCounterRegister(handle.c_obj, &attr)

    def _set_schema_id(self):
        raise NotImplementedError("Use Domain.get_counter() to create a concrete counter.")

    def sample(self, value):
        """
        Record one counter sample.

        Parameters
        ----------
        value
            Sample value. The accepted type depends on the concrete counter
            subclass returned by :meth:`nvtx.Domain.get_counter`.
        """

        raise NotImplementedError("Use Domain.get_counter() to create a concrete counter.")

    def sample_no_value(self, reason: CounterNoValueReason):
        """
        Record that a counter sample has no explicit value.

        Parameters
        ----------
        reason
            Reason why the value is not present.
        """

        cdef DomainHandle handle = self.domain.handle
        cdef uint8_t reason_value = reason.value
        nvtxCounterSampleNoValue(
            handle.c_obj,
            self._counter_id,
            reason_value,
        )

    def batch_submit(self, values: Iterable, timestamps: Iterable[int]):
        """
        Record a batch of timestamped counter samples.

        Use this to reduce overhead when values are produced in a hot path
        but do not need to be submitted immediately.

        Parameters
        ----------
        values
            Sample values. The accepted type depends on the concrete counter
            subclass returned by :meth:`nvtx.Domain.get_counter`.
        timestamps
            Timestamp for each sample. The number of timestamps must match
            the number of values.
        """

        raise NotImplementedError("Use Domain.get_counter() to create a concrete counter.")


cdef void _submit_counter_batch(
    Counter counter,
    const void* counters,
    size_t counters_size,
    Py_ssize_t sample_count,
    object timestamps,
):
    cdef c_array timestamps_array
    cdef object timestamps_np
    cdef const int64_t* timestamp_data = NULL
    cdef Py_ssize_t timestamp_count
    cdef nvtxCounterBatch_t batch
    cdef DomainHandle handle = counter.domain.handle

    # When numpy is available, np.ascontiguousarray avoids a copy if the
    # caller already passed a C-contiguous int64 array.
    if np is not None:
        timestamps_np = np.ascontiguousarray(timestamps, dtype=np.int64)
        timestamp_count = timestamps_np.size
        if timestamp_count:
            timestamp_data = <const int64_t*><size_t>timestamps_np.ctypes.data
    else:
        timestamps_array = pyarray.array("q", timestamps)
        timestamp_count = len(timestamps_array)
        if timestamp_count:
            timestamp_data = <const int64_t*>timestamps_array.data.as_voidptr

    if sample_count != timestamp_count:
        raise ValueError("values and timestamps must have the same length.")

    batch.counterId = counter._counter_id
    batch.counters = counters
    batch.countersSize = counters_size
    batch.flags = 0
    batch.timestamps = timestamp_data
    batch.timestampsSize = <size_t>timestamp_count * sizeof(int64_t)
    nvtxCounterBatchSubmit(handle.c_obj, &batch)


cdef void _submit_array_counter_batch(
    Counter counter,
    object values,
    object typecode,
    size_t sample_size,
    object timestamps,
):
    cdef c_array value_array
    cdef object value_np
    cdef const void* counters = NULL
    cdef Py_ssize_t sample_count

    # When numpy is available, np.ascontiguousarray avoids a copy if the
    # caller already passed a C-contiguous array of the matching dtype.
    if np is not None:
        value_np = np.ascontiguousarray(values, dtype=np.dtype(typecode))
        sample_count = value_np.size
        if sample_count:
            counters = <const void*><size_t>value_np.ctypes.data
    else:
        value_array = pyarray.array(typecode, values)
        sample_count = len(value_array)
        if sample_count:
            counters = <const void*>value_array.data.as_voidptr

    _submit_counter_batch(
        counter,
        counters,
        <size_t>sample_count * sample_size,
        sample_count,
        timestamps,
    )


cdef class Int64Counter(Counter):
    """
    Counter for signed 64-bit integer samples.

    Examples
    --------
    >>> import nvtx
    >>> domain = nvtx.get_domain("Example")
    >>> bytes_processed_counter = domain.get_counter(
    ...     "bytes processed",
    ...     int,
    ...     semantics=nvtx.CounterSemantics(unit="bytes", min=0),
    ... )
    >>> bytes_processed_counter.sample(4096)
    """

    def _set_schema_id(self):
        self._schema_id = NVTX_PAYLOAD_ENTRY_TYPE_INT64

    def sample(self, int64_t value):
        """
        Record one signed 64-bit integer counter sample.

        This is a low-overhead path and does not perform runtime schema
        validation. Passing a value that does not match the registered
        counter type is undefined.

        Parameters
        ----------
        value : int
            Sample value. It must fit in a signed 64-bit integer.

        Examples
        --------
        >>> import nvtx
        >>> domain = nvtx.get_domain("Example")
        >>> bytes_processed_counter = domain.get_counter("bytes processed", int)
        >>> bytes_processed_counter.sample(4096)
        """

        cdef DomainHandle handle = self.domain.handle
        nvtxCounterSampleInt64(handle.c_obj, self._counter_id, value)

    def batch_submit(self, values: Iterable[int], timestamps: Iterable[int]):
        """
        Record a batch of signed 64-bit integer counter samples.

        Parameters
        ----------
        values : iterable of int
            Sample values. Each value must fit in a signed 64-bit integer.
        timestamps : iterable of int
            Timestamp for each value. The number of timestamps must match
            the number of values.

        Examples
        --------
        >>> import nvtx
        >>> import numpy as np
        >>> domain = nvtx.get_domain("Example")
        >>> bytes_processed_counter = domain.get_counter("bytes processed", int)
        >>> values = np.array([1024, 2048, 4096])
        >>> timestamps = np.array([domain.get_timestamp() for _ in values])
        >>> bytes_processed_counter.batch_submit(values, timestamps)
        """

        _submit_array_counter_batch(
            self,
            values,
            "q",
            sizeof(int64_t),
            timestamps,
        )


cdef class Float64Counter(Counter):
    """
    Counter for double-precision floating-point samples.

    Examples
    --------
    >>> import nvtx
    >>> domain = nvtx.get_domain("Training")
    >>> loss_counter = domain.get_counter("loss", float)
    >>> loss_counter.sample(0.42)
    """

    def _set_schema_id(self):
        self._schema_id = NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64

    def sample(self, double value):
        """
        Record one double-precision floating-point counter sample.

        This is a low-overhead path and does not perform runtime schema
        validation. Passing a value that does not match the registered
        counter type is undefined.

        Parameters
        ----------
        value : float
            Sample value.

        Examples
        --------
        >>> import nvtx
        >>> domain = nvtx.get_domain("Training")
        >>> loss_counter = domain.get_counter("loss", float)
        >>> loss_counter.sample(0.42)
        """

        cdef DomainHandle handle = self.domain.handle
        nvtxCounterSampleFloat64(handle.c_obj, self._counter_id, value)

    def batch_submit(self, values: Iterable[float], timestamps: Iterable[int]):
        """
        Record a batch of double-precision floating-point samples.

        Parameters
        ----------
        values : iterable of float
            Sample values.
        timestamps : iterable of int
            Timestamp for each value. The number of timestamps must match
            the number of values.

        Examples
        --------
        >>> import nvtx
        >>> import numpy as np
        >>> domain = nvtx.get_domain("Training")
        >>> loss_counter = domain.get_counter("loss", float)
        >>> values = np.array([0.9, 0.7, 0.5])
        >>> timestamps = np.array([domain.get_timestamp() for _ in values])
        >>> loss_counter.batch_submit(values, timestamps)
        """

        _submit_array_counter_batch(
            self,
            values,
            "d",
            sizeof(double),
            timestamps,
        )


cdef class ExtCounter(Counter):
    """
    Counter for NumPy dtype-based samples and counter groups.

    The counter dtype defines the binary layout of each sample. Structured
    dtypes can be used to represent flat counter groups.

    Examples
    --------
    >>> import nvtx
    >>> domain = nvtx.get_domain("CUDA")
    >>> memory_dtype = nvtx.numpy_dtype([
    ...     ("allocated", int),
    ...     ("reserved", int),
    ... ])
    >>> memory_counter = domain.get_counter("memory", memory_dtype)
    >>> memory_counter.sample((1024, 2048))
    """

    def __init__(
        self,
        object domain,
        object name,
        object dtype,
        object description,
        object scope,
        object semantics,
        uint64_t time_domain,
    ):
        if np is None:
            raise RuntimeError("Install numpy to submit non-scalar NVTX counters.")
        super().__init__(
            domain,
            name,
            dtype,
            description,
            scope,
            semantics,
            time_domain,
        )

    def _set_schema_id(self):
        self._schema_id = self.domain._get_numpy_dtype_schema(
            PayloadSchemaKey(
                self.dtype, counter_group=self.dtype.fields is not None
            )
        )

    def sample(self, value):
        """
        Record one dtype-compatible counter sample.

        This is a low-overhead path and does not perform runtime schema
        validation. Passing data that does not match the registered dtype
        schema is undefined.

        Parameters
        ----------
        value
            A value accepted by ``numpy.ascontiguousarray(value, dtype=self.dtype)``.
            For a structured dtype, pass one tuple containing the field
            values for a single sample. Prefer to pass values "as-is"
            for avoiding memory allocations when no tool is attached.

        Examples
        --------
        >>> import nvtx
        >>> domain = nvtx.get_domain("CUDA")
        >>> memory_dtype = nvtx.numpy_dtype([
        ...     ("allocated", int),
        ...     ("reserved", int),
        ... ])
        >>> memory_counter = domain.get_counter("memory", memory_dtype)
        >>> memory_counter.sample((1024, 2048))
        """

        cdef object payload
        cdef const void* data
        cdef DomainHandle handle = self.domain.handle

        payload = np.ascontiguousarray(value, self.dtype)
        if payload.size != 1:
            raise ValueError(
                f"sample() expects exactly one sample of dtype {self.dtype} "
                f"({self.dtype.itemsize} bytes); got {payload.size} samples "
                f"({payload.nbytes} bytes). Use batch_submit() to submit "
                f"multiple samples at once."
            )
        data = <const void*><size_t>payload.ctypes.data
        nvtxCounterSample(handle.c_obj, self._counter_id, data, payload.nbytes)

    def batch_submit(self, values: Iterable, timestamps: Iterable[int]):
        """
        Record a batch of dtype-compatible counter samples.

        Parameters
        ----------
        values
            Values accepted by ``numpy.ascontiguousarray(values, dtype=self.dtype)``.
            For a structured dtype, pass an iterable of tuples, with one
            tuple per sample. As a rule of thumb: if you would assemble the
            batch yourself, build it as a NumPy array matching ``self.dtype``
            to avoid a copy; if the data already exists as native Python, pass
            it as-is so no array is allocated when no tool is attached.
        timestamps : iterable of int
            Timestamp for each sample. The number of timestamps must match
            the number of samples represented by ``values``.

        Examples
        --------
        >>> import nvtx
        >>> import numpy as np
        >>> domain = nvtx.get_domain("CUDA")
        >>> memory_dtype = nvtx.numpy_dtype([
        ...     ("allocated", int),
        ...     ("reserved", int),
        ... ])
        >>> memory_counter = domain.get_counter("memory", memory_dtype)
        >>> values = np.array([(1024, 2048), (2048, 4096)], dtype=memory_dtype)
        >>> timestamps = np.array([domain.get_timestamp() for _ in values])
        >>> memory_counter.batch_submit(values, timestamps)
        """

        cdef object payload
        cdef const void* counters = NULL
        cdef Py_ssize_t sample_count
        cdef size_t counters_size

        payload = np.ascontiguousarray(values, self.dtype)
        counters_size = payload.nbytes
        sample_count = counters_size // self.dtype.itemsize
        if sample_count:
            counters = <const void*><size_t>payload.ctypes.data

        _submit_counter_batch(
            self,
            counters,
            counters_size,
            sample_count,
            timestamps,
        )


_dtype_to_counter_classes = {
    int: Int64Counter,
    float: Float64Counter,
}

if np is not None:
    _dtype_to_counter_classes[np.dtype(np.int64)] = Int64Counter
    _dtype_to_counter_classes[np.dtype(np.float64)] = Float64Counter


def _counter_class_from_dtype(dtype):
    """
    Return the concrete counter class used for ``dtype``.
    """

    return _dtype_to_counter_classes.get(dtype, ExtCounter)

class DummyCounter:
    """
    A replacement for :class:`Counter` when the domain is disabled.
    (e.g., when no tool is attached).

    ``DummyCounter`` implements the same public methods as ``Counter`` as
    no-ops.
    """

    name = None
    dtype = None
    domain = None
    description = None
    scope = None
    semantics = None
    time_domain = None

    def sample(self, value):
        pass

    def batch_submit(self, values, timestamps):
        pass

    def sample_no_value(self, reason):
        pass


dummy_counter = DummyCounter()
