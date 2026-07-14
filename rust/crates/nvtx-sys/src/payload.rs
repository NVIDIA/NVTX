// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use core::ffi::c_void;

use crate::{ffi, DomainHandle, RangeId};

pub type PayloadEntryTypeInfo = ffi::nvtxPayloadEntryTypeInfo_t;
pub type PayloadData = ffi::nvtxPayloadData_t;
pub type SemanticsHeader = ffi::nvtxSemanticsHeader_t;
pub type PayloadSchemaEntry = ffi::nvtxPayloadSchemaEntry_t;
pub type PayloadSchemaAttributes = ffi::nvtxPayloadSchemaAttr_t;
pub type PayloadEnumEntry = ffi::nvtxPayloadEnum_t;
pub type PayloadEnumAttributes = ffi::nvtxPayloadEnumAttr_t;
pub type ScopeAttributes = ffi::nvtxScopeAttr_t;
pub type TimeDomainAttributes = ffi::nvtxTimeDomainAttr_t;
pub type SyncPoint = ffi::nvtxSyncPoint_t;
pub type EventBatch = ffi::nvtxEventBatch_t;
pub type CorrelationSemantics = ffi::nvtxSemanticsCorrelation_t;
pub type CounterSemantics = ffi::nvtxSemanticsCounter_t;
pub type ScopeSemantics = ffi::nvtxSemanticsScope_t;
pub type TimeSemantics = ffi::nvtxSemanticsTime_t;

pub type TimestampProvider = unsafe extern "C" fn() -> i64;
pub type TimestampProviderWithData = unsafe extern "C" fn(*mut c_void) -> i64;

/// Core event-attribute discriminator for extended payload data.
pub const PAYLOAD_TYPE_EXT: i32 = 0xDFBD_0009_u32 as i32;
/// First user-defined static payload schema or enumeration ID.
pub const PAYLOAD_SCHEMA_ID_STATIC_START: u64 = 1_u64 << 24;
/// First tool-generated dynamic payload schema or enumeration ID.
pub const PAYLOAD_SCHEMA_ID_DYNAMIC_START: u64 = 1_u64 << 32;
/// First user-defined static scope ID.
pub const SCOPE_ID_STATIC_START: u64 = 1_u64 << 24;
/// First tool-generated dynamic scope ID.
pub const SCOPE_ID_DYNAMIC_START: u64 = 1_u64 << 32;
/// First user-defined static time-domain ID.
pub const TIME_DOMAIN_ID_STATIC_START: u64 = 1_u64 << 24;
/// First tool-generated dynamic time-domain ID.
pub const TIME_DOMAIN_ID_DYNAMIC_START: u64 = 1_u64 << 32;

/// Register an extended-payload schema.
///
/// # Safety
/// Every pointer reachable from `attributes` must be valid for the duration of
/// the call and describe a layout accepted by NVTX.
#[must_use]
pub unsafe fn schema_register(domain: DomainHandle, attributes: &PayloadSchemaAttributes) -> u64 {
    // SAFETY: The caller guarantees the complete descriptor graph is valid.
    unsafe { ffi::nvtxPayloadSchemaRegister(domain.handle, attributes) }
}

/// Register an extended-payload enumeration.
///
/// # Safety
/// Every pointer reachable from `attributes` must be valid for the duration of
/// the call.
#[must_use]
pub unsafe fn enum_register(domain: DomainHandle, attributes: &PayloadEnumAttributes) -> u64 {
    // SAFETY: The caller guarantees the complete descriptor graph is valid.
    unsafe { ffi::nvtxPayloadEnumRegister(domain.handle, attributes) }
}

/// Register an NVTX scope.
///
/// # Safety
/// The path pointer in `attributes`, when non-null, must be a valid
/// NUL-terminated C string for the duration of the call.
#[must_use]
pub unsafe fn scope_register(domain: DomainHandle, attributes: &ScopeAttributes) -> u64 {
    // SAFETY: The caller guarantees pointer validity.
    unsafe { ffi::nvtxScopeRegister(domain.handle, attributes) }
}

/// Emit a mark carrying extended payloads.
///
/// # Safety
/// Every payload data pointer must remain valid for the duration of the call
/// and match its declared schema and size.
pub unsafe fn mark(domain: DomainHandle, payloads: &[PayloadData]) {
    // SAFETY: The caller guarantees each payload descriptor is valid.
    unsafe { ffi::nvtxMarkPayload(domain.handle, payload_pointer(payloads), payloads.len()) }
}

/// Push a thread-local range carrying extended payloads.
///
/// # Safety
/// Every payload data pointer must remain valid for the duration of the call
/// and match its declared schema and size.
#[allow(clippy::must_use_candidate)]
pub unsafe fn range_push(domain: DomainHandle, payloads: &[PayloadData]) -> i32 {
    // SAFETY: The caller guarantees each payload descriptor is valid.
    unsafe { ffi::nvtxRangePushPayload(domain.handle, payload_pointer(payloads), payloads.len()) }
}

/// Pop a thread-local range carrying extended payloads.
///
/// # Safety
/// Every payload data pointer must remain valid for the duration of the call
/// and match its declared schema and size.
#[allow(clippy::must_use_candidate)]
pub unsafe fn range_pop(domain: DomainHandle, payloads: &[PayloadData]) -> i32 {
    // SAFETY: The caller guarantees each payload descriptor is valid.
    unsafe { ffi::nvtxRangePopPayload(domain.handle, payload_pointer(payloads), payloads.len()) }
}

