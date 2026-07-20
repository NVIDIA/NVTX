# SPDX-FileCopyrightText: Copyright (c) 2020-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

import enum
import warnings
from nvtx._lib.counters import (
    Counter,
    DummyCounter,
    _counter_class_from_dtype,
    dummy_counter,
)
from functools import lru_cache
from libc.stdlib cimport malloc, free
from libc.string cimport memcpy
from nvtx._lib.counters cimport (
    _fill_counter_semantics,
    nvtxSemanticsCounter_t,
)
from nvtx._lib.time cimport nvtxTimestampGet
from nvtx._lib.lib cimport *
from nvtx.colors import color_to_hex
from nvtx._lib.time import TimestampType
from nvtx._metadata import PayloadSchemaKey

from typing import Optional

try:
    import numpy as np
    _dtype_to_entry_type = {
        np.int8: NVTX_PAYLOAD_ENTRY_TYPE_INT8,
        np.int16: NVTX_PAYLOAD_ENTRY_TYPE_INT16,
        np.int32: NVTX_PAYLOAD_ENTRY_TYPE_INT32,
        np.int64: NVTX_PAYLOAD_ENTRY_TYPE_INT64,
        np.uint8: NVTX_PAYLOAD_ENTRY_TYPE_UINT8,
        np.uint16: NVTX_PAYLOAD_ENTRY_TYPE_UINT16,
        np.uint32: NVTX_PAYLOAD_ENTRY_TYPE_UINT32,
        np.uint64: NVTX_PAYLOAD_ENTRY_TYPE_UINT64,
        np.float16: NVTX_PAYLOAD_ENTRY_TYPE_FLOAT16,
        np.float32: NVTX_PAYLOAD_ENTRY_TYPE_FLOAT32,
        np.float64: NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64,
        np.str_: NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32,
        np.bytes_: NVTX_PAYLOAD_ENTRY_TYPE_BYTE,
    }
except ImportError:
    np = None


cpdef bytes _to_bytes(object s):
    return s if isinstance(s, bytes) else s.encode()

def initialize():
    nvtxInitialize(NULL)


class NvtxWarning(UserWarning):
    pass


def _validate_counter_group_dtype(dtype):
    if np is None:
        if dtype is int or dtype is float:
            return
        raise RuntimeError("Install numpy to use dtype-based NVTX counters.")
    if dtype.subdtype is not None and dtype.subdtype[0].fields is not None:
        raise TypeError(
            "Nested counter groups are not supported. "
            "Counter dtype must not be a fixed-size array of a structured dtype."
        )

    if dtype.fields is None:
        return

    # Iterate dtype.names, not dtype.fields to avoid alias duplicates.
    for field_name in dtype.names:
        field_type = dtype.fields[field_name][0]
        if field_type.subdtype:
            raise TypeError(
                "Counter group array fields are not supported. "
                f"Field {field_name!r} must be a scalar dtype."
            )

        if field_type.fields is not None:
            raise TypeError(
                "Nested counter groups are not supported. "
                f"Field {field_name!r} must not have a structured dtype."
            )


def _normalize_counter_dtype(dtype):
    """
    Normalize dtype-like counter specifications before caching.
    """

    if np is not None:
        if dtype is int:
            # Force 64-bit integer (64-bit Windows uses int32 for plain ``int``).
            dtype = np.dtype(np.int64)
        else:
            try:
                dtype = np.dtype(dtype)
            except TypeError as e:
                raise TypeError("Counter dtype must be int, float, or NumPy dtype-like.") from e
    _validate_counter_group_dtype(dtype)
    return dtype


def _counter_semantics_from_metadata(metadata):
    """
    Return counter semantics stored in NVTX dtype metadata, if present.
    """

    if metadata is None:
        return None
    return getattr(metadata, "counter_semantics", None)


class PredefinedScope(enum.Enum):
    """Predefined NVTX execution scopes."""

    NONE = NVTX_SCOPE_NONE
    ROOT = NVTX_SCOPE_ROOT
    CURRENT_HW_MACHINE = NVTX_SCOPE_CURRENT_HW_MACHINE
    CURRENT_HW_SOCKET = NVTX_SCOPE_CURRENT_HW_SOCKET
    CURRENT_HW_CPU_PHYSICAL = NVTX_SCOPE_CURRENT_HW_CPU_PHYSICAL
    CURRENT_HW_CPU_LOGICAL = NVTX_SCOPE_CURRENT_HW_CPU_LOGICAL
    CURRENT_HW_INNERMOST = NVTX_SCOPE_CURRENT_HW_INNERMOST
    CURRENT_HYPERVISOR = NVTX_SCOPE_CURRENT_HYPERVISOR
    CURRENT_VM = NVTX_SCOPE_CURRENT_VM
    CURRENT_KERNEL = NVTX_SCOPE_CURRENT_KERNEL
    CURRENT_CONTAINER = NVTX_SCOPE_CURRENT_CONTAINER
    CURRENT_OS = NVTX_SCOPE_CURRENT_OS
    CURRENT_SW_PROCESS = NVTX_SCOPE_CURRENT_SW_PROCESS
    CURRENT_SW_THREAD = NVTX_SCOPE_CURRENT_SW_THREAD
    CURRENT_SW_INNERMOST = NVTX_SCOPE_CURRENT_SW_INNERMOST


