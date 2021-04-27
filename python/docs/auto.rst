Automatic function annotation
=============================

You can use ``nvtx`` to automatically annotate each function call in
your program. This can give you lots of useful information about your
program.  Note that this adds a tiny amount of overhead to each and every
function invocation, and can significantly impact the overall runtime
(by about 10x or more).


Command-line interface
----------------------

You can invoke ``nvtx`` as a command-line script, which annotates every function call,
with no changes to the source code:


::

   python -m nvtx script.py


The ``Profile`` class
---------------------

You can also use ``Profile`` to enable and disable
automatic function annotation in different parts of
your program:


::

   pr = nvtx.Profile()
   pr.enable()  # begin annotating function calls
   # -- do something -- #
   pr.disable()  # stop annotating function calls
