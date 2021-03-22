import sys
from runpy import run_path


def profiler(frame, event, args):
    from nvtx import push_range, pop_range

    if event == "call":
        push_range(message=frame.f_code.co_name)
    if event == "return":
        pop_range()
    return None

def main():
    script_file = sys.argv[1]
    sys.argv = sys.argv[1:]
    run_path(script_file)

main()
