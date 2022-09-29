# Copyright 2020-2022 NVIDIA Corporation.  All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

from libc.stdint cimport uint32_t

from nvtx._lib.lib cimport *
from nvtx.colors import color_to_hex
from nvtx.utils.cached import CachedInstanceMeta


cpdef bytes _to_bytes(object s):
    return s if isinstance(s, bytes) else s.encode()


def initialize():
    nvtxInitialize(NULL)


cdef class EventAttributes:

    def __init__(self, object message=None, color=None, category=None):
        self._color = color
        self.message = message
        self.c_obj = nvtxEventAttributes_t(0)
        self.c_obj.version = NVTX_VERSION
        self.c_obj.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE
        self.c_obj.colorType = NVTX_COLOR_ARGB
        self.c_obj.color = color_to_hex(self._color)
        self.c_obj.messageType = NVTX_MESSAGE_TYPE_ASCII
        self.c_obj.message.ascii = self._message
        if category is not None:
            self.c_obj.category = category

    @property
    def message(self):
        return self._message.decode()

    @message.setter
    def message(self, object value):
        if value is None:
            value = b""
        self._message = _to_bytes(value)
        self.c_obj.message.ascii = self._message

    @property
    def color(self):
        return self._color

    @color.setter
    def color(self, value):
        self._color = value
        self.c_obj.color = color_to_hex(self._color)


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

    def __dealloc__(self):
        nvtxDomainDestroy(self.c_obj)


class Domain(metaclass=CachedInstanceMeta):
    def __init__(self, name=None):
        self.name = name
        self.handle = DomainHandle(name)
        self.categories = {}

    def get_category_id(self, name):
        """
        Returns the category ID corresponding to the category `name`.
        If no category `name` exists, it is added to the domain,
        and the corresponding id.
        """
        cdef DomainHandle dh = self.handle
        if name not in self.categories:
            category_id = len(self.categories) + 1
            self.categories[name] = category_id
            nvtxDomainNameCategoryA(
                dh.c_obj,
                category_id,
                _to_bytes(name)
            )
        return self.categories[name]


cdef class RangeId:
    """
    Handle to code range created using start_range()
    """
    def __cinit__(self, nvtxRangeId_t range_id, DomainHandle domain):
        self.c_obj = range_id
        self.domain = domain


def push_range(EventAttributes attributes, DomainHandle domain):
    nvtxDomainRangePushEx(domain.c_obj, &attributes.c_obj)


def pop_range(DomainHandle domain):
    nvtxDomainRangePop(domain.c_obj)


def start_range(EventAttributes attributes, DomainHandle domain):
    cdef nvtxRangeId_t c_rng_id = nvtxDomainRangeStartEx(domain.c_obj, &attributes.c_obj)
    rng_id = RangeId(c_rng_id, domain)
    return rng_id


def end_range(RangeId range_id):
    nvtxDomainRangeEnd(range_id.domain.c_obj, range_id.c_obj)


def mark(EventAttributes attributes, DomainHandle domain):
    nvtxDomainMarkEx(domain.c_obj, &attributes.c_obj)
