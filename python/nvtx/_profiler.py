import os

from nvtx import push_range, pop_range


def _message(frame, arg, event, profile_cfuncs, linenos):
    message = None
    if event  == "call":
        name = frame.f_code.co_name
        if linenos:
            fname = frame.f_code.co_filename
            lineno = frame.f_lineno
            message = f"{fname}:{lineno}(fname)"
        else:
            message = name
    elif event == "c_call" and profile_cfuncs:
        message = arg.__name__
    return message


def profiler(profile_cfuncs=False, linenos=False):
    """
    Parameters
    ----------
    profile_cfuncs: bool
        If True, also profile calls to extension and builtin functions
    linenos: bool
        If True, include file and line number information
    """
    def profile(frame, event, arg):
        if event in {"call", "c_call"}:
            push_range(
                message=_message(
                    frame,
                    arg,
                    event,
                    profile_cfuncs=profile_cfuncs,
                    linenos=linenos
                )
            )
        elif event in {"return", "c_return", "c_exception"}:
            pop_range()
        return None
    return profile
