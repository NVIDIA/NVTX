from libcpp cimport bool

from nvtx._lib.lib cimport EventAttributes, DomainHandle


cdef class Profiler:
    cdef DomainHandle __domain
    cdef EventAttributes __attrib

    cdef bint linenos
    cdef bint annotate_cfuncs

    cdef push_range(self, message)
    cdef pop_range(self)
