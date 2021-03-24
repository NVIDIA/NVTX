from nvtx import push_range, pop_range


def profiler(profile_cfuncs=False):
    """
    TODO
    """
    def profile(frame, event, arg):
        if event == "call":
            push_range(message=frame.f_code.co_name)
        elif event == "return":
            pop_range()
        elif event == "c_call" and profile_cfuncs:
            push_range(message=arg.__name__)
        elif event == "c_return" and profile_cfuncs:
            pop_range()
        elif event == "c_exception" and profile_cfuncs:
            pop_range()
        return None
    return profile
