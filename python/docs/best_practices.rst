.. SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

Best practices
==============

Give, don't take
----------------

NVTX is primarily a one-way API. Your program gives information to the tool,
but it does not get actionable information back from the tool.
Some NVTX functions return values, but these should only be used as inputs to other NVTX functions.
Programs should behave exactly the same regardless of whether a tool is present or not.

Isolate NVTX annotations in a library using a domain
----------------------------------------------------

Programs may use multiple libraries that produce NVTX annotations.
A library should isolate its annotations by creating them within a dedicated domain.
Tools can group annotation data by library,
and provide options for which domains to enable or disable during the program execution.

Use categories to organize annotations
--------------------------------------

While domains are intended to separate the annotations from different libraries,
it may be useful to have separate categories for annotations within a library.
Tools are encouraged to logically group annotations into categories.
Using slashes in category names like filesystem paths allows the user to
create a hierarchy of categories, and tools should handle these as a hierarchy.

Reduce cache lookups and object allocations
-------------------------------------------

NVTX is designed to produce minimal overhead during the program execution.
As such, it caches :class:`nvtx.Domain` objects, registered strings, category IDs,
and :class:`nvtx.Counter` objects.

Although the functions :func:`nvtx.mark`, :func:`nvtx.push_range`, :func:`nvtx.pop_range`,
:func:`nvtx.start_range`, and :func:`nvtx.end_range` are convenient to use,
they include event attributes object allocation and cache lookups for the domain on each call.
Therefore, for best performance, it's better to use the methods from :class:`nvtx.Domain` instead.
For example:
::

    import nvtx

    def my_func(param: int):
        # This call includes allocation of the event attributes object,
        # and a cache lookup for the domain, the message registered string and the category ID.
        # See `my_func_fast` for a faster alternative.
        nvtx.mark(message='my_func', domain='My Lib', category='my_category', payload=param)

        # continue with the function logic

   # Save a reference to the domain object,
   # so it can be accessed everywhere in the library code,
   # to avoid multiple calls to `nvtx.get_domain()`
   domain = nvtx.get_domain('My Lib')

   # Reuse category IDs and EventAttributes objects when possible
   # to avoid multiple calls to nvtx.get_category_id() and nvtx.get_event_attributes()
   category_id = domain.get_category_id('my_category')
   attr = domain.get_event_attributes(message='my_func')

   def my_func_fast(param: int):
       """Faster version of my_func() using the domain object directly."""
       attr.payload = param
       domain.mark(attr)    # No cache lookups

       # continue with the function logic

Use payload for large data, don't embed data in messages
--------------------------------------------------------

Embedding data in messages may lead to increased memory usage at measurement time,
because messages are cached and registered.
Furthermore, using payloads provides a separation between the message and the data of the event,
which is often useful for analysis.
In the Nsight Systems GUI, payloads are displayed in the description and the tooltip of the event.

Use counter annotations for values that change over time
--------------------------------------------------------

Range and marker annotations identify when code is running or when an event
happened. Counter annotations describe quantities that change over time.
For example, prefer a counter for memory usage, queue depth, bytes processed,
or loss values that should be plotted over time.
Use :meth:`nvtx.Counter.batch_submit` to reduce overhead when values are
produced in a hot path but do not need to be submitted immediately.

.. _pass_data_native_form:

Pass data in its native form
----------------------------

NVTX does no work when no tool is attached: :func:`nvtx.get_domain` returns a
disabled domain whose annotation methods are no-ops, and
:meth:`nvtx.Domain.get_counter` returns a disabled counter. Avoid preparing or
converting annotation data ahead of a call: that preparation runs whether or not
a tool is attached, so it adds overhead with no benefit when none is present.
Pass your values in their native form and let NVTX convert them only when a tool
consumes them.

When a tool is attached, NVTX converts arrays and dtype-based data (counter
samples, counter groups, and binary payloads attached to ranges and markers)
with :func:`numpy.ascontiguousarray` before reading its bytes. A C-contiguous
NumPy array that already matches the target dtype is forwarded without a copy,
while native Python data (a tuple or list) is copied into a fresh array.
Choose the input form accordingly:

* If you assemble a large or batched buffer yourself, build it as a
  C-contiguous NumPy array of the target dtype to skip the copy. The saving
  grows with the amount of data.
* If the data is a single value or already exists as native Python, pass it
  as-is. Converting one value to a NumPy array only adds an allocation.
