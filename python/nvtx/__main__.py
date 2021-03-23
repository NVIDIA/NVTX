import sys
from runpy import run_path
from optparse import OptionParser

def main():
    from nvtx import profiler

    usage = "nvtx script args ..."
    parser = OptionParser(usage)
    options, args = parser.parse_args()
    script_file = args[0]

    sys.argv = args

    sys.setprofile(profiler)
    run_path(script_file)

main()