def _entry_flags_from_metadata(metadata):
    """
    Return the resolved NVTX payload entry flags stored in dtype metadata, if
    present (0 when absent).
    """

    if metadata is None:
        return 0
    entry_kind = getattr(metadata, "entry_kind", None)
    return entry_kind.value if entry_kind is not None else 0


def _entry_type_from_metadata(metadata):
    """Return an explicit NVTX payload entry type, or ``None``."""
    if metadata is None:
        return None
    entry_type = getattr(metadata, "entry_type", None)
    return entry_type.value if entry_type is not None else None


class EntryKind(enum.Enum):
    """
    Role of a field in a structured payload dtype.

    Attach to a field with :func:`nvtx.numpy_dtype` and its ``entry_kind``
    argument. The event roles (``RANGE_BEGIN``, ``RANGE_END``, ``MARK``)
    mark the timestamp fields of an event schema; ``COUNTER_TIMESTAMP``
    marks the embedded timestamp field of a counter group; ``MESSAGE``
    marks a string field holding the event message.
    """

    RANGE_BEGIN = (
        NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN
        | NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP
    )
    RANGE_END = (
        NVTX_PAYLOAD_ENTRY_FLAG_RANGE_END | NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP
    )
    MARK = NVTX_PAYLOAD_ENTRY_FLAG_MARK | NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP
    MESSAGE = NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE
    COUNTER_TIMESTAMP = NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP


class PayloadEntryType(enum.Enum):
    """
    NVTX interpretation of a specially typed payload field.

    Attach to a field with :func:`nvtx.numpy_dtype` and its ``entry_type``
    argument. The field retains ordinary fixed-width NumPy storage while NVTX
    tools interpret it as an event attribute or identifier.
    """

    RANGE_ID = NVTX_PAYLOAD_ENTRY_TYPE_RANGE_ID
    CATEGORY = NVTX_PAYLOAD_ENTRY_TYPE_CATEGORY
    COLOR_ARGB = NVTX_PAYLOAD_ENTRY_TYPE_COLOR_ARGB
    SCOPE_ID = NVTX_PAYLOAD_ENTRY_TYPE_SCOPE_ID
    REGISTERED_STRING = NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE


class EventKind(enum.Enum):
    """
    Event kind for the writer's ``write_event`` / ``write_event_batch``,
    mapped to the schema-level ``NVTX_PAYLOAD_SCHEMA_FLAG_*`` flag.

    ``NVTX_PAYLOAD_SCHEMA_FLAG_COUNTER_GROUP`` is not exposed as an
    ``EventKind`` because counter-group schemas are created by
    ``Domain.get_counter()``. The flag is set internally during registration.
    """

    MARK = NVTX_PAYLOAD_SCHEMA_FLAG_MARK
    RANGE_PUSHPOP = NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSHPOP
    RANGE_STARTEND = NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_STARTEND
    RANGE_PUSH = NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSH
    RANGE_POP = NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_POP
    RANGE_START = NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_START
    RANGE_END = NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_END


class BatchOrdering(enum.Enum):
    """Timestamp ordering within a batch of deferred events or counters."""

    SORTED = NVTX_BATCH_FLAG_TIME_SORTED
    PARTIALLY_SORTED = NVTX_BATCH_FLAG_TIME_SORTED_PARTIALLY
    SORTED_PER_SCOPE = NVTX_BATCH_FLAG_TIME_SORTED_PER_SCOPE
    UNSORTED = NVTX_BATCH_FLAG_UNSORTED


_payload_setters = {}


def payload_setter(type):
    """
    A helper decorator to register a payload setter for a given type.
    """
    def register(func):
        _payload_setters[type] = func
        return func
    return register


