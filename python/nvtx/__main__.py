import sys
from runpy import run_path
from optparse import OptionParser

def main():
    from nvtx import Profile

    usage = "%prog [options] scriptfile [args] ..."

    parser = OptionParser(usage)
    parser.add_option(
        "--linenos",
        action="store_true",
        dest="linenos",
        default=True,
        help="Include file and line number information in annotations. Otherwise, "
              "only the function name is used."
    )
    parser.add_option(
        "--no-linenos",
        action="store_false",
        dest="linenos",
        default=True,
        help="Do not include file and line number information in annotations."
    )
    parser.add_option(
        "--annotate-cfuncs",
        action="store_true",
        dest="annotate_cfuncs",
        default=False,
        help="Also annotate C-extension and builtin functions. [default: %default]",
    )

    options, args = parser.parse_args()
    script_file = args[0]

    sys.argv = args
    profiler = Profile(
        linenos=options.linenos,
        annotate_cfuncs=options.annotate_cfuncs
    )

    profiler.enable()

    try:
        run_path(script_file)
    finally:
        profiler.disable()

if __name__ == "__main__":
    main()
