.. SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

Writer API
==========

.. automodule:: nvtx.writer

Backend
-------

.. autofunction:: nvtx.writer.load_backend

.. autoclass:: nvtx.writer.Backend()
   :members: close

.. autoexception:: nvtx.writer.WriterError

Sessions and Streams
--------------------

.. autoclass:: nvtx.writer.Session
   :members: begin, end, get_domain, create_stream

.. autoclass:: nvtx.writer.Domain()
   :members: get_registered_string, get_category_id, get_scope, get_schema, get_counter

.. autoclass:: nvtx.writer.Stream()
   :members: open, close, write_mark, write_pushpop, write_startend, write_push,
             write_pop, write_start, write_end, write_event, write_event_batch,
             write_counter_sample, write_counter_sample_no_value,
             write_counter_batch

Registered Objects
------------------

.. autoclass:: nvtx.writer.RegisteredString()

.. autoclass:: nvtx.writer.Scope()
   :members: scope_id, path

.. autoclass:: nvtx.writer.Schema()
   :members: schema_id, dtype, kind

.. autoclass:: nvtx.writer.Counter()
   :members: counter_id, name, dtype, description, semantics

Stream Options
--------------

.. autoclass:: nvtx.writer.StreamInterleaving
   :members:

.. autoclass:: nvtx.writer.StreamOrdering
   :members:

.. autoclass:: nvtx.writer.StreamSkid
   :members:

.. py:class:: PredefinedScope

   Alias of :class:`nvtx.PredefinedScope`.