cdef class EventAttributes:
    """
    A wrapper class for ``nvtxEventAttributes_t`` C struct.
    Use :func:`nvtx.Domain.get_event_attributes` to create an instance.

    Attributes
    ----------
    message : RegisteredString
        A message associated with the event.
        Retrieved by :func:`nvtx.Domain.get_registered_string`.
    color : str or int
        A color associated with the event.
        Supports `matplotlib` colors if it is available.
    category : int
        An integer specifying the category within the domain
        under which the event is scoped.
        If not set, the event is not associated with a category.
        Retrieved by :func:`nvtx.Domain.get_category_id`.
    payload : int, float, numpy.ndarray, list, tuple, range, or bytes
        A value associated with this event. Using payload for large data
        is more efficient than embedding data in messages.
        It also produces richer information for analysis by profiling tools.

        .. note:: payloads of type other than ``int`` or ``float`` requires
                  NumPy to be installed (not installed with ``nvtx`` package).
    """

    def __dealloc__(self):
        self._clear_payload()

    def __init__(self, object domain, object message=None, color=None, category=None,
                 payload=None):
        self.domain = domain
        self.c_obj.version = NVTX_VERSION
        self.c_obj.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE
        self.c_obj.colorType = NVTX_COLOR_ARGB

        self.message = message
        self.color = color
        self.category = category
        self.payload = payload

    @property
    def message(self):
        return self._message

    @message.setter
    def message(self, object value):
        if value is None:
            self.c_obj.messageType = NVTX_MESSAGE_UNKNOWN
            self._message = None
        else:
            self.c_obj.messageType = NVTX_MESSAGE_TYPE_REGISTERED
            self._message = value
            self.c_obj.message.registered = (<StringHandle> self._message.handle).c_obj

    @property
    def color(self):
        return self._color

    @color.setter
    def color(self, value):
        self._color = value
        self.c_obj.color = color_to_hex(self._color)

    @property
    def category(self):
        return self._category

    @category.setter
    def category(self, value):
        if value is None:
            value = 0
        self._category = value
        self.c_obj.category = value

    @property
    def payload(self):
        return self._payload

    @payload.setter
    def payload(self, value):
        self._clear_payload()
        if value is None:
            self.c_obj.payloadType = NVTX_PAYLOAD_UNKNOWN
            return

        self._payload = value
        setter = _payload_setters.get(type(value))
        if setter is None:
            msg = f"Unsupported payload type: {type(value)}."
            if np is None:
                msg += " Install numpy for extended payload support."
            warnings.warn(msg, NvtxWarning)
            return
        setter(self, value)

    @payload_setter(int)
    def _set_payload_int(self, payload):
        self.c_obj.payload.llValue = payload
        self.c_obj.payloadType = NVTX_PAYLOAD_TYPE_INT64

    @payload_setter(float)
    def _set_payload_float(self, payload):
        self.c_obj.payload.dValue = payload
        self.c_obj.payloadType = NVTX_PAYLOAD_TYPE_DOUBLE

    if np is not None:
        @payload_setter(np.ndarray)
        def _set_payload_numpy(self, payload):
            if payload.nbytes == 0:
                return
            schema = self.domain._get_numpy_dtype_schema(
                PayloadSchemaKey(payload.dtype)
            )
            cdef size_t array_length = 0
            if payload.ndim:
                schema = self.domain._get_numpy_array_schema(schema)
                payload = np.ascontiguousarray(payload)
                array_length = payload.size

            self._payload_object = payload
            self._set_binary_payload(
                <void*><size_t>payload.ctypes.data,
                <uint64_t>schema,
                <size_t>payload.nbytes,
                array_length)


        @payload_setter(range)
        @payload_setter(list)
        @payload_setter(tuple)
        @payload_setter(bytes)
        def _set_payload_iterable(self, payload):
            self._set_payload_numpy(np.array(payload))

    cdef _set_binary_payload(self,
                            void* payload, uint64_t schema, size_t nbytes, size_t array_length):
        self.c_obj.payloadType = NVTX_PAYLOAD_TYPE_EXT
        self.c_obj.payload.ullValue = <uint64_t>&self._payload_data
        self.c_obj.reserved0 = 1
        self._payload_data.schemaId = schema
        if array_length == 0:
            self._payload_data.size = nbytes
            self._payload_data.payload = payload
        else:
            payload_size = sizeof(uint64_t) + nbytes
            self._payload_data.size = payload_size
            self._payload_data.payload = self._allocated_payload = malloc(payload_size)
            if self._allocated_payload is NULL:
                raise MemoryError("Failed to allocate memory for payload")
            memcpy(self._allocated_payload, &array_length, sizeof(uint64_t))
            memcpy(<char*>self._allocated_payload + sizeof(uint64_t), payload, nbytes)

    cdef _clear_payload(self):
        self._payload = None
        self._payload_object = None
        if self._allocated_payload is not NULL:
            free(self._allocated_payload)
            self._allocated_payload = NULL


cdef class DomainHandle:

    def __init__(self, object name=None):
        if name is not None:
            self._name = _to_bytes(name)
            self.c_obj = nvtxDomainCreateA(
                self._name
            )
        else:
            self._name = b""
            self.c_obj = NULL

    @property
    def name(self):
        return self._name.decode()

    def enabled(self):
        return bool(nvtxDomainIsEnabled(self.c_obj))

    def __dealloc__(self):
        nvtxDomainDestroy(self.c_obj)


class RegisteredString:
    """
    A wrapper class for ``nvtxStringHandle_t`` C struct.
    Use :func:`nvtx.Domain.get_registered_string` to create an instance.
    """
    def __init__(self, domain, string=None):
        self.string = string
        self.domain = domain
        self.handle = StringHandle(domain, string)


