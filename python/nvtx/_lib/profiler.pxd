from libcpp cimport bool

from nvtx._lib.lib cimport EventAttributes, DomainHandle


cdef class Profiler:
    cdef bint linenos
    cdef bint annotate_cfuncs
    cdef DomainHandle __domain
    cdef EventAttributes __attrib
