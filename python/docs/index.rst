.. nvtx documentation master file, created by
   sphinx-quickstart on Wed May  6 16:27:08 2020.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

================================================
nvtx - Annotate code ranges and events in Python
================================================

``nvtx`` lets you annotate your Python code so that it can be analyzed and visualized
using `NVIDIA Nsight Systems <https://developer.nvidia.com/nsight-systems>`_.
This gives you a detailed timeline of execution
of your Python program for the purposes of debugging and optimization.

.. image:: images/timeline.png
    :align: center

Quickstart
==========

``nvtx`` is both Python package and a command-line script.
The package lets you annotate specific parts of your code manually.
When invoked as a script, ``nvtx`` automatically annotates every function
call in your program.


Here is an example of code annotated with the ``nvtx`` package:

::

   # example_lib.py

   import time
   import nvtx


   def sleep_for(i):
       time.sleep(i)

   @nvtx.annotate()
   def my_func():
       time.sleep(1)

   with nvtx.annotate("for_loop", color="green"):
       for i in range(5):
           sleep_for(i)
           my_func()



Adding ``nvtx`` annotations to your code doesn't achieve anything by itself.
To derive something useful from annotated code,
you'll need to use a third-party application that supports NVTX annotations.
The command below uses the Nsight Systems command-line interface (CLI) to collect
information from the annotated code:

::

   nsys profile python demo.py

This produces a ``.qdrep`` file that you can use with the Nsight Systems GUI
to see a timeline of execution of your program:

.. image:: images/timeline_lib.png
    :align: center


You can also invoke ``nvtx`` as a script -- this will
automatically annotate every function call in your program.
This requires no changes to your own code.

::

   # example_cli.py

   import time

   def sleep_for(i):
       time.sleep(i)

   for i in range(5):
       sleep_for(i)
       sleep_for(1)


Simply add ``-m nvtx`` when running Python from the command-line:

::

   nsys profile -o example.qdrep python -m nvtx example_cli.py

Every function call is automatically annotated and appears on the command-line:

.. image:: images/timeline_cli.png
   :align: center


Contents
========

.. include:: toctree.rst


Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
