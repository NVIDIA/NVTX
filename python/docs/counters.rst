.. SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

.. nvtx documentation master file, created by
   sphinx-quickstart on Wed May  6 16:27:08 2020.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Counters
========

Counter annotations describe quantities that change over time, such as memory
usage, queue depth, bytes processed, or model training metrics.

This page covers counter annotations captured while the application runs. To
submit pre-collected samples with explicit timestamps, see :doc:`writer`.

Create counters with :meth:`nvtx.Domain.get_counter` and keep the counter
object around; call :meth:`nvtx.Counter.sample` from performance-sensitive code.

Scalar Counters
---------------

Use ``int`` for signed 64-bit integer samples and ``float`` for
double-precision floating point samples:
::

   import nvtx
   import torch


   domain = nvtx.get_domain("Example")
   gpu_utilization_counter = domain.get_counter(
       "gpu utilization",
       int,
       description="Percent of time kernels were executing on the GPU",
   )

   gpu_utilization_counter.sample(torch.cuda.utilization())

Counter Groups
--------------

Use a NumPy dtype to expose multiple fields as one counter sample.
Structured dtypes represent counter groups:
::

   import nvtx
   import torch


   domain = nvtx.get_domain("CUDA")
   device = torch.cuda.current_device()

   gpu_metrics_dtype = nvtx.numpy_dtype([
       ("gpu_utilization", int),
       ("memory_allocated", int),
       ("memory_reserved", int),
   ])

   gpu_metrics_counter = domain.get_counter(
       "gpu metrics",
       gpu_metrics_dtype,
       description="GPU utilization and PyTorch CUDA memory usage",
   )

   gpu_metrics_counter.sample((
       torch.cuda.utilization(device),
       torch.cuda.memory_allocated(device),
       torch.cuda.memory_reserved(device),
   ))

Counter groups are flat: fields must be scalar dtypes. Fixed-size array
fields and structured or nested fields are not supported.

Avoiding Copies
---------------

Counter samples and groups follow the general guidance in
:ref:`pass_data_native_form`: pass native Python values as-is, and prefer a
C-contiguous NumPy array matching the counter's dtype when you assemble a
batch yourself with :meth:`nvtx.Counter.batch_submit`.

Counter Semantics
-----------------

Use :class:`nvtx.CounterSemantics` to describe how counter values should be
interpreted, including units, bounds, value type, or interpolation.
For top-level scalar counters, pass :class:`nvtx.CounterSemantics` to
:meth:`nvtx.Domain.get_counter`:
::

   gpu_utilization_counter = domain.get_counter(
       "gpu utilization",
       int,
       semantics=nvtx.CounterSemantics(unit="percent", min=0, max=100),
   )

For per-field semantics in a counter group, build the field dtype with
:func:`nvtx.numpy_dtype`:
::

   percent_dtype = nvtx.numpy_dtype(
       int,
       counter_semantics=nvtx.CounterSemantics(unit="percent", min=0, max=100),
   )
   bytes_dtype = nvtx.numpy_dtype(
       int,
       counter_semantics=nvtx.CounterSemantics(unit="bytes", min=0),
   )
   gpu_metrics_dtype = nvtx.numpy_dtype([
       ("gpu_utilization", percent_dtype),
       ("memory_allocated", bytes_dtype),
       ("memory_reserved", bytes_dtype),
   ])

Batched Samples
---------------

Batched samples can reduce overhead when metrics are produced in a hot
path but do not need to be submitted immediately. Store the metric values and
timestamps with minimal work in the hot path, then use :meth:`nvtx.Counter.batch_submit`
to submit them together.
Use :meth:`nvtx.Domain.get_timestamp` to get NVTX timestamps:
::

   loss_counter = domain.get_counter("training loss", float)
   losses = []
   timestamps = []

   for inputs, targets in dataloader:
       optimizer.zero_grad(set_to_none=True)
       outputs = model(inputs)
       loss = criterion(outputs, targets)
       loss.backward()
       optimizer.step()

       losses.append(loss.detach())
       timestamps.append(domain.get_timestamp())

   loss_samples = [loss.item() for loss in losses]
   loss_counter.batch_submit(loss_samples, timestamps)

The default ``time_domain`` matches timestamps returned by
:meth:`nvtx.Domain.get_timestamp`. If timestamps come from another clock,
create the counter with the matching :class:`nvtx.TimestampType` so the
batch declares the clock domain used by its timestamps.

No-Value Samples
----------------

Use :meth:`nvtx.Counter.sample_no_value` when a sample is known to be zero,
unchanged, or unavailable, but no explicit value should be submitted:
::

   gpu_utilization_counter.sample_no_value(nvtx.CounterNoValueReason.UNAVAILABLE)
