.. SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

Writing Pre-Collected Data
==========================

NVTX code annotations and NVTX Writer solve different problems:

* Use the regular NVTX API (``nvtx.Domain.push_range``, ``nvtx.Domain.pop_range``, etc.)
  to annotate work as it executes.
  A profiling tool observes those calls while the application is running.
* Use a writer to submit events and counter samples that already have
  timestamps. A writer backend consumes the data and decides where to store,
  merge, or display it.

Backends and Integrations
-------------------------

The :mod:`nvtx.writer` module defines the backend-independent writer API. A
backend implementation must be available before a session can write data.

For example, NVIDIA Nsight Systems provides the ``nsys_writer`` integration.
See the `Nsight Systems documentation
<https://docs.nvidia.com/nsight-systems/AnalysisGuide/index.html#writing-post-collection-events>`_
for its setup and specific usage.

Backend configuration and output behavior are integration-specific.

Session and Basic Events
------------------------

A :class:`nvtx.writer.Session` owns one collection of output data. Within a
session, a :class:`nvtx.writer.Domain` namespaces registered objects, while a
:class:`nvtx.writer.Stream` represents a logical source such as a thread,
process, rank, or device stream.

Create sessions and streams with context managers so their output is finalized
even if writing fails. For individual events, use
:meth:`nvtx.writer.Stream.write_mark`,
:meth:`~nvtx.writer.Stream.write_pushpop`, or
:meth:`~nvtx.writer.Stream.write_startend`.

::

   from nvtx.writer import Session, load_backend


   # The integration providing the backend defines its path and configuration.
   backend = load_backend("/path/to/backend/library")
   with Session("Example Writer", backend=backend) as session:
       domain = session.get_domain("My Domain")
       with session.create_stream("My Stream", domain=domain) as stream:
           stream.write_mark(100, message="My Mark")
           stream.write_pushpop(
               start=200,
               end=500,
               message="My Message",
               color="green",
           )

The following sections reuse the ``domain`` and ``stream`` names from this
setup.

Counters and Batches
--------------------

Regular NVTX counters submit annotations with :meth:`nvtx.Counter.sample`.
Writer counter samples are instead submitted to a stream with an explicit
timestamp.

Register a counter with :meth:`nvtx.writer.Domain.get_counter`,
then submit values with :meth:`nvtx.writer.Stream.write_counter_sample` or
:meth:`~nvtx.writer.Stream.write_counter_batch`. Each sample has a timestamp
in the stream's time domain; a batch pairs values and timestamps by position.

::

   counter = domain.get_counter("My Counter", int)

   # Write one sample ...
   stream.write_counter_sample(counter, timestamp=600, value=12)

   # ... or submit several samples together.
   stream.write_counter_batch(
       counter,
       [10, 8, 4],
       timestamps=[700, 800, 900],
   )

Generic Events
--------------

Generic events are useful when a mark or range needs additional data fields.
A structured NumPy dtype defines the layout: annotated fields provide the
event's timing and message, while ordinary fields carry application-specific
data. Register it with :meth:`nvtx.writer.Domain.get_schema`, then submit a row
with :meth:`nvtx.writer.Stream.write_event`.

::

   import numpy as np
   import nvtx


   event_dtype = np.dtype([
       ("start", nvtx.numpy_dtype(
           np.int64, entry_kind=nvtx.EntryKind.RANGE_BEGIN)),
       ("end", nvtx.numpy_dtype(
           np.int64, entry_kind=nvtx.EntryKind.RANGE_END)),
       ("message", nvtx.numpy_dtype(
           (np.str_, 32), entry_kind=nvtx.EntryKind.MESSAGE)),
       ("items", np.int64),
       ("input_bytes", np.int64),
   ])
   event_schema = domain.get_schema(
       event_dtype,
       kind=nvtx.EventKind.RANGE_STARTEND,
   )

   stream.write_event(
       event_schema,
       (1_000, 1_500, "process items", 64, 16_384),
   )

Use :meth:`nvtx.writer.Stream.write_event_batch` to submit multiple rows with
the same schema. See the :doc:`writer_reference` for all supported operations
and their dtype requirements.
