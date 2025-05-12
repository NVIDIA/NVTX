# SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

from functools import lru_cache

from nvtx._lib.lib cimport *
from nvtx.colors import color_to_hex

from typing import Optional


cpdef bytes _to_bytes(object s):
    return s if isinstance(s, bytes) else s.encode()

def initialize():
    nvtxInitialize(NULL)


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
    payload : int
        A numeric value to be associated with this event.
    """

    def __init__(self, object message=None, color=None, category=None, payload=None):
        self.c_obj = nvtxEventAttributes_t(0)
        self.c_obj.version = NVTX_VERSION
        self.c_obj.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE
        self.c_obj.colorType = NVTX_COLOR_ARGB
        self.c_obj.messageType = NVTX_MESSAGE_TYPE_REGISTERED

        self.message = message
        self.color = color
        self.category = category
        self.payload = payload

    @property
    def message(self):
        return self._message

    @message.setter
    def message(self, object value):
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
        if value is not None:
            self._category = value
            self.c_obj.category = value

    @property
    def payload(self):
        return self._payload

    @payload.setter
    def payload(self, value):
        if value is not None:
            self._payload = value

            if isinstance(self._payload, int):
                self.c_obj.payload.llValue = self._payload
                self.c_obj.payloadType = NVTX_PAYLOAD_TYPE_INT64
            elif isinstance(self._payload, float):
                self.c_obj.payload.dValue = self._payload
                self.c_obj.payloadType = NVTX_PAYLOAD_TYPE_DOUBLE
            else:
                raise RuntimeError('Payload must be int or float')


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

    def mark(self, EventAttributes attributes):
        pass

    def push_range(self, EventAttributes attributes):
        pass

    def pop_range(self):
        pass

    def start_range(self, EventAttributes attributes):
        return 0

    def end_range(self, nvtxRangeId_t range_id):
        pass


dummy_domain = DummyDomain()

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

    @lru_cache(maxsize=None)
    def get_event_attributes(self, message=None, color=None, category=None, payload=None
            )-> EventAttributes:
        """
        Get or create an :class:`nvtx._lib.lib.EventAttributes` object.
        The results of this function are cached, i.e.,
        the same object is returned for the same parameters.

        Parameters
        ----------
        message : str
            A message associated with the event.
        color : str, int, optional
            A color associated with the event.
            Supports `matplotlib` colors if it is available.
        category : str, int, optional
            A string or an integer specifying the category within the domain under which the event
            is scoped. If unspecified, the event is not associated with a category.
        payload : int, float, optional
            A numeric value to be associated with this event.
        """
        if isinstance(category, str):
            category = self.get_category_id(category)
        return EventAttributes(self.get_registered_string(message), color, category, payload)

    def mark(self, EventAttributes attributes):
        """
        Mark an instantaneous event.

        Parameters
        ----------
        attributes : EventAttributes
            The event attributes to be associated with the event.

        Examples
        --------
        >>> import nvtx
        >>> domain = nvtx.Domain('my_domain')
        >>> attributes = domain.get_event_attributes(message='my_marker')
        >>> domain.mark(attributes)
        """
        nvtxDomainMarkEx((<DomainHandle>self.handle).c_obj, &attributes.c_obj)

    def push_range(self, EventAttributes attributes):
        """
        Mark the beginning of a code range.

        Parameters
        ----------
        attributes: EventAttributes
            The event attributes to be associated with the range.
        
        Notes
        -----
        When applicable, prefer to use :class:`annotate`.

        Examples
        --------
        >>> import time
        >>> import nvtx
        >>> domain = nvtx.Domain('my_domain')
        >>> attributes = domain.get_event_attributes(message='my_code_range')
        >>> domain.push_range(attributes)
        >>> time.sleep(1)
        >>> domain.pop_range()
        """
        nvtxDomainRangePushEx((<DomainHandle>self.handle).c_obj, &attributes.c_obj)

    def pop_range(self):
        """
        Mark the end of a code range that was started with :func:`Domain.push_range`.
        """
        nvtxDomainRangePop((<DomainHandle>self.handle).c_obj)

    def start_range(self, EventAttributes attributes) -> int:
        """
        Mark the beginning of a process range.

        Parameters
        ----------
        attributes : EventAttributes
            The event attributes to be associated with the range.

        Returns
        -------
        A numeric value that must be passed to :func:`Domain.end_range`.

        Examples
        --------
        >>> import time
        >>> import nvtx
        >>> domain = nvtx.Domain('my_domain')
        >>> attributes = domain.get_event_attributes(message='my_code_range')
        >>> range_id = domain.start_range(attributes)
        >>> time.sleep(1)
        >>> domain.end_range(range_id)
        """
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
