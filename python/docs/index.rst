.. nvtx documentation master file, created by
   sphinx-quickstart on Wed May  6 16:27:08 2020.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

================================================
nvtx - Annotate code ranges and events in Python
================================================

``nvtx`` lets you annotate your Python code so that it can be analyzed and visualized
using `NVIDIA Nsight Systems <https://developer.nvidia.com/nsight-systems>`_.
This enables generating detailed timelines of execution
of Python programs for the purposes of debugging and optimization.

.. image:: images/timeline.png
    :align: center


A quick example
===============

The ``annotate()`` function is used to annotate a function or a snippet of code.
It can be used both as a decorator or a context manager:
::

   # demo.py

   import time
   import nvtx

   @nvtx.annotate(color="blue")
   def my_function():
       for i in range(5):
           with nvtx.annotate("my_loop", color="red"):
               time.sleep(i)

   my_function()


Adding ``nvtx`` annotations to your code doesn't achieve anything by itself.
To derive something useful from annotated code, you'll need to use a third-party application
such as `Nsight Systems <https://developer.nvidia.com/nsight-systems>`_, which
helps you visualize the execution of your code:
::

   nsight-sys profile -t nvtx python demo.py


This will produce a ``.qdrep`` file that can be opened in the Nsight systems GUI to view
the timeline:

.. image:: images/demo_timeline.png
    :align: center


Contents
========

.. include:: toctree.rst


Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
