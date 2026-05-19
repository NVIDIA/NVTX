.. SPDX-FileCopyrightText: Copyright (c) 2020-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

API Reference
=============

.. automodule:: nvtx
   :members:
   :imported-members:
   :exclude-members: Profile, Domain, enabled

.. autoclass:: nvtx.Domain
   :members: mark, push_range, pop_range, start_range, end_range, get_event_attributes,
             set_event_attributes, get_registered_string, get_category_id

.. autoclass:: nvtx._lib.lib.EventAttributes

.. autoclass:: nvtx._lib.lib.RegisteredString

.. autoclass:: nvtx._lib.lib.DummyDomain

.. autoclass:: nvtx.Profile
