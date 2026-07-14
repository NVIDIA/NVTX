// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use crate::{ffi, DomainHandle};

pub type CounterAttributes = ffi::nvtxCounterAttr_t;
pub type CounterBatch = ffi::nvtxCounterBatch_t;

/// First user-defined static counter ID.
pub const COUNTER_ID_STATIC_START: u64 = 1_u64 << 24;
/// First tool-generated dynamic counter ID.
pub const COUNTER_ID_DYNAMIC_START: u64 = 1_u64 << 32;
/// Batch timestamps are begin-time/interval pairs.
pub const COUNTER_BATCH_FLAG_BEGIN_TIME_INTERVAL_PAIR: u64 = 1_u64 << 32;
/// Batch timestamps are end-time/interval pairs.
pub const COUNTER_BATCH_FLAG_END_TIME_INTERVAL_PAIR: u64 = 2_u64 << 32;

/// Register a counter or counter group.
///
/// # Safety
/// Every pointer reachable from `attributes` must be valid for the duration of
/// the call and refer to data compatible with its schema and semantics.
#[must_use]
pub unsafe fn register(domain: DomainHandle, attributes: &CounterAttributes) -> u64 {
    // SAFETY: The caller guarantees the descriptor graph is valid.
    unsafe { ffi::nvtxCounterRegister(domain.handle, attributes) }
}

/// Sample an `i64` counter.
pub fn sample_i64(domain: DomainHandle, counter_id: u64, value: i64) {
    // SAFETY: All arguments are scalar values.
    unsafe { ffi::nvtxCounterSampleInt64(domain.handle, counter_id, value) }
}

/// Sample an `f64` counter.
pub fn sample_f64(domain: DomainHandle, counter_id: u64, value: f64) {
    // SAFETY: All arguments are scalar values.
    unsafe { ffi::nvtxCounterSampleFloat64(domain.handle, counter_id, value) }
}

/// Sample a counter or counter group from its encoded bytes.
pub fn sample_bytes(domain: DomainHandle, counter_id: u64, value: &[u8]) {
    // SAFETY: `value` is a valid contiguous byte slice for the call duration.
    unsafe {
        ffi::nvtxCounterSample(
            domain.handle,
            counter_id,
            value.as_ptr().cast(),
            value.len(),
        );
    }
}

/// Sample a counter without a value.
pub fn sample_no_value(domain: DomainHandle, counter_id: u64, reason: u8) {
    // SAFETY: All arguments are scalar values.
    unsafe { ffi::nvtxCounterSampleNoValue(domain.handle, counter_id, reason) }
}

/// Submit a counter batch.
///
/// # Safety
/// Every pointer reachable from `batch` must be valid for the duration of the
/// call and its data must match the registered counter schema.
pub unsafe fn batch_submit(domain: DomainHandle, batch: &CounterBatch) {
    // SAFETY: The caller guarantees the batch descriptor graph is valid.
    unsafe { ffi::nvtxCounterBatchSubmit(domain.handle, batch) }
}
