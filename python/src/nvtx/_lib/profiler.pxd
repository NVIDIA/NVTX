from nvtx._lib.lib cimport nvtxEventAttributes_t, nvtxDomainHandle_t


cdef class Profile:
    cdef nvtxDomainHandle_t __domain
    cdef nvtxEventAttributes_t __attrib

    cdef bint linenos
    cdef bint annotate_cfuncs

    cdef push_range(self, message)
    cdef pop_range(self)
