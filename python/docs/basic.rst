Basic Usage
===========

annotate
--------

The ``annotate()`` function annotates a code range, i.e., one or more statements.
Each annotation may have a message and a color associated with it.
This makes it easy to distinguish annotated ranges when visualizing them.
``annotate`` can be used in two ways:

As a decorator:
::

   @nvtx.annotate(message="my_message", color="blue")
   def my_func():
       pass


As a context manager:
::

   with nvtx.annotate(message="my_message", color="green"):
       pass


When used as a decorator, the ``message`` argument defaults to the
name of the function being decorated:
::

   @nvtx.annotate()  # message defaults to "my_func"
   def my_func():
       pass


mark
----

The ``mark()`` function marks an instantaneous event in the execution of a program.
For example, you may want to mark when an exceptional event occurs:
::

   try:
       something()
   except SomeError():
       nvtx.mark(message="some error occurred", color="red")
       # ... do something else ...


Domains
-------

In addition to a message and a color,
annotations can also have a `domain` associated with them.
This allows grouping related annotations together.
::

   import time
   import nvtx


   @nvtx.annotate(color="blue", domain="Domain_1")
   def func_1():
   time.sleep(1)


   @nvtx.annotate(color="green", domain="Domain_2")
   def func_2():
   time.sleep(1)


   @nvtx.annotate(color="red", domain="Domain_1")
   def func_3():
   time.sleep(1)


   func_1()
   func_2()
   func_3()


The timeline generated from the above:

.. image:: images/domains.png
    :align: center
