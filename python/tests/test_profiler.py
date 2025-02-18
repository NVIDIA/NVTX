from .cython.test_profiler import test_profiler_message as _test_profiler_message


def test_profiler():
    _test_profiler_message()
