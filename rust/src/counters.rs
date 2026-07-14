// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use alloc::{borrow::ToOwned, ffi::CString};
use core::{
    ffi::CStr,
    marker::PhantomData,
    mem::{size_of, size_of_val},
};

use crate::{
    CounterSemantic, Domain, PayloadEntryType, PayloadError, PayloadSchemaType, SchemaId, ScopeId,
    SemanticChain, TimeDomainId, TimeSemantic,
};

/// Error returned by counter construction or batch submission.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub enum CounterError {
    /// Registering the counter's payload schema failed validation.
    Payload(PayloadError),
    /// A statically assigned counter ID was outside the NVTX static-ID range.
    InvalidStaticId,
    /// The number of timestamps did not match the number of samples.
    TimestampCountMismatch,
}

impl core::fmt::Display for CounterError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::Payload(error) => write!(f, "counter payload schema error: {error}"),
            Self::InvalidStaticId => write!(f, "counter ID is outside the NVTX static-ID range"),
            Self::TimestampCountMismatch => {
                write!(f, "counter sample and timestamp counts differ")
            }
        }
    }
}

impl From<PayloadError> for CounterError {
    fn from(value: PayloadError) -> Self {
        Self::Payload(value)
    }
}

#[cfg(feature = "std")]
impl std::error::Error for CounterError {}

/// Identifier of a registered counter or counter group.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct CounterId(u64);

impl CounterId {
    /// No counter.
    pub const NONE: Self = Self(0);

    /// Construct an ID from its raw value.
    #[must_use]
    pub const fn from_raw(raw: u64) -> Self {
        Self(raw)
    }

    /// Return the raw NVTX counter ID.
    #[must_use]
    pub const fn get(self) -> u64 {
        self.0
    }
}

/// Reason for recording a counter sample without a value.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NoValueReason {
    /// The value is known to be zero.
    Zero,
    /// The value is unchanged from the previous sample.
    Unchanged,
    /// A value could not be obtained.
    Unavailable,
}

impl NoValueReason {
    const fn raw(self) -> u8 {
        match self {
            Self::Zero => nvtx_sys::ffi::NVTX_COUNTER_SAMPLE_ZERO as u8,
            Self::Unchanged => nvtx_sys::ffi::NVTX_COUNTER_SAMPLE_UNCHANGED as u8,
            Self::Unavailable => nvtx_sys::ffi::NVTX_COUNTER_SAMPLE_UNAVAILABLE as u8,
        }
    }
}

/// Pairing convention for timestamps in a counter batch.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CounterInterval {
    /// Each pair is a begin time followed by an interval.
    BeginTimeAndInterval,
    /// Each pair is an end time followed by an interval.
    EndTimeAndInterval,
}

/// Counter batch submission options.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CounterBatchOptions {
    order: crate::BatchOrder,
    interval: Option<CounterInterval>,
}

impl Default for CounterBatchOptions {
    fn default() -> Self {
        Self::new()
    }
}

impl CounterBatchOptions {
    /// Start with time-sorted samples and one timestamp per sample.
    #[must_use]
    pub const fn new() -> Self {
        Self {
            order: crate::BatchOrder::TimeSorted,
            interval: None,
        }
    }

    /// Set timestamp ordering.
    #[must_use]
    pub const fn order(mut self, order: crate::BatchOrder) -> Self {
        self.order = order;
        self
    }

    /// Use timestamp/interval pairs.
    #[must_use]
    pub const fn interval(mut self, interval: CounterInterval) -> Self {
        self.interval = Some(interval);
        self
    }

    fn raw(self) -> u64 {
        let order = match self.order {
            crate::BatchOrder::TimeSorted => nvtx_sys::ffi::NVTX_BATCH_FLAG_TIME_SORTED as u64,
            crate::BatchOrder::TimeSortedPartially => {
                nvtx_sys::ffi::NVTX_BATCH_FLAG_TIME_SORTED_PARTIALLY as u64
            }
            crate::BatchOrder::TimeSortedPerScope => {
                nvtx_sys::ffi::NVTX_BATCH_FLAG_TIME_SORTED_PER_SCOPE as u64
            }
            crate::BatchOrder::Unsorted => nvtx_sys::ffi::NVTX_BATCH_FLAG_UNSORTED as u64,
        };
        order
            | match self.interval {
                None => 0,
                Some(CounterInterval::BeginTimeAndInterval) => {
                    nvtx_sys::counters::COUNTER_BATCH_FLAG_BEGIN_TIME_INTERVAL_PAIR
                }
                Some(CounterInterval::EndTimeAndInterval) => {
                    nvtx_sys::counters::COUNTER_BATCH_FLAG_END_TIME_INTERVAL_PAIR
                }
            }
    }
}