class DummyDomain:
    """
    A replacement for :class:`nvtx.Domain` when the domain is disabled.
    (e.g., when no tool is attached).
    """
    def get_registered_string(self, string):
        pass

    def get_category_id(self, name):
        pass

    def get_event_attributes(self, message=None, color=None, category=None, payload=None):
        pass

    def set_event_attributes(self, EventAttributes attributes, *,
                             message=None, color=None, category=None, payload=None):
        pass

    def mark(self, EventAttributes attributes=None, *, message=None, color=None, category=None,
             payload=None):
        pass

    def push_range(self, EventAttributes attributes=None, *, message=None, color=None,
                   category=None, payload=None):
        pass

    def pop_range(self):
        pass

    def start_range(self, EventAttributes attributes=None, *, message=None, color=None,
                    category=None, payload=None):
        return 0

    def end_range(self, nvtxRangeId_t range_id):
        pass

    def get_counter(
        self,
        name,
        dtype,
        *,
        description=None,
        scope=None,
        semantics=None,
        time_domain=TimestampType.TOOL_PROVIDED,
    ) -> DummyCounter:
        _normalize_counter_dtype(dtype) # Validate that the user-provided dtype is supported.
        return dummy_counter

    def get_timestamp(self):
        return 0


dummy_domain = DummyDomain()
dummy_counter.domain = dummy_domain

# A sentinel value to indicate that the argument should not be set.
# Used in `Domain.set_event_attributes` to allow setting fields to None.
DONT_SET = object()

