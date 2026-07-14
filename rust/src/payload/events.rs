// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use core::marker::PhantomData;

use crate::{domain, Domain, EventAttributes, PayloadData, PayloadError, SchemaId, ScopeId};

use super::raw_payloads;

/// Ordering of deferred events or counter batches.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BatchOrder {
    /// Fully sorted by time.
    TimeSorted,
    /// Partially sorted by time.
    TimeSortedPartially,
    /// Sorted independently within each scope.
    TimeSortedPerScope,
    /// No timestamp ordering guarantee.
    Unsorted,
}

impl BatchOrder {
    const fn raw(self) -> u64 {
        match self {
            Self::TimeSorted => nvtx_sys::ffi::NVTX_BATCH_FLAG_TIME_SORTED as u64,
            Self::TimeSortedPartially => {
                nvtx_sys::ffi::NVTX_BATCH_FLAG_TIME_SORTED_PARTIALLY as u64
            }
            Self::TimeSortedPerScope => nvtx_sys::ffi::NVTX_BATCH_FLAG_TIME_SORTED_PER_SCOPE as u64,
            Self::Unsorted => nvtx_sys::ffi::NVTX_BATCH_FLAG_UNSORTED as u64,
        }
    }
}

/// Borrowed batch of deferred events.
#[derive(Debug, Clone, Copy)]
pub struct EventBatch<'a> {
    schema: SchemaId,
    events: &'a [u8],
    scope: ScopeId,
    order: BatchOrder,
    flex_data: &'a [u8],
    flex_offset: usize,
}

impl<'a> EventBatch<'a> {
    /// Create a deferred event batch.
    #[must_use]
    pub const fn new(schema: SchemaId, events: &'a [u8]) -> Self {
        Self {
            schema,
            events,
            scope: ScopeId::NONE,
            order: BatchOrder::TimeSorted,
            flex_data: &[],
            flex_offset: 0,
        }
    }

    /// Set the default event scope.
    #[must_use]
    pub const fn scope(mut self, scope: ScopeId) -> Self {
        self.scope = scope;
        self
    }

    /// Set the timestamp ordering.
    #[must_use]
    pub const fn order(mut self, order: BatchOrder) -> Self {
        self.order = order;
        self
    }

    /// Attach flexible data referenced by events in this batch.
    ///
    /// # Errors
    /// Returns [`PayloadError::InvalidFlexDataOffset`] when `offset` exceeds
    /// the supplied buffer.
    pub fn flex_data(mut self, data: &'a [u8], offset: usize) -> Result<Self, PayloadError> {
        if offset > data.len() {
            return Err(PayloadError::InvalidFlexDataOffset);
        }
        self.flex_data = data;
        self.flex_offset = offset;
        Ok(self)
    }

    fn raw(self) -> nvtx_sys::payload::EventBatch {
        nvtx_sys::payload::EventBatch {
            eventSchemaId: self.schema.get(),
            size: self.events.len(),
            events: self.events.as_ptr().cast(),
            scope: self.scope.get(),
            flags: self.order.raw(),
            flexData: self.flex_data.as_ptr().cast(),
            flexDataSize: self.flex_data.len(),
            flexDataOffset: self.flex_offset,
        }
    }
}

/// RAII thread-local range created by the dedicated payload API.
pub struct PayloadLocalRange<'a> {
    domain: &'a Domain,
    active: bool,
    not_send: PhantomData<*mut ()>,
}

impl PayloadLocalRange<'_> {
    /// Pop the range and attach end payloads.
    #[allow(clippy::must_use_candidate)]
    pub fn pop_with_payloads(mut self, payloads: &[PayloadData<'_>]) -> i32 {
        let raw = raw_payloads(payloads);
        self.active = false;
        // SAFETY: `PayloadData` ties every raw payload pointer to a live Rust borrow.
        unsafe { nvtx_sys::payload::range_pop(self.domain.raw_handle(), &raw) }
    }
}

impl Drop for PayloadLocalRange<'_> {
    fn drop(&mut self) {
        if self.active {
            // SAFETY: An empty payload slice carries no pointer validity requirement.
            let _ = unsafe { nvtx_sys::payload::range_pop(self.domain.raw_handle(), &[]) };
        }
    }
}

/// RAII process-visible range created by the dedicated payload API.
pub struct PayloadRange<'a> {
    domain: &'a Domain,
    id: Option<nvtx_sys::RangeId>,
}

impl PayloadRange<'_> {
    /// End the range and attach end payloads.
    pub fn finish_with_payloads(mut self, payloads: &[PayloadData<'_>]) {
        if let Some(id) = self.id.take() {
            let raw = raw_payloads(payloads);
            // SAFETY: `PayloadData` ties every raw payload pointer to a live Rust borrow.
            unsafe { nvtx_sys::payload::range_end(self.domain.raw_handle(), id, &raw) }
        }
    }
}

