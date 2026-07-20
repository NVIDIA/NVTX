.. SPDX-FileCopyrightText: Copyright (c) 2020-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

Annotation API
==============

.. automodule:: nvtx
   :members:
   :imported-members:
   :exclude-members: Profile, Domain, Counter, Int64Counter, Float64Counter,
                     ExtCounter, CounterSemantics, CounterValueType,
                     CounterInterpolation, CounterNoValueReason, TimestampType, enabled

.. autoclass:: nvtx.Domain
   :members: mark, push_range, pop_range, start_range, end_range, get_event_attributes,
             set_event_attributes, get_registered_string, get_category_id, get_counter,
             get_timestamp

.. autoclass:: nvtx._lib.lib.EventAttributes

.. autoclass:: nvtx._lib.lib.RegisteredString

.. autoclass:: nvtx._lib.lib.DummyDomain

.. autoclass:: nvtx._lib.counters.DummyCounter

.. currentmodule:: nvtx

.. autoclass:: nvtx.Counter()

   .. automethod:: sample

   .. automethod:: sample_no_value

   .. automethod:: batch_submit

.. autoclass:: nvtx.Int64Counter()
   :show-inheritance:

   .. automethod:: sample

   .. automethod:: batch_submit

.. autoclass:: nvtx.Float64Counter()
   :show-inheritance:

   .. automethod:: sample

   .. automethod:: batch_submit

.. autoclass:: nvtx.ExtCounter()
   :show-inheritance:

   .. automethod:: sample

   .. automethod:: batch_submit

.. autoclass:: nvtx.CounterSemantics
   :members:

.. autoclass:: nvtx.CounterValueType
   :members:

.. autoclass:: nvtx.CounterInterpolation
   :members:

.. autoclass:: nvtx.CounterNoValueReason
   :members:

.. autoclass:: nvtx.TimestampType
   :members:

.. autoclass:: nvtx.Profile
