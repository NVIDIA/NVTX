import os
import  sys
import threading

from nvtx._lib import (
    push_range as libnvtx_push_range,
    pop_range as libnvtx_pop_range
)

from nvtx._lib.lib cimport EventAttributes, DomainHandle


cdef class Profile:

    def __init__(self, linenos=True, annotate_cfuncs=True):
        """
        Class that enables turning on and off automatic annotation.

        Parameters
        ----------
        linenos: bool (default True)
            Include file and line number information in annotations.
        annotate_cfuncs: bool (default False)
            Also annotate C-extensions and builtin functions.

        Examples
        --------
        >>> import nvtx
        >>> import time
        >>> pr = nvtx.Profile()
        >>> pr.enable()
        >>> time.sleep(1) # this call to `sleep` is captured by nvtx.
        >>> pr.disable()
        >>> time.sleep(1) # this one is not.
        """
        self.linenos = linenos
        self.annotate_cfuncs = annotate_cfuncs
        self.__domain = DomainHandle("nvtx.py")
        self.__attrib = EventAttributes("", "blue", None)

    def _profile(self, frame, event, arg):
        # profile function meant to be used with sys.setprofile
        if event == "call":
            name = frame.f_code.co_name
            if self.linenos:
                fname = os.path.basename(frame.f_code.co_filename)
                lineno = frame.f_lineno
                message = f"{fname}:{lineno}({name})"
            else:
                message = name
            self.push_range(message)
        elif event == "c_call" and self.annotate_cfuncs:
            self.push_range(arg.__name__)
        elif event == "return":
            self.pop_range()
        elif event in {"c_return", "c_exception"} and self.annotate_cfuncs:
            self.pop_range()
        return None

    cdef push_range(self, message):
        self.__attrib.message = message
        libnvtx_push_range(self.__attrib, self.__domain)

    cdef pop_range(self):
        libnvtx_pop_range(self.__domain)

    def enable(self):
        """Start annotating function calls automatically.
        """
        threading.setprofile(self._profile)
        sys.setprofile(self._profile)

    def disable(self):
        """Stop annotating function calls automatically.
        """
        threading.setprofile(None)
        sys.setprofile(None)