impl Drop for PayloadRange<'_> {
    fn drop(&mut self) {
        if let Some(id) = self.id.take() {
            // SAFETY: An empty payload slice carries no pointer validity requirement.
            unsafe { nvtx_sys::payload::range_end(self.domain.raw_handle(), id, &[]) }
        }
    }
}

enum AttributeTarget<'a> {
    Global,
    Domain(&'a Domain),
}

/// RAII process-visible range whose start attributes include extended payloads.
pub struct PayloadAttributeRange<'a> {
    target: AttributeTarget<'a>,
    id: Option<nvtx_sys::RangeId>,
}

impl Drop for PayloadAttributeRange<'_> {
    fn drop(&mut self) {
        if let Some(id) = self.id.take() {
            match self.target {
                AttributeTarget::Global => nvtx_sys::range_end(id),
                AttributeTarget::Domain(domain) => {
                    nvtx_sys::domain_range_end(domain.raw_handle(), id);
                }
            }
        }
    }
}

/// RAII thread-local range whose start attributes include extended payloads.
pub struct PayloadAttributeLocalRange<'a> {
    target: AttributeTarget<'a>,
    active: bool,
    not_send: PhantomData<*mut ()>,
}

impl Drop for PayloadAttributeLocalRange<'_> {
    fn drop(&mut self) {
        if self.active {
            match self.target {
                AttributeTarget::Global => {
                    let _ = nvtx_sys::range_pop();
                }
                AttributeTarget::Domain(domain) => {
                    let _ = nvtx_sys::domain_range_pop(domain.raw_handle());
                }
            }
        }
    }
}