cdef class SchemaRegistrar:
    """
    Builds payload schemas from NumPy dtypes and registers them, caching so
    each distinct schema registers once. Subclasses override :meth:`_do_register`
    to supply the register target (global ``nvtxPayloadSchemaRegister`` for
    instrumentation, ``SchemaRegister`` for the NVTXW writer).
    """

    def __cinit__(self, *args, **kwargs):
        # Per-domain schema dedup caches.
        self._dtype_cache = {}
        self._array_cache = {}
        self._fixed_cache = {}

    cdef uint64_t _do_register(
        self, const nvtxPayloadSchemaAttr_t* attr
    ) except *:
        raise NotImplementedError(
            "SchemaRegistrar subclasses must implement _do_register")

    def _register_builtin_schema(self, dt, metadata):
        name = dt.name.encode()
        cdef nvtxSemanticsCounter_t counter_semantics
        cdef const nvtxSemanticsHeader_t* semantics = NULL
        cdef object counter_semantics_obj = _counter_semantics_from_metadata(metadata)
        if counter_semantics_obj is not None:
            _fill_counter_semantics(&counter_semantics, counter_semantics_obj)
            semantics = &counter_semantics.header

        array_length = 0
        flags = NVTX_PAYLOAD_ENTRY_FLAG_UNUSED
        entry_type = _entry_type_from_metadata(metadata)
        if entry_type is None:
            entry_type = _dtype_to_entry_type[dt.type]
        if entry_type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32:
            array_length = dt.itemsize // 4
        elif entry_type == NVTX_PAYLOAD_ENTRY_TYPE_BYTE:
            array_length = dt.itemsize
            flags = NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE
        flags |= _entry_flags_from_metadata(metadata)  # entry-role flags

        cdef nvtxPayloadSchemaEntry_t schemaEntry = nvtxPayloadSchemaEntry_t(
            flags=flags,
            type=entry_type,
            name=name,
            description=NULL,
            arrayOrUnionDetail=array_length,
            offset=0,
            semantics=semantics,
            reserved=NULL,
        )

        cdef nvtxPayloadSchemaAttr_t schemaAttr
        schemaAttr.fieldMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE | \
            NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES | \
            NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE
        schemaAttr.flags = 0
        schemaAttr.type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC
        schemaAttr.entries = &schemaEntry
        schemaAttr.numEntries = 1
        schemaAttr.payloadStaticSize = dt.itemsize
        return self._do_register(&schemaAttr)

    def _register_structured_schema(self, schema_key):
        cdef bint counter_group = schema_key.counter_group
        dt = schema_key.dtype
        names = []
        cdef const nvtxSemanticsHeader_t* semantics
        cdef nvtxPayloadSchemaAttr_t schemaAttr
        schemaAttr.fieldMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE | \
            NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES | \
            NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES | \
            NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE
        schemaAttr.name = NULL
        schemaAttr.flags = 0
        cdef uint64_t schema_flags = schema_key.schema_flags
        if counter_group:
            schema_flags |= NVTX_PAYLOAD_SCHEMA_FLAG_COUNTER_GROUP
        if schema_flags:
            schemaAttr.fieldMask |= NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_FLAGS
            schemaAttr.flags = schema_flags
        # Counter groups and writer event schemas are flat: scalar fields map
        # to predefined entry types rather than nested per-field sub-schemas.
        cdef bint flat = counter_group or schema_key.schema_flags != 0
        schemaAttr.type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC
        # dt.names, not dt.fields: the fields dict also contains title
        # aliases, which would duplicate titled fields in the schema.
        schemaAttr.numEntries = len(dt.names)
        schemaAttr.payloadStaticSize = dt.itemsize

        cdef nvtxPayloadSchemaEntry_t* schemaEntries = \
            <nvtxPayloadSchemaEntry_t*>malloc(len(dt.names) * sizeof(nvtxPayloadSchemaEntry_t))
        if schemaEntries is NULL:
            raise MemoryError("Failed to allocate memory for schema entries")
        cdef nvtxSemanticsCounter_t* semanticsEntries = \
            <nvtxSemanticsCounter_t*>malloc(len(dt.names) * sizeof(nvtxSemanticsCounter_t))
        if semanticsEntries is NULL:
            free(schemaEntries)
            raise MemoryError("Failed to allocate memory for schema semantics")
        try:
            schemaAttr.entries = schemaEntries
            for i, field_name in enumerate(dt.names):
                field_type, offset = dt.fields[field_name][:2]
                array_length = 0
                flags = NVTX_PAYLOAD_ENTRY_FLAG_UNUSED
                field_schema_key = PayloadSchemaKey(field_type)
                semantics = NULL

                if flat and field_type.subdtype:
                    subdtype, shape = field_type.subdtype
                    if subdtype.type in _dtype_to_entry_type:
                        entry_type = _dtype_to_entry_type[subdtype.type]
                        array_length = np.prod(shape)
                        flags = NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE
                    else:
                        entry_type = self._get_numpy_dtype_schema(field_schema_key)
                elif flat:
                    entry_type = _entry_type_from_metadata(
                        field_schema_key.metadata
                    )
                    if entry_type is None:
                        if field_type.type in _dtype_to_entry_type:
                            entry_type = _dtype_to_entry_type[field_type.type]
                        else:
                            entry_type = self._get_numpy_dtype_schema(
                                field_schema_key
                            )
                else:
                    entry_type = self._get_numpy_dtype_schema(field_schema_key)

                if entry_type == NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32:
                    array_length = field_type.itemsize // 4
                elif entry_type == NVTX_PAYLOAD_ENTRY_TYPE_BYTE:
                    array_length = field_type.itemsize
                    flags = NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE

                field_counter_semantics = _counter_semantics_from_metadata(
                    field_schema_key.metadata
                )
                if field_counter_semantics is not None:
                    _fill_counter_semantics(
                        &semanticsEntries[i], field_counter_semantics
                    )
                    semantics = &semanticsEntries[i].header

                metadata_flags = _entry_flags_from_metadata(
                    field_schema_key.metadata
                )
                flags |= metadata_flags
                if counter_group and not (
                    metadata_flags & NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP
                ):
                    flags |= NVTX_PAYLOAD_ENTRY_FLAG_COUNTER

                name = field_name.encode()
                names.append(name)
                schemaEntries[i] = nvtxPayloadSchemaEntry_t(
                    flags=flags,
                    type=entry_type,
                    name=name,
                    description=NULL,
                    arrayOrUnionDetail=array_length,
                    offset=offset,
                    semantics=semantics,
                    reserved=NULL,
                )
            return self._do_register(&schemaAttr)
        finally:
            free(schemaEntries)
            free(semanticsEntries)

    def _get_numpy_array_schema(self, uint64_t scalar_schema):
        cached = self._array_cache.get(scalar_schema)
        if cached is not None:
            return cached
        field_names = [b'size', b'data']

        size_entry = nvtxPayloadSchemaEntry_t(
            flags=NVTX_PAYLOAD_ENTRY_FLAG_UNUSED,
            type=NVTX_PAYLOAD_ENTRY_TYPE_UINT64,
            name=field_names[0],
            description=NULL,
            arrayOrUnionDetail=0,
            offset=0,
            semantics=NULL,
            reserved=NULL,
        )
        data_entry = nvtxPayloadSchemaEntry_t(
            flags=NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX,
            type=scalar_schema,
            name=field_names[1],
            description=NULL,
            arrayOrUnionDetail=0,
            offset=0,
            semantics=NULL,
            reserved=NULL,
        )
        cdef nvtxPayloadSchemaEntry_t[2] entries = [size_entry, data_entry]

        cdef nvtxPayloadSchemaAttr_t schemaAttr
        schemaAttr.fieldMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE | \
            NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES
        schemaAttr.type = NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC
        schemaAttr.entries = entries
        schemaAttr.numEntries = 2
        result = self._do_register(&schemaAttr)
        self._array_cache[scalar_schema] = result
        return result

    def _get_fixed_size_array_schema(self, uint64_t scalar_schema, size_t array_length, size_t size):
        cache_key = (scalar_schema, array_length, size)
        cached = self._fixed_cache.get(cache_key)
        if cached is not None:
            return cached
        cdef nvtxPayloadSchemaEntry_t entry = nvtxPayloadSchemaEntry_t(
            flags=NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE,
            type=scalar_schema,
            name=NULL,
            description=NULL,
            arrayOrUnionDetail=array_length,
            offset=0,
            semantics=NULL,
            reserved=NULL,
        )

        cdef nvtxPayloadSchemaAttr_t schemaAttr
        schemaAttr.fieldMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE | \
            NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES | NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES | \
            NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE
        schemaAttr.type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC
        schemaAttr.payloadStaticSize = size
        schemaAttr.entries = &entry
        schemaAttr.numEntries = 1
        result = self._do_register(&schemaAttr)
        self._fixed_cache[cache_key] = result
        return result

    def _get_numpy_dtype_schema(self, schema_key):
        cached = self._dtype_cache.get(schema_key)
        if cached is not None:
            return cached
        dt = schema_key.dtype
        if dt.subdtype:
            subdtype = dt.subdtype[0]
            subdtype_schema = self._get_numpy_dtype_schema(
                PayloadSchemaKey(subdtype)
            )
            result = self._get_fixed_size_array_schema(
                subdtype_schema, np.prod(dt.shape), dt.itemsize
            )
        elif dt.type in _dtype_to_entry_type:
            result = self._register_builtin_schema(dt, schema_key.metadata)
        else:
            result = self._register_structured_schema(schema_key)
        self._dtype_cache[schema_key] = result
        return result


