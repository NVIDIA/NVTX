import os

from nvtx import push_range, pop_range


def _message(frame, arg, event, annotate_cfuncs, linenos):
    message = None
    if event  == "call":
        name = frame.f_code.co_name
        if linenos:
            fname = frame.f_code.co_filename
            lineno = frame.f_lineno
            message = f"{fname}:{lineno}(fname)"
        else:
            message = name
    elif event == "c_call" and annotate_cfuncs:
        message = arg.__name__
    return message


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
        if event in {"call", "c_call"}:
            push_range(
                message=_message(
                    frame,
                    arg,
                    event,
                    annotate_cfuncs=annotate_cfuncs,
                    linenos=linenos
                )
            )
        elif event in {"return", "c_return", "c_exception"}:
            pop_range()
        return None
    return profile
