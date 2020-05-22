# Copyright (c) 2020, NVIDIA CORPORATION.

from nvtx._lib.lib cimport *
from nvtx.colors import color_to_hex
from nvtx.utils.cached import CachedInstanceMeta

cdef extern from "Python.h":
    wchar_t* PyUnicode_AsWideCharString(object, Py_ssize_t *)

def initialize():
    nvtxInitialize(NULL)


cdef class EventAttributes:
    cdef unicode _message
    cdef object _color
    cdef nvtxEventAttributes_t c_obj

    def __init__(self, unicode message=None, color="blue"):
        if message is None:
            message = ""
        self._message = message
        self._color = color
        self.c_obj = nvtxEventAttributes_t(0)
        self.c_obj.version = NVTX_VERSION
        self.c_obj.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE
        self.c_obj.colorType = NVTX_COLOR_ARGB
        self.c_obj.color = color_to_hex(self._color)
        self.c_obj.messageType = NVTX_MESSAGE_TYPE_UNICODE
        self.c_obj.message.unicode = PyUnicode_AsWideCharString(self._message, NULL)

    @property
    def message(self):
        return self._message

    @message.setter
    def message(self, unicode value):
        self._message = value
        self.c_obj.message.unicode = PyUnicode_AsWideCharString(self._message, NULL)

    @property
    def color(self):
        return self._color

    @color.setter
    def color(self, value):
        self._color = value
        self.c_obj.color = color_to_hex(self._color)

        
cdef class DomainHandle:
    cdef unicode _name
    cdef nvtxDomainHandle_t c_obj

    def __init__(self, unicode name=None):
        if name is not None:
            self._name = name
            self.c_obj = nvtxDomainCreateW(
                PyUnicode_AsWideCharString(self._name, NULL)
            )
        else:
            self._name = None
            self.c_obj = NULL

    def name(self):
        return self._name
            
    def __dealloc__(self):
        nvtxDomainDestroy(self.c_obj)


cdef class RangeId:
    cdef nvtxRangeId_t c_obj

    def __cinit__(self, uint64_t range_id):
        self.c_obj = range_id


class Domain(metaclass=CachedInstanceMeta):
    def __init__(self, name=None):
        self.name = name
        self.handle = DomainHandle(name)


def push_range(EventAttributes attributes, DomainHandle domain):
    nvtxDomainRangePushEx(domain.c_obj, &attributes.c_obj)


def pop_range(DomainHandle domain):
    nvtxDomainRangePop(domain.c_obj)


def mark(EventAttributes attributes, DomainHandle domain):
    nvtxDomainMarkEx(domain.c_obj, &attributes.c_obj)
