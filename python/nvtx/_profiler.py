import os

from nvtx import push_range, pop_range


def profiler(linenos=True, annotate_cfuncs=False):
    """
    Parameters
    ----------
    linenos: bool
        If True, include file and line number information
    annotate_cfuncs: bool
        If True, also annotate calls to extension and builtin functions
    """
    def profile(frame, event, arg):
        # profile function meant to be used with sys.setprofile
        if event == "call":
            name = frame.f_code.co_name
            if linenos:
                fname = os.path.basename(frame.f_code.co_filename)
                lineno = frame.f_lineno
                message = f"{fname}:{lineno}({name})"
            else:
                message = name
            push_range(message)
        elif event == "c_call" and annotate_cfuncs:
            message = arg.__name__
            push_range(message)
        elif event == "return":
            pop_range()
        elif event in {"c_return", "c_exception"} and annotate_cfuncs:
            pop_range()
        return None
    return profile
