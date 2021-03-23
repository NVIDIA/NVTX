from nvtx import push_range, pop_range

def profiler(frame, event, args):
    if event == "call":
        push_range(message=frame.f_code.co_name)
    elif event == "return":
        pop_range()
    return None
