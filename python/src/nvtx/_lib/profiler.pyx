import os
import sys
import threading

from nvtx._lib.lib import _to_bytes
from nvtx.colors import _NVTX_COLORS

from nvtx._lib.lib cimport *

DEFAULT_COLOR = _NVTX_COLORS[None]

cdef class Profile:
    """
    Class for programmatically controlling NVTX automatic annotations.

    Parameters
    ----------
    linenos
        Include file and line number information in annotations.
    annotate_cfuncs
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
    def __init__(self, linenos: bool = True, annotate_cfuncs: bool = True):
        
        self.linenos = linenos
        self.annotate_cfuncs = annotate_cfuncs
        self.__domain = nvtxDomainCreateA(b"nvtx.py")
        self.__attrib = nvtxEventAttributes_t(0)
        self.__attrib.version = NVTX_VERSION
        self.__attrib.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE
        self.__attrib.colorType = NVTX_COLOR_ARGB
        self.__attrib.color = DEFAULT_COLOR
        self.__attrib.messageType = NVTX_MESSAGE_TYPE_REGISTERED

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
        self.__attrib.message.registered = nvtxDomainRegisterStringA(
            self.__domain, _to_bytes(message))
        nvtxDomainRangePushEx(self.__domain, &self.__attrib)

    cdef pop_range(self):
        nvtxDomainRangePop(self.__domain)

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