/// A value type that can be sampled by an NVTX counter.
///
/// # Safety
/// Implementors used with batch submission must have a fully initialized,
/// stable byte representation matching the schema returned by [`Self::schema_id`].
pub unsafe trait CounterValue: Sized + 'static {
    /// Return the predefined or registered schema ID for this type.
    ///
    /// # Errors
    /// Returns a counter or payload-schema error when the type cannot be
    /// registered in `domain`.
    fn schema_id(domain: &Domain) -> Result<SchemaId, CounterError>;

    /// Submit one value.
    fn sample(domain: &Domain, counter_id: CounterId, value: &Self);
}

macro_rules! byte_counter_value {
    ($type:ty, $entry_type:expr) => {
        // SAFETY: Primitive numeric types have stable, fully initialized byte
        // representations matching their predefined NVTX schema IDs.
        unsafe impl CounterValue for $type {
            fn schema_id(_domain: &Domain) -> Result<SchemaId, CounterError> {
                Ok(SchemaId::from_raw($entry_type.get()))
            }

            fn sample(domain: &Domain, counter_id: CounterId, value: &Self) {
                nvtx_sys::counters::sample_bytes(
                    domain.raw_handle(),
                    counter_id.get(),
                    bytes_of(value),
                );
            }
        }
    };
}

// SAFETY: `i64` has a stable, fully initialized representation matching INT64.
unsafe impl CounterValue for i64 {
    fn schema_id(_domain: &Domain) -> Result<SchemaId, CounterError> {
        Ok(SchemaId::from_raw(PayloadEntryType::INT64.get()))
    }

    fn sample(domain: &Domain, counter_id: CounterId, value: &Self) {
        nvtx_sys::counters::sample_i64(domain.raw_handle(), counter_id.get(), *value);
    }
}

// SAFETY: `f64` has a stable, fully initialized representation matching FLOAT64.
unsafe impl CounterValue for f64 {
    fn schema_id(_domain: &Domain) -> Result<SchemaId, CounterError> {
        Ok(SchemaId::from_raw(PayloadEntryType::FLOAT64.get()))
    }

    fn sample(domain: &Domain, counter_id: CounterId, value: &Self) {
        nvtx_sys::counters::sample_f64(domain.raw_handle(), counter_id.get(), *value);
    }
}

byte_counter_value!(u64, PayloadEntryType::UINT64);
byte_counter_value!(i32, PayloadEntryType::INT32);
byte_counter_value!(u32, PayloadEntryType::UINT32);
byte_counter_value!(f32, PayloadEntryType::FLOAT);

/// Builder for a domain-bound typed counter.
pub struct CounterBuilder<'a, T> {
    domain: &'a Domain,
    name: CString,
    description: Option<CString>,
    schema_id: SchemaId,
    scope: ScopeId,
    semantics: Option<CounterSemantic>,
    time_domain: Option<TimeDomainId>,
    static_id: CounterId,
    sample: fn(&Domain, CounterId, &T),
}

impl<'a, T> CounterBuilder<'a, T> {
    /// Set a longer counter description.
    #[must_use]
    pub fn description(mut self, description: &CStr) -> Self {
        self.description = Some(description.to_owned());
        self
    }

    /// Set the counter scope.
    #[must_use]
    pub const fn scope(mut self, scope: ScopeId) -> Self {
        self.scope = scope;
        self
    }

    /// Set whole-counter semantic metadata.
    #[must_use]
    pub fn semantics(mut self, semantics: CounterSemantic) -> Self {
        self.semantics = Some(semantics);
        self
    }

    /// Associate external batch timestamps with a time domain.
    #[must_use]
    pub const fn time_domain(mut self, time_domain: TimeDomainId) -> Self {
        self.time_domain = Some(time_domain);
        self
    }

    /// Request a static counter ID.
    ///
    /// # Errors
    /// Returns [`CounterError::InvalidStaticId`] when `id` is outside the
    /// user-defined static-ID range.
    pub fn static_id(mut self, id: CounterId) -> Result<Self, CounterError> {
        validate_static_id(id.get())?;
        self.static_id = id;
        Ok(self)
    }

    /// Register and return the counter.
    #[must_use]
    pub fn register(self) -> Counter<'a, T> {
        let mut semantic_chain = SemanticChain::new();
        if let Some(time_domain) = self.time_domain {
            semantic_chain = semantic_chain.semantic(TimeSemantic::new(time_domain));
        }
        if let Some(semantics) = self.semantics {
            semantic_chain = semantic_chain.semantic(semantics);
        }
        let encoded_semantics = semantic_chain.encode();
        let attributes = nvtx_sys::counters::CounterAttributes {
            structSize: size_of::<nvtx_sys::counters::CounterAttributes>(),
            schemaId: self.schema_id.get(),
            name: self.name.as_ptr(),
            description: self
                .description
                .as_ref()
                .map_or(core::ptr::null(), |description| description.as_ptr()),
            scopeId: self.scope.get(),
            semantics: encoded_semantics.head(),
            counterId: self.static_id.get(),
        };
        // SAFETY: All strings and the encoded semantics chain remain alive for
        // the complete registration call.
        let id = unsafe { nvtx_sys::counters::register(self.domain.raw_handle(), &attributes) };
        Counter {
            domain: self.domain,
            id: CounterId::from_raw(id),
            sample: self.sample,
            marker: PhantomData,
        }
    }
}