/// Start a process-visible range carrying extended payloads.
///
/// # Safety
/// Every payload data pointer must remain valid for the duration of the call
/// and match its declared schema and size.
#[must_use]
pub unsafe fn range_start(domain: DomainHandle, payloads: &[PayloadData]) -> RangeId {
    // SAFETY: The caller guarantees each payload descriptor is valid.
    unsafe { ffi::nvtxRangeStartPayload(domain.handle, payload_pointer(payloads), payloads.len()) }
}

/// End a process-visible range carrying extended payloads.
///
/// # Safety
/// Every payload data pointer must remain valid for the duration of the call
/// and match its declared schema and size. `id` must belong to `domain`.
pub unsafe fn range_end(domain: DomainHandle, id: RangeId, payloads: &[PayloadData]) {
    // SAFETY: The caller guarantees each payload descriptor and range ID are valid.
    unsafe {
        ffi::nvtxRangeEndPayload(domain.handle, id, payload_pointer(payloads), payloads.len());
    }
}

/// Return whether a domain is enabled by the active tool.
#[must_use]
pub fn domain_is_enabled(domain: DomainHandle) -> bool {
    // SAFETY: `domain` is an opaque handle created by NVTX.
    unsafe { ffi::nvtxDomainIsEnabled(domain.handle) != 0 }
}

/// Read a timestamp from the active NVTX handler.
#[must_use]
pub fn timestamp_get() -> i64 {
    // SAFETY: This function has no arguments and no memory safety preconditions.
    unsafe { ffi::nvtxTimestampGet() }
}

/// Register a time domain.
#[must_use]
pub fn time_domain_register(domain: DomainHandle, attributes: &TimeDomainAttributes) -> u64 {
    // SAFETY: `attributes` contains only scalar values.
    unsafe { ffi::nvtxTimeDomainRegister(domain.handle, attributes) }
}

/// Register a callback that provides timestamps.
///
/// # Safety
/// `provider` must remain callable for as long as the NVTX tool may invoke it,
/// must obey the C ABI, and must not unwind.
pub unsafe fn timer_source(
    domain: DomainHandle,
    time_domain_id: u64,
    flags: u64,
    provider: Option<TimestampProvider>,
) {
    // SAFETY: The caller guarantees callback validity.
    unsafe { ffi::nvtxTimerSource(domain.handle, time_domain_id, flags, provider) }
}

/// Register a callback that provides timestamps and receives user data.
///
/// # Safety
/// `provider` and `data` must remain valid for as long as the NVTX tool may
/// invoke the callback. The callback must obey the C ABI and must not unwind.
pub unsafe fn timer_source_with_data(
    domain: DomainHandle,
    time_domain_id: u64,
    flags: u64,
    provider: Option<TimestampProviderWithData>,
    data: *mut c_void,
) {
    // SAFETY: The caller guarantees callback and data validity.
    unsafe {
        ffi::nvtxTimerSourceWithData(domain.handle, time_domain_id, flags, provider, data);
    }
}

/// Submit one synchronization point between two time domains.
pub fn time_sync_point(
    domain: DomainHandle,
    source_id: u64,
    destination_id: u64,
    source_timestamp: i64,
    destination_timestamp: i64,
) {
    // SAFETY: All arguments are scalar values.
    unsafe {
        ffi::nvtxTimeSyncPoint(
            domain.handle,
            source_id,
            destination_id,
            source_timestamp,
            destination_timestamp,
        );
    }
}

/// Submit synchronization points between two time domains.
pub fn time_sync_point_table(
    domain: DomainHandle,
    source_id: u64,
    destination_id: u64,
    points: &[SyncPoint],
) {
    // SAFETY: `points` is a valid contiguous slice for the call duration.
    unsafe {
        ffi::nvtxTimeSyncPointTable(
            domain.handle,
            source_id,
            destination_id,
            points.as_ptr(),
            points.len(),
        );
    }
}

/// Submit a conversion factor between two time domains.
pub fn timestamp_conversion_factor(
    domain: DomainHandle,
    source_id: u64,
    destination_id: u64,
    slope: f64,
    source_timestamp: i64,
    destination_timestamp: i64,
) {
    // SAFETY: All arguments are scalar values.
    unsafe {
        ffi::nvtxTimestampConversionFactor(
            domain.handle,
            source_id,
            destination_id,
            slope,
            source_timestamp,
            destination_timestamp,
        );
    }
}

/// Submit one deferred event.
///
/// # Safety
/// Every payload data pointer must remain valid for the duration of the call
/// and match its declared schema and size.
pub unsafe fn event_submit(domain: DomainHandle, payloads: &[PayloadData]) {
    // SAFETY: The caller guarantees every payload descriptor is valid.
    unsafe { ffi::nvtxEventSubmit(domain.handle, payload_pointer(payloads), payloads.len()) }
}

/// Submit a batch of deferred events.
///
/// # Safety
/// Every pointer reachable from `batch` must be valid for the duration of the
/// call and match the registered event schema.
pub unsafe fn event_batch_submit(domain: DomainHandle, batch: &EventBatch) {
    // SAFETY: The caller guarantees the batch descriptor graph is valid.
    unsafe { ffi::nvtxEventBatchSubmit(domain.handle, batch) }
}

fn payload_pointer(payloads: &[PayloadData]) -> *const PayloadData {
    if payloads.is_empty() {
        core::ptr::null()
    } else {
        payloads.as_ptr()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn empty_payload_slice_uses_null_pointer() {
        assert!(payload_pointer(&[]).is_null());
    }
}