impl Domain {
    /// Emit a dedicated extended-payload mark.
    pub fn mark_payloads(&self, payloads: &[PayloadData<'_>]) {
        let raw = raw_payloads(payloads);
        // SAFETY: `PayloadData` ties every raw payload pointer to a live Rust borrow.
        unsafe { nvtx_sys::payload::mark(self.raw_handle(), &raw) }
    }

    /// Push a dedicated extended-payload thread-local range.
    #[must_use]
    pub fn local_range_payloads(&self, payloads: &[PayloadData<'_>]) -> PayloadLocalRange<'_> {
        let raw = raw_payloads(payloads);
        // SAFETY: `PayloadData` ties every raw payload pointer to a live Rust borrow.
        let depth = unsafe { nvtx_sys::payload::range_push(self.raw_handle(), &raw) };
        PayloadLocalRange {
            domain: self,
            active: push_succeeded(depth),
            not_send: PhantomData,
        }
    }

    /// Start a dedicated extended-payload process-visible range.
    #[must_use]
    pub fn range_payloads(&self, payloads: &[PayloadData<'_>]) -> PayloadRange<'_> {
        let raw = raw_payloads(payloads);
        // SAFETY: `PayloadData` ties every raw payload pointer to a live Rust borrow.
        let id = unsafe { nvtx_sys::payload::range_start(self.raw_handle(), &raw) };
        PayloadRange {
            domain: self,
            id: Some(id),
        }
    }

    /// Submit one deferred event.
    pub fn submit_event(&self, payloads: &[PayloadData<'_>]) {
        let raw = raw_payloads(payloads);
        // SAFETY: `PayloadData` ties every raw payload pointer to a live Rust borrow.
        unsafe { nvtx_sys::payload::event_submit(self.raw_handle(), &raw) }
    }

    /// Submit a batch of deferred events.
    pub fn submit_event_batch(&self, batch: EventBatch<'_>) {
        let raw = batch.raw();
        // SAFETY: `EventBatch` ties all raw pointers to live byte slices and
        // validates the flexible-data offset.
        unsafe { nvtx_sys::payload::event_batch_submit(self.raw_handle(), &raw) }
    }

    /// Emit a regular mark whose event attributes carry extended payloads.
    ///
    /// # Errors
    /// Returns [`PayloadError::TooManyPayloads`] when the payload count does
    /// not fit the core event-attribute ABI.
    pub fn mark_with_payloads(
        &self,
        attributes: &domain::EventAttributes<'_>,
        payloads: &[PayloadData<'_>],
    ) -> Result<(), PayloadError> {
        let raw_payloads = raw_payloads(payloads);
        let encoded = encode_payload_attributes(attributes.encode(), &raw_payloads)?;
        nvtx_sys::domain_mark_ex(self.raw_handle(), &encoded);
        Ok(())
    }

    /// Start a regular process-visible range whose attributes carry payloads.
    ///
    /// # Errors
    /// Returns [`PayloadError::TooManyPayloads`] when the payload count does
    /// not fit the core event-attribute ABI.
    pub fn range_with_payloads(
        &self,
        attributes: &domain::EventAttributes<'_>,
        payloads: &[PayloadData<'_>],
    ) -> Result<PayloadAttributeRange<'_>, PayloadError> {
        let raw_payloads = raw_payloads(payloads);
        let encoded = encode_payload_attributes(attributes.encode(), &raw_payloads)?;
        let id = nvtx_sys::domain_range_start_ex(self.raw_handle(), &encoded);
        Ok(PayloadAttributeRange {
            target: AttributeTarget::Domain(self),
            id: Some(id),
        })
    }

    /// Push a regular thread-local range whose attributes carry payloads.
    ///
    /// # Errors
    /// Returns [`PayloadError::TooManyPayloads`] when the payload count does
    /// not fit the core event-attribute ABI.
    pub fn local_range_with_payloads(
        &self,
        attributes: &domain::EventAttributes<'_>,
        payloads: &[PayloadData<'_>],
    ) -> Result<PayloadAttributeLocalRange<'_>, PayloadError> {
        let raw_payloads = raw_payloads(payloads);
        let encoded = encode_payload_attributes(attributes.encode(), &raw_payloads)?;
        let depth = nvtx_sys::domain_range_push_ex(self.raw_handle(), &encoded);
        Ok(PayloadAttributeLocalRange {
            target: AttributeTarget::Domain(self),
            active: push_succeeded(depth),
            not_send: PhantomData,
        })
    }
}

/// Emit a global mark whose event attributes carry extended payloads.
///
/// # Errors
/// Returns [`PayloadError::TooManyPayloads`] when the payload count does not
/// fit the core event-attribute ABI.
pub fn mark_with_payloads(
    attributes: &EventAttributes,
    payloads: &[PayloadData<'_>],
) -> Result<(), PayloadError> {
    let raw_payloads = raw_payloads(payloads);
    let encoded = encode_payload_attributes(attributes.encode(), &raw_payloads)?;
    nvtx_sys::mark_ex(&encoded);
    Ok(())
}

/// Start a global process-visible range whose attributes carry payloads.
///
/// # Errors
/// Returns [`PayloadError::TooManyPayloads`] when the payload count does not
/// fit the core event-attribute ABI.
pub fn range_with_payloads(
    attributes: &EventAttributes,
    payloads: &[PayloadData<'_>],
) -> Result<PayloadAttributeRange<'static>, PayloadError> {
    let raw_payloads = raw_payloads(payloads);
    let encoded = encode_payload_attributes(attributes.encode(), &raw_payloads)?;
    let id = nvtx_sys::range_start_ex(&encoded);
    Ok(PayloadAttributeRange {
        target: AttributeTarget::Global,
        id: Some(id),
    })
}

/// Push a global thread-local range whose attributes carry payloads.
///
/// # Errors
/// Returns [`PayloadError::TooManyPayloads`] when the payload count does not
/// fit the core event-attribute ABI.
pub fn local_range_with_payloads(
    attributes: &EventAttributes,
    payloads: &[PayloadData<'_>],
) -> Result<PayloadAttributeLocalRange<'static>, PayloadError> {
    let raw_payloads = raw_payloads(payloads);
    let encoded = encode_payload_attributes(attributes.encode(), &raw_payloads)?;
    let depth = nvtx_sys::range_push_ex(&encoded);
    Ok(PayloadAttributeLocalRange {
        target: AttributeTarget::Global,
        active: push_succeeded(depth),
        not_send: PhantomData,
    })
}

fn encode_payload_attributes(
    mut attributes: nvtx_sys::EventAttributes,
    payloads: &[nvtx_sys::payload::PayloadData],
) -> Result<nvtx_sys::EventAttributes, PayloadError> {
    let count = i32::try_from(payloads.len()).map_err(|_| PayloadError::TooManyPayloads)?;
    attributes.payloadType = nvtx_sys::payload::PAYLOAD_TYPE_EXT;
    attributes.reserved0 = count;
    attributes.payload.ullValue = payloads.as_ptr() as usize as u64;
    Ok(attributes)
}

const fn push_succeeded(depth: i32) -> bool {
    depth >= 0
}

#[cfg(test)]
mod tests {
    use super::push_succeeded;

    #[test]
    fn local_range_is_armed_only_after_successful_push() {
        assert!(push_succeeded(0));
        assert!(push_succeeded(1));
        assert!(!push_succeeded(-1));
    }
}