/// Typed, domain-bound NVTX counter.
pub struct Counter<'a, T> {
    domain: &'a Domain,
    id: CounterId,
    sample: fn(&Domain, CounterId, &T),
    marker: PhantomData<fn(T)>,
}

impl<T> Counter<'_, T> {
    /// Return the registered counter ID.
    #[must_use]
    pub const fn id(&self) -> CounterId {
        self.id
    }

    /// Submit one sample.
    pub fn sample(&self, value: &T) {
        (self.sample)(self.domain, self.id, value);
    }

    /// Submit a sample without a value.
    pub fn sample_no_value(&self, reason: NoValueReason) {
        nvtx_sys::counters::sample_no_value(self.domain.raw_handle(), self.id.get(), reason.raw());
    }

    /// Submit a contiguous batch of samples.
    ///
    /// # Errors
    /// Returns [`CounterError::TimestampCountMismatch`] when the timestamp
    /// slice does not match the sample count and interval convention.
    pub fn submit_batch(
        &self,
        values: &[T],
        timestamps: Option<&[i64]>,
        options: CounterBatchOptions,
    ) -> Result<(), CounterError> {
        if let Some(timestamps) = timestamps {
            let expected = if options.interval.is_some() {
                values.len().saturating_mul(2)
            } else {
                values.len()
            };
            if timestamps.len() != expected {
                return Err(CounterError::TimestampCountMismatch);
            }
        }
        let timestamps = timestamps.unwrap_or(&[]);
        let batch = nvtx_sys::counters::CounterBatch {
            counterId: self.id.get(),
            counters: values.as_ptr().cast(),
            countersSize: size_of_val(values),
            flags: options.raw(),
            timestamps: timestamps.as_ptr(),
            timestampsSize: size_of_val(timestamps),
        };
        // SAFETY: Both slices are alive for the call and `Counter<T>` was
        // registered with the same representation used by `values`.
        unsafe { nvtx_sys::counters::batch_submit(self.domain.raw_handle(), &batch) }
        Ok(())
    }
}

impl Domain {
    /// Begin building a primitive typed counter.
    ///
    /// # Errors
    /// Returns the error produced while resolving `T`'s counter schema.
    pub fn counter<T: CounterValue>(
        &self,
        name: &CStr,
    ) -> Result<CounterBuilder<'_, T>, CounterError> {
        Ok(CounterBuilder {
            domain: self,
            name: name.to_owned(),
            description: None,
            schema_id: T::schema_id(self)?,
            scope: ScopeId::NONE,
            semantics: None,
            time_domain: None,
            static_id: CounterId::NONE,
            sample: T::sample,
        })
    }

    /// Begin building a counter for a registered structured payload type.
    ///
    /// # Errors
    /// Returns the error produced while constructing or registering `T`'s
    /// payload schema.
    pub fn payload_counter<T: PayloadSchemaType>(
        &self,
        name: &CStr,
    ) -> Result<CounterBuilder<'_, T>, CounterError> {
        Ok(CounterBuilder {
            domain: self,
            name: name.to_owned(),
            description: None,
            schema_id: self.counter_payload_schema::<T>()?,
            scope: ScopeId::NONE,
            semantics: None,
            time_domain: None,
            static_id: CounterId::NONE,
            sample: sample_payload::<T>,
        })
    }
}

fn sample_payload<T>(domain: &Domain, counter_id: CounterId, value: &T) {
    nvtx_sys::counters::sample_bytes(domain.raw_handle(), counter_id.get(), bytes_of(value));
}

fn bytes_of<T>(value: &T) -> &[u8] {
    // SAFETY: A shared reference guarantees `size_of::<T>()` readable bytes.
    // Primitive implementations contain no uninitialized padding. Structured
    // counter types opt into the initialized-representation requirement through
    // `PayloadSchemaType`.
    unsafe { core::slice::from_raw_parts(core::ptr::from_ref(value).cast(), size_of::<T>()) }
}

fn validate_static_id(id: u64) -> Result<(), CounterError> {
    if (nvtx_sys::counters::COUNTER_ID_STATIC_START..nvtx_sys::counters::COUNTER_ID_DYNAMIC_START)
        .contains(&id)
    {
        Ok(())
    } else {
        Err(CounterError::InvalidStaticId)
    }
}
