This test suite builds an executable called "runtest" and several dynamic libraries.
The dynamic libraries serve as tests, NVTX injections, or both.  To invoke "runtest",
specify the desired test library using -t and the desired injection library using -i.
For example:
    runtest -t coverage -i inj
...will run the "coverage" test using "inj" for the injection.  The tests can be run
without any injection to see what they do without NVTX.  Some test libraries are self-
injecting, i.e. they serve as both the test and the injection.  Specifying "-i -" for
the injection option instructs runtest to use the test library as the injection also.
Additional arguments are forwarded to the test.  For example:
    runtest -t calls -i - -v
...will use the "calls" library for both the test and the injection, and will pass
the "-v" argument to the calls test.

Some tests include compile-time negative tests, guarded by #ifs.  By defining macros
like ERROR_TEST_NAME_IS_MISSING, the tests should fail to compile with the expected
error message.  In these cases, successful compilation or emitting the wrong error
message should be considered a failure of the test.

Here are the dynamic libraries, and X in columns to show which usage they support:

                 Test? Injection? Description
attributes         X              - Use NVTX C++ API for setting event attributes
calls              X       X      - Use self-injection to test C/C++ APIs call handlers with all parameters correctly
categories         X              - Use NVTX C++ API for naming categories
coverage           X              - Use all features of NVTX C++ API
coveragec          X              - Use all features of NVTX C API
coverage-counter   X              - Use all features of NVTX C API Extension for Counters
coverage-cu        X              - Use all features of NVTX C++ API from a .cu file (i.e. use nvcc instead of host cc)
coverage-mem       X              - Use all features of NVTX C API Extension for Memory Naming
coverage-memcudart X              - Use all features of NVTX C API Extension for Memory Naming (using CUDART types)
coverage-payload   X              - Use all features of NVTX C API Extension for Payloads
domains            X              - Use NVTX C++ API for creating domains
inj                        X      - A simple injection that prints messages when NVTX functions are called
linkerdupes        X              - Compile-time tests to ensure multiple libraries using NVTX don't conflict
regstrings         X              - Use NVTX C++ API for registering strings
self               X       X      - Use self-injection to demonstrate the self-injection mechanism is working
