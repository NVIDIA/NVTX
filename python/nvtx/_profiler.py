import os
import  sys

from nvtx._lib import (
    push_range as libnvtx_push_range,
    pop_range as libnvtx_pop_range,
    EventAttributes,
    Domain
)


class Profiler:

    def __init__(self, linenos=True, annotate_cfuncs=True):
        self.linenos = linenos
        self.annotate_cfuncs = annotate_cfuncs
        self.__domain = Domain("nvtx.py").handle
        self.__attrib = EventAttributes("", "blue", None)

    def profile(self, frame, event, arg):
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

    def push_range(self, message):
        self.__attrib.message = message
        libnvtx_push_range(self.__attrib, self.__domain)

    def pop_range(self):
        libnvtx_pop_range(self.__domain)

    def enable(self):
        sys.setprofile(self.profile)

    def disable(self):
        sys.setprofile(None)
