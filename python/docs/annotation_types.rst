.. SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

Annotation Types
=================

Markers
-------

Annotate a specific point in a program execution.

For example, mark when an exception occurs:
::

   import nvtx

   domain = nvtx.get_domain('My Lib')
   attr = domain.get_event_attributes(color='red')

   try:
       raise Exception()
   except Exception() as e:
       attr.message = str(e)
       domain.mark(attr)
       raise

Ranges
------

Annotate a range between two points in a program's execution. There are two types of ranges:


Push/Pop Ranges
~~~~~~~~~~~~~~~

- Form a stack of nested NVTX ranges per thread per NVTX domain.
- When possible, prefer to use :class:`nvtx.annotate`.
- Otherwise, for best performance, prefer to use :func:`nvtx.Domain.push_range`
  and :func:`nvtx.Domain.pop_range` over :func:`nvtx.push_range` and :func:`nvtx.pop_range`.

Start/End Ranges
~~~~~~~~~~~~~~~~

- May overlap with other ranges arbitrarily.
- Can be started and ended by different threads.
- For best performance, prefer to use :func:`nvtx.Domain.start_range`
  and :func:`nvtx.Domain.end_range` over :func:`nvtx.start_range` and :func:`nvtx.end_range`.

Counters
--------

Annotate quantities that change over time, such as memory usage, queue depth,
bytes processed, or training metrics.
For details, see :doc:`counters`.
