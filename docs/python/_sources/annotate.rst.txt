Function Decorator and Context Manager
======================================

:class:`nvtx.annotate` can be used in two ways:

As a function decorator, for generating a range every time the function is called:
::

   @nvtx.annotate(message="my_message", color="blue")
   def my_func():
       pass

The ``message`` argument defaults to the name of the function being decorated:
::

   @nvtx.annotate()  # message defaults to "my_func"
   def my_func():
       pass

As a context manager, for generating a range of a code block:
::

   with nvtx.annotate(message="my_message", color="green"):
       pass
