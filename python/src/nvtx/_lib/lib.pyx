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


cpdef bytes _to_bytes(object s):
    return s if isinstance(s, bytes) else s.encode()

def initialize():
    nvtxInitialize(NULL)


cdef class EventAttributes:

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
    def __init__(self, domain, string=None):
        self.string = string
        self.domain = domain
        self.handle = StringHandle(domain, string)

class DummyDomain:
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
    def __new__(cls, name=None):
        handle = DomainHandle(name)
        if handle.enabled():
            obj = super().__new__(cls)
            obj.handle = handle
            return obj
        else:
            return dummy_domain

    def __init__(self, name=None):
        self.name = name
        self.categories = {}

    @lru_cache(maxsize=None)
    def get_registered_string(self, string):
        return RegisteredString(self.handle, string)

    @lru_cache(maxsize=None)
    def get_category_id(self, name):
        """
        Returns the category ID corresponding to the category `name`.
        If no category `name` exists, it is added to the domain,
        and the corresponding id.
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
    def get_event_attributes(self, message=None, color='blue', category=None, payload=None):
        if isinstance(category, str):
            category = self.get_category_id(category)
        return EventAttributes(self.get_registered_string(message), color, category, payload)

    def mark(self, EventAttributes attributes):
        nvtxDomainMarkEx((<DomainHandle>self.handle).c_obj, &attributes.c_obj)

    def push_range(self, EventAttributes attributes):
        nvtxDomainRangePushEx((<DomainHandle>self.handle).c_obj, &attributes.c_obj)

    def pop_range(self):
        nvtxDomainRangePop((<DomainHandle>self.handle).c_obj)

    def start_range(self, EventAttributes attributes):
        return nvtxDomainRangeStartEx((<DomainHandle>self.handle).c_obj, &attributes.c_obj)

    def end_range(self, nvtxRangeId_t range_id):
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