cdef class _GlobalSchemaRegistrar(SchemaRegistrar):
    """Registers schemas via the global ``nvtxPayloadSchemaRegister``."""

    cdef nvtxDomainHandle_t _domain

    def __init__(self, DomainHandle handle):
        self._domain = handle.c_obj

    cdef uint64_t _do_register(
        self, const nvtxPayloadSchemaAttr_t* attr
    ) except *:
        return nvtxPayloadSchemaRegister(self._domain, attr)


class Domain:
    """
    A class that provides an interface to NVTX API per domain,
    and produces less overhead than using the global functions from ``nvtx`` module.

    Notes
    -----
    - Use :func:`nvtx.get_domain` to create an instance.
    - If the domain is disabled (e.g., when no tool is attached),
      the instance returned is a :class:`nvtx._lib.lib.DummyDomain`.
    """

    def __new__(cls, name: Optional[str] = None):
        handle = DomainHandle(name)
        if handle.enabled():
            obj = super().__new__(cls)
            obj.handle = handle
            return obj
        else:
            return dummy_domain

    def __init__(self, name: Optional[str] = None):
        self.name = name
        self.categories = {}
        self._schema_registrar = _GlobalSchemaRegistrar(self.handle)

    @lru_cache(maxsize=None)
    def get_registered_string(self, string) -> RegisteredString:
        """
        Register a given string under this domain (on first use), and return the handle.

        Parameters
        ----------
        string : str
            The string to be registered.
        """
        return RegisteredString(self.handle, string)

    @lru_cache(maxsize=None)
    def get_category_id(self, name) -> int:
        """
        Returns the category ID corresponding to the category `name`.
        On first use with a specific `name`, a new ID is assigned with the given name.

        Parameters
        ----------
        name : str
            The name of the category.
        """
        cdef DomainHandle dh = self.handle
        category_id = len(self.categories) + 1
        self.categories[name] = category_id
        nvtxDomainNameCategoryA(
            dh.c_obj,
            category_id,
            _to_bytes(name)
        )
        return category_id

    def get_event_attributes(self, message=None, color=None, category=None, payload=None
            )-> EventAttributes:
        """
        Create an :class:`nvtx._lib.lib.EventAttributes` object.

        Parameters
        ----------
        message : str
            A message associated with the event.
            If the given message was not registered then it will be registered under this domain.
        color : str, int, optional
            A color associated with the event.
            Supports `matplotlib` colors if it is available.
        category : str, int, optional
            A string or an integer specifying the category within the domain under which the event
            is scoped. If unspecified, the event is not associated with a category.
        payload : int, float, list, tuple, range, bytes, numpy.ndarray, optional
            A value associated with the event.
            Using payloads provides a separation between the message and the data of the event,
            which is often useful for analysis.
        """
        if isinstance(category, str):
            category = self.get_category_id(category)
        if message is not None:
            message = self.get_registered_string(message)
        return EventAttributes(self, message, color, category, payload)
    
    def set_event_attributes(self, EventAttributes attributes, *,
                             message=DONT_SET, color=DONT_SET, category=DONT_SET, payload=DONT_SET):
        """
        Set the attributes of an :class:`nvtx._lib.lib.EventAttributes` object.

        Parameters
        ----------
        attributes : EventAttributes, optional
            The event attributes to be set.
        message : str, RegisteredString, optional
            A message associated with the event.
            If the given message was not registered then it will be registered under this domain.
        color : str, int, optional
            A color associated with the event.
            Supports `matplotlib` colors if it is available.
        category : str, int, optional
            A string or an integer specifying the category within the domain under which the event
            is scoped. If unspecified, the event is not associated with a category.
        payload : int, float, list, tuple, range, bytes, numpy.ndarray, optional
            A value associated with the event.
            Using payloads provides a separation between the message and the data of the event,
            which is often useful for analysis.
        """
        if message is not DONT_SET:
            if isinstance(message, str):
                message = self.get_registered_string(message)
            attributes.message = message
        if color is not DONT_SET:
            attributes.color = color
        if category is not DONT_SET:
            if isinstance(category, str):
                category = self.get_category_id(category)
            attributes.category = category
        if payload is not DONT_SET:
            attributes.payload = payload

    def mark(self, EventAttributes attributes=None, *, **kwargs):
        """
        Mark an instantaneous event.

        Parameters
        ----------
        attributes : EventAttributes, optional
            The event attributes to be associated with the event.
            If not provided, a new :class:`EventAttributes` object is created
            with the given keyword arguments.
            Otherwise, this method mutates the attributes object if `kwargs` are provided.
        message : str, optional
            A message associated with the event.
            If the given message was not registered then it will be registered under this domain.
        color : str, int, optional
            A color associated with the event.
            Supports `matplotlib` colors if it is available.
        category : str, int, optional
            A string or an integer specifying the category within the domain under which the event
            is scoped. If unspecified, the event is not associated with a category.
        payload : int, float, list, tuple, range, bytes, numpy.ndarray, optional
            A value associated with the event.
            Using payloads provides a separation between the message and the data of the event,
            which is often useful for analysis.

        Examples
        --------
        >>> import nvtx
        >>> domain = nvtx.Domain('my_domain')
        >>> domain.mark(message='my_marker')

        Alternatively, an EventAttributes object can be reused:

        >>> attributes = domain.get_event_attributes(message='my_marker')
        >>> domain.mark(attributes)
        >>> domain.mark(attributes, message='my_marker_2')
        """
        if attributes is None:
            attributes = self.get_event_attributes(**kwargs)
        elif kwargs:
            self.set_event_attributes(attributes, **kwargs)
        nvtxDomainMarkEx((<DomainHandle>self.handle).c_obj, &attributes.c_obj)

    def push_range(self, EventAttributes attributes=None, *, **kwargs):
        """
        Mark the beginning of a code range.

        Parameters
        ----------
        attributes : EventAttributes, optional
            The event attributes to be associated with the event.
            If not provided, a new :class:`EventAttributes` object is created
            with the given keyword arguments.
            Otherwise, this method mutates the attributes object if `kwargs` are provided.
        message : str, optional
            A message associated with the event.
            If the given message was not registered then it will be registered under this domain.
        color : str, int, optional
            A color associated with the event.
            Supports `matplotlib` colors if it is available.
        category : str, int, optional
            A string or an integer specifying the category within the domain under which the event
            is scoped. If unspecified, the event is not associated with a category.
        payload : int, float, list, tuple, range, bytes, numpy.ndarray, optional
            A value associated with the event.
            Using payloads provides a separation between the message and the data of the event,
            which is often useful for analysis.
        
        Notes
        -----
        When applicable, prefer to use :class:`annotate`.

        Examples
        --------
        >>> import time
        >>> import nvtx
        >>> domain = nvtx.Domain('my_domain')
        >>> domain.push_range(message='my_code_range')
        >>> time.sleep(1)
        >>> domain.pop_range()

        Alternatively, an EventAttributes object can be reused:

        >>> attributes = domain.get_event_attributes(message='my_code_range')
        >>> domain.push_range(attributes)
        >>> domain.push_range(attributes, message='my_code_range_2')
        >>> time.sleep(1)
        >>> domain.pop_range()
        >>> domain.pop_range()
        """
        if attributes is None:
            attributes = self.get_event_attributes(**kwargs)
        elif kwargs:
            self.set_event_attributes(attributes, **kwargs)
        nvtxDomainRangePushEx((<DomainHandle>self.handle).c_obj, &attributes.c_obj)

    def pop_range(self):
        """
        Mark the end of a code range that was started with :func:`Domain.push_range`.
        """
        nvtxDomainRangePop((<DomainHandle>self.handle).c_obj)

    def start_range(self, EventAttributes attributes=None, *, **kwargs) -> int:
        """
        Mark the beginning of a process range.

        Parameters
        ----------
        attributes : EventAttributes, optional
            The event attributes to be associated with the event.
            If not provided, a new :class:`EventAttributes` object is created
            with the given keyword arguments.
            Otherwise, this method mutates the attributes object if `kwargs` are provided.
        message : str, optional
            A message associated with the event.
            If the given message was not registered then it will be registered under this domain.
        color : str, int, optional
            A color associated with the event.
            Supports `matplotlib` colors if it is available.
        category : str, int, optional
            A string or an integer specifying the category within the domain under which the event
            is scoped. If unspecified, the event is not associated with a category.
        payload : int, float, list, tuple, range, bytes, numpy.ndarray, optional
            A value associated with the event.
            Using payloads provides a separation between the message and the data of the event,
            which is often useful for analysis.

        Returns
        -------
        A numeric value that must be passed to :func:`Domain.end_range`.

        Examples
        --------
        >>> import time
        >>> import nvtx
        >>> domain = nvtx.Domain('my_domain')
        >>> range_id = domain.start_range(message='my_code_range')
        >>> time.sleep(1)
        >>> domain.end_range(range_id)

        Alternatively, an EventAttributes object can be reused:

        >>> attributes = domain.get_event_attributes(message='my_code_range')
        >>> range_id = domain.start_range(attributes)
        >>> time.sleep(1)
        >>> domain.end_range(range_id)
        >>> range_id = domain.start_range(attributes, message='my_code_range_2')
        >>> time.sleep(1)
        >>> domain.end_range(range_id)
        """
        if attributes is None:
            attributes = self.get_event_attributes(**kwargs)
        elif kwargs:
            self.set_event_attributes(attributes, **kwargs)
        return nvtxDomainRangeStartEx((<DomainHandle>self.handle).c_obj, &attributes.c_obj)

    def end_range(self, nvtxRangeId_t range_id):
        """
        Mark the end of a process range that was started with :func:`Domain.start_range`.

        Parameters
        ----------
        range_id : int
            The value returned by :func:`Domain.start_range`.
        """
        nvtxDomainRangeEnd((<DomainHandle>self.handle).c_obj, range_id)

    def get_counter(
        self,
        name,
        dtype,
        *,
        description=None,
        scope=None,
        semantics=None,
        time_domain=TimestampType.TOOL_PROVIDED,
    ) -> Counter:
        """
        Get an NVTX counter in this domain.

        For enabled domains, the counter is created on first use and cached.
        Subsequent calls with the same arguments return the same counter
        object.

        Parameters
        ----------
        name : str or bytes
            Display name for the counter.
        dtype : int, float, or numpy.dtype
            Counter value type. ``int`` records signed 64-bit integer
            samples, ``float`` records double-precision samples, and a
            NumPy dtype-like object records samples with that dtype.
            Structured dtypes represent flat counter groups.
        description : str or bytes, optional
            Longer description for the counter.
        scope : str or bytes, optional
            Scope path for the counter.
        semantics : CounterSemantics, optional
            Semantics for the counter as a whole. For per-field semantics
            in a structured dtype, build the field dtype with
            :func:`nvtx.numpy_dtype`. Dtype metadata applies to schema entries,
            so top-level scalar counters should use this argument instead
            of ``nvtx.numpy_dtype(..., counter_semantics=...)``.
        time_domain : TimestampType, optional
            Timestamp domain used for batched samples.

        Returns
        -------
        Counter
            A counter whose concrete sample type is determined by ``dtype``,
            one of :class:`Int64Counter`, :class:`Float64Counter`, or
            :class:`ExtCounter`.
        """

        dtype = _normalize_counter_dtype(dtype)
        schema_key = PayloadSchemaKey(
            dtype, counter_group=getattr(dtype, "fields", None) is not None
        )
        return self._get_counter_cached(
            name,
            schema_key,
            description,
            scope,
            semantics,
            time_domain.value,
        )

    @lru_cache(maxsize=None)
    def _get_scope_id(self, path) -> int:
        cdef bytes path_bytes = _to_bytes(path)
        cdef nvtxScopeAttr_t attr
        attr.structSize = sizeof(nvtxScopeAttr_t)
        attr.path = path_bytes
        attr.parentScope = NVTX_SCOPE_NONE
        attr.scopeId = NVTX_SCOPE_NONE
        return nvtxScopeRegister((<DomainHandle>self.handle).c_obj, &attr)

    @lru_cache(maxsize=None)
    def _get_counter_cached(
        self,
        name,
        schema_key,
        description,
        scope,
        semantics,
        uint64_t time_domain,
    ):
        dtype = schema_key.dtype
        counter_cls = _counter_class_from_dtype(dtype)
        return counter_cls(
            self,
            name,
            dtype,
            description,
            scope,
            semantics,
            time_domain,
        )

    def get_timestamp(self):
        """
        Return an NVTX timestamp for use with batched counter samples.
        """

        return nvtxTimestampGet()

    def _get_numpy_dtype_schema(self, schema_key):
        # Schema building/caching lives on the domain's SchemaRegistrar.
        return self._schema_registrar._get_numpy_dtype_schema(schema_key)

    def _get_numpy_array_schema(self, scalar_schema):
        return self._schema_registrar._get_numpy_array_schema(
            scalar_schema
        )


cdef class StringHandle:

    def __init__(self, DomainHandle domain_handle, object string=None):
        if string is not None:
            self._string = _to_bytes(string)
            self.c_obj = nvtxDomainRegisterStringA(
                domain_handle.c_obj, self._string
            )
        else:
            self._string = b""
            self.c_obj = NULL

    @property
    def string(self):
        return self._string.decode()


def push_range(EventAttributes attributes, DomainHandle domain):
    nvtxDomainRangePushEx(domain.c_obj, &attributes.c_obj)


def pop_range(DomainHandle domain):
    nvtxDomainRangePop(domain.c_obj)


def start_range(EventAttributes attributes, DomainHandle domain):
    return nvtxDomainRangeStartEx(domain.c_obj, &attributes.c_obj), domain


def end_range(nvtxRangeId_t range_id, DomainHandle domain):
    nvtxDomainRangeEnd(domain.c_obj, range_id)


def mark(EventAttributes attributes, DomainHandle domain):
    nvtxDomainMarkEx(domain.c_obj, &attributes.c_obj)
