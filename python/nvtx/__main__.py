import sys
from runpy import run_path
from optparse import OptionParser

def main():
    from nvtx import profiler

    usage = "%prog [options] script args ..."
    parser = OptionParser(usage, prog="python -m nvtx")
    parser.add_option(
        "--profile_cfuncs",
        action="store_true",
        dest="profile_cfuncs",
        default=False,
        help="Also profile C-extension and builtin functions [default: %default]",
    )
    parser.add_option(
        "--linenos",
        action="store_true",
        dest="linenos",
        default=False,
        help="Include file and line number information [default: %default]"
    )

    options, args = parser.parse_args()
    script_file = args[0]

    sys.argv = args

    sys.setprofile(profiler(profile_cfuncs=options.profile_cfuncs))
    run_path(script_file)
    sys.setprofile(None)

main()
