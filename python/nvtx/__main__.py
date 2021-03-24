import sys
from runpy import run_path
from optparse import OptionParser

def main():
    from nvtx import profiler

    usage = "nvtx script args ..."
    parser = OptionParser(usage)
    parser.add_option("--profile_cfuncs", action="store_true", dest="profile_cfuncs")
    options, args = parser.parse_args()
    script_file = args[0]

    sys.argv = args

    sys.setprofile(profiler(profile_cfuncs=options.profile_cfuncs))
    run_path(script_file)
    sys.setprofile(None)

main()
