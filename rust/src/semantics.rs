// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use alloc::{borrow::ToOwned, ffi::CString, vec::Vec};
use core::{
    ffi::{c_void, CStr},
    mem::{discriminant, size_of},
    ptr::{addr_of, addr_of_mut},
};

use crate::Domain;

/// Error returned while constructing semantic metadata.
#[derive(Debug, Clone, Copy, PartialEq)]
#[non_exhaustive]
pub enum SemanticError {
    /// A unit scale numerator or denominator was zero.
    ZeroUnitScale,
    /// Counter limits used different numeric representations.
    LimitTypeMismatch,
    /// A minimum counter limit was greater than its maximum.
    InvalidLimits,
    /// A statically assigned scope or time-domain ID was out of range.
    InvalidStaticId,
}

impl core::fmt::Display for SemanticError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::ZeroUnitScale => write!(f, "semantic unit scales must be non-zero"),
            Self::LimitTypeMismatch => write!(f, "counter limits must use the same numeric type"),
            Self::InvalidLimits => write!(f, "counter minimum cannot exceed maximum"),
            Self::InvalidStaticId => write!(f, "ID is outside the NVTX static-ID range"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for SemanticError {}

/// NVTX execution scope identifier.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ScopeId(u64);

macro_rules! scope_ids {
    ($($(#[$meta:meta])* $name:ident = $raw:ident;)+) => {
        impl ScopeId {
            $(
                $(#[$meta])*
                pub const $name: Self = Self(nvtx_sys::ffi::$raw as u64);
            )+
        }
    };
}

scope_ids! {
    /// No scope specified.
    NONE = NVTX_SCOPE_NONE;
    /// Root of a scope hierarchy.
    ROOT = NVTX_SCOPE_ROOT;
    /// Current machine.
    CURRENT_HW_MACHINE = NVTX_SCOPE_CURRENT_HW_MACHINE;
    /// Current hardware socket.
    CURRENT_HW_SOCKET = NVTX_SCOPE_CURRENT_HW_SOCKET;
    /// Current physical CPU.
    CURRENT_HW_CPU_PHYSICAL = NVTX_SCOPE_CURRENT_HW_CPU_PHYSICAL;
    /// Current logical CPU.
    CURRENT_HW_CPU_LOGICAL = NVTX_SCOPE_CURRENT_HW_CPU_LOGICAL;
    /// Innermost hardware execution context.
    CURRENT_HW_INNERMOST = NVTX_SCOPE_CURRENT_HW_INNERMOST;
    /// Current hypervisor.
    CURRENT_HYPERVISOR = NVTX_SCOPE_CURRENT_HYPERVISOR;
    /// Current virtual machine.
    CURRENT_VM = NVTX_SCOPE_CURRENT_VM;
    /// Current kernel.
    CURRENT_KERNEL = NVTX_SCOPE_CURRENT_KERNEL;
    /// Current container.
    CURRENT_CONTAINER = NVTX_SCOPE_CURRENT_CONTAINER;
    /// Current operating system.
    CURRENT_OS = NVTX_SCOPE_CURRENT_OS;
    /// Current software process.
    CURRENT_SW_PROCESS = NVTX_SCOPE_CURRENT_SW_PROCESS;
    /// Current software thread.
    CURRENT_SW_THREAD = NVTX_SCOPE_CURRENT_SW_THREAD;
    /// Innermost software execution context.
    CURRENT_SW_INNERMOST = NVTX_SCOPE_CURRENT_SW_INNERMOST;
}

impl ScopeId {
    /// Construct a scope ID from its raw value.
    #[must_use]
    pub const fn from_raw(raw: u64) -> Self {
        Self(raw)
    }

    /// Return the raw NVTX scope ID.
    #[must_use]
    pub const fn get(self) -> u64 {
        self.0
    }
}

/// A scope registered in a particular domain.
#[derive(Debug, Clone, Copy)]
pub struct RegisteredScope<'a> {
    id: ScopeId,
    domain: &'a Domain,
}

impl<'a> RegisteredScope<'a> {
    /// Return the scope identifier.
    #[must_use]
    pub const fn id(self) -> ScopeId {
        self.id
    }

    /// Return the owning domain.
    #[must_use]
    pub const fn domain(self) -> &'a Domain {
        self.domain
    }
}

/// Timestamp source identifier.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct TimestampType(u64);

macro_rules! timestamp_types {
    ($($(#[$meta:meta])* $name:ident = $raw:ident;)+) => {
        impl TimestampType {
            $(
                $(#[$meta])*
                pub const $name: Self = Self(nvtx_sys::ffi::$raw as u64);
            )+
        }
    };
}

timestamp_types! {
    /// Unknown timestamp source.
    NONE = NVTX_TIMESTAMP_TYPE_NONE;
    /// Timestamp supplied by the NVTX tool.
    TOOL_PROVIDED = NVTX_TIMESTAMP_TYPE_TOOL_PROVIDED;
    /// CPU timestamp counter.
    CPU_TSC = NVTX_TIMESTAMP_TYPE_CPU_TSC;
    /// Non-virtualized CPU timestamp counter.
    CPU_TSC_NONVIRTUALIZED = NVTX_TIMESTAMP_TYPE_CPU_TSC_NONVIRTUALIZED;
    /// POSIX realtime clock.
    CPU_CLOCK_GETTIME_REALTIME = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_REALTIME;
    /// Coarse POSIX realtime clock.
    CPU_CLOCK_GETTIME_REALTIME_COARSE = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_REALTIME_COARSE;
    /// POSIX monotonic clock.
    CPU_CLOCK_GETTIME_MONOTONIC = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC;
    /// Raw POSIX monotonic clock.
    CPU_CLOCK_GETTIME_MONOTONIC_RAW = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC_RAW;
    /// Coarse POSIX monotonic clock.
    CPU_CLOCK_GETTIME_MONOTONIC_COARSE = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_MONOTONIC_COARSE;
    /// POSIX boot-time clock.
    CPU_CLOCK_GETTIME_BOOTTIME = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_BOOTTIME;
    /// Process CPU time.
    CPU_CLOCK_GETTIME_PROCESS_CPUTIME_ID = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_PROCESS_CPUTIME_ID;
    /// Thread CPU time.
    CPU_CLOCK_GETTIME_THREAD_CPUTIME_ID = NVTX_TIMESTAMP_TYPE_CPU_CLOCK_GETTIME_THREAD_CPUTIME_ID;
    /// Windows performance counter.
    WIN_QPC = NVTX_TIMESTAMP_TYPE_WIN_QPC;
    /// Windows system file time.
    WIN_GSTAFT = NVTX_TIMESTAMP_TYPE_WIN_GSTAFT;
    /// Precise Windows system file time.
    WIN_GSTAFTP = NVTX_TIMESTAMP_TYPE_WIN_GSTAFTP;
    /// C `time`.
    C_TIME = NVTX_TIMESTAMP_TYPE_C_TIME;
    /// C `clock`.
    C_CLOCK = NVTX_TIMESTAMP_TYPE_C_CLOCK;
    /// C `timespec_get`.
    C_TIMESPEC_GET = NVTX_TIMESTAMP_TYPE_C_TIMESPEC_GET;
    /// C++ steady clock.
    CPP_STEADY_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_STEADY_CLOCK;
    /// C++ high-resolution clock.
    CPP_HIGH_RESOLUTION_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_HIGH_RESOLUTION_CLOCK;
    /// C++ system clock.
    CPP_SYSTEM_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_SYSTEM_CLOCK;
    /// C++ UTC clock.
    CPP_UTC_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_UTC_CLOCK;
    /// C++ TAI clock.
    CPP_TAI_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_TAI_CLOCK;
    /// C++ GPS clock.
    CPP_GPS_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_GPS_CLOCK;
    /// C++ file clock.
    CPP_FILE_CLOCK = NVTX_TIMESTAMP_TYPE_CPP_FILE_CLOCK;
    /// GPU global timer.
    GPU_GLOBALTIMER = NVTX_TIMESTAMP_TYPE_GPU_GLOBALTIMER;
}

impl TimestampType {
    /// Construct a timestamp type from its raw value.
    #[must_use]
    pub const fn from_raw(raw: u64) -> Self {
        Self(raw)
    }

    /// Return the raw NVTX timestamp type.
    #[must_use]
    pub const fn get(self) -> u64 {
        self.0
    }
}

/// Identifier of a registered time domain.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct TimeDomainId(u64);

impl TimeDomainId {
    /// No time domain.
    pub const NONE: Self = Self(0);

    /// Construct an ID from its raw value.
    #[must_use]
    pub const fn from_raw(raw: u64) -> Self {
        Self(raw)
    }

    /// Construct an unambiguous time-domain ID from a predefined timestamp type.
    #[must_use]
    pub const fn predefined(timestamp_type: TimestampType) -> Self {
        Self(timestamp_type.get())
    }

    /// Return the raw NVTX ID.
    #[must_use]
    pub const fn get(self) -> u64 {
        self.0
    }
}

/// Properties of a registered timer.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct TimerFlags(u64);

impl TimerFlags {
    /// Start with no timer flags.
    #[must_use]
    pub const fn new() -> Self {
        Self(0)
    }

    /// Mark the clock monotonic.
    #[must_use]
    pub const fn monotonic(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_TIMER_FLAG_CLOCK_MONOTONIC as u64;
        self
    }

    /// Mark the clock steady.
    #[must_use]
    pub const fn steady(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_TIMER_FLAG_CLOCK_STEADY as u64;
        self
    }
}

/// Epoch or origin of a timer.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TimerStart {
    /// Unknown origin.
    Unknown,
    /// System boot.
    SystemBoot,
    /// Virtual machine boot.
    VirtualMachineBoot,
    /// Unix epoch.
    UnixEpoch,
    /// Windows FILETIME epoch.
    WindowsFileTime,
}

impl TimerStart {
    const fn raw(self) -> u64 {
        match self {
            Self::Unknown => nvtx_sys::ffi::NVTX_TIMER_START_UNKNOWN as u64,
            Self::SystemBoot => nvtx_sys::ffi::NVTX_TIMER_START_SYSTEM_BOOT as u64,
            Self::VirtualMachineBoot => nvtx_sys::ffi::NVTX_TIMER_START_VM_BOOT as u64,
            Self::UnixEpoch => nvtx_sys::ffi::NVTX_TIMER_START_UNIX_EPOCH as u64,
            Self::WindowsFileTime => nvtx_sys::ffi::NVTX_TIMER_START_WIN_FILETIME as u64,
        }
    }
}

/// Builder for registering a time domain.
#[derive(Debug, Clone, Copy)]
pub struct TimeDomainBuilder {
    timestamp_type: TimestampType,
    scope: ScopeId,
    flags: TimerFlags,
    resolution: i64,
    start: TimerStart,
    static_id: TimeDomainId,
}

impl TimeDomainBuilder {
    /// Start a time domain for the given timestamp source.
    #[must_use]
    pub const fn new(timestamp_type: TimestampType) -> Self {
        Self {
            timestamp_type,
            scope: ScopeId::NONE,
            flags: TimerFlags::new(),
            resolution: 0,
            start: TimerStart::Unknown,
            static_id: TimeDomainId::NONE,
        }
    }

    /// Associate the time domain with a scope.
    #[must_use]
    pub const fn scope(mut self, scope: ScopeId) -> Self {
        self.scope = scope;
        self
    }

    /// Set timer properties.
    #[must_use]
    pub const fn flags(mut self, flags: TimerFlags) -> Self {
        self.flags = flags;
        self
    }

    /// Set timer ticks per second.
    #[must_use]
    pub const fn resolution(mut self, ticks_per_second: i64) -> Self {
        self.resolution = ticks_per_second;
        self
    }

    /// Set the timer origin.
    #[must_use]
    pub const fn start(mut self, start: TimerStart) -> Self {
        self.start = start;
        self
    }

    /// Request a static time-domain ID.
    ///
    /// # Errors
    /// Returns [`SemanticError::InvalidStaticId`] when `id` is outside the
    /// user-defined static-ID range.
    pub fn static_id(mut self, id: TimeDomainId) -> Result<Self, SemanticError> {
        validate_static_id(id.get())?;
        self.static_id = id;
        Ok(self)
    }
}

/// A time domain registered in a particular NVTX domain.
#[derive(Debug, Clone, Copy)]
pub struct RegisteredTimeDomain<'a> {
    id: TimeDomainId,
    domain: &'a Domain,
}

impl<'a> RegisteredTimeDomain<'a> {
    /// Return the time-domain ID.
    #[must_use]
    pub const fn id(self) -> TimeDomainId {
        self.id
    }

    /// Return the owning NVTX domain.
    #[must_use]
    pub const fn domain(self) -> &'a Domain {
        self.domain
    }

    /// Register a timer callback.
    ///
    /// # Safety
    /// `provider` must remain callable for as long as an NVTX tool may invoke
    /// it, use the C ABI, and never unwind.
    pub unsafe fn set_timer_source(
        self,
        safe_after_process_teardown: bool,
        provider: nvtx_sys::payload::TimestampProvider,
    ) {
        let flags = u64::from(!safe_after_process_teardown);
        // SAFETY: The caller accepts the callback lifetime and ABI requirements.
        unsafe {
            nvtx_sys::payload::timer_source(
                self.domain.raw_handle(),
                self.id.get(),
                flags,
                Some(provider),
            );
        }
    }

    /// Register a timer callback with an opaque data pointer.
    ///
    /// # Safety
    /// `provider` and `data` must remain valid for as long as an NVTX tool may
    /// invoke the callback. The callback must use the C ABI and never unwind.
    pub unsafe fn set_timer_source_with_data(
        self,
        safe_after_process_teardown: bool,
        provider: nvtx_sys::payload::TimestampProviderWithData,
        data: *mut c_void,
    ) {
        let flags = u64::from(!safe_after_process_teardown);
        // SAFETY: The caller accepts the callback and data lifetime requirements.
        unsafe {
            nvtx_sys::payload::timer_source_with_data(
                self.domain.raw_handle(),
                self.id.get(),
                flags,
                Some(provider),
                data,
            );
        }
    }

    /// Report one synchronization point to another time domain.
    pub fn sync_point(self, other: TimeDomainId, timestamp_self: i64, timestamp_other: i64) {
        nvtx_sys::payload::time_sync_point(
            self.domain.raw_handle(),
            self.id.get(),
            other.get(),
            timestamp_self,
            timestamp_other,
        );
    }

    /// Report synchronization points to another time domain.
    pub fn sync_points(self, other: TimeDomainId, points: &[SyncPoint]) {
        let raw: Vec<nvtx_sys::payload::SyncPoint> = points
            .iter()
            .map(|point| nvtx_sys::payload::SyncPoint {
                src: point.source,
                dst: point.destination,
            })
            .collect();
        nvtx_sys::payload::time_sync_point_table(
            self.domain.raw_handle(),
            self.id.get(),
            other.get(),
            &raw,
        );
    }

    /// Report a timestamp conversion factor to another time domain.
    pub fn conversion_factor(
        self,
        other: TimeDomainId,
        slope: f64,
        timestamp_self: i64,
        timestamp_other: i64,
    ) {
        nvtx_sys::payload::timestamp_conversion_factor(
            self.domain.raw_handle(),
            self.id.get(),
            other.get(),
            slope,
            timestamp_self,
            timestamp_other,
        );
    }
}

/// Pair of simultaneous timestamps in two time domains.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SyncPoint {
    /// Timestamp in the source time domain.
    pub source: i64,
    /// Timestamp in the destination time domain.
    pub destination: i64,
}

/// Numeric counter limit.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum CounterLimit {
    /// Signed integer limit.
    I64(i64),
    /// Unsigned integer limit.
    U64(u64),
    /// Floating-point limit.
    F64(f64),
}

impl From<i64> for CounterLimit {
    fn from(value: i64) -> Self {
        Self::I64(value)
    }
}

impl From<u64> for CounterLimit {
    fn from(value: u64) -> Self {
        Self::U64(value)
    }
}

impl From<f64> for CounterLimit {
    fn from(value: f64) -> Self {
        Self::F64(value)
    }
}

/// Meaning of a counter value relative to earlier samples.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CounterValueType {
    /// Absolute value.
    Absolute,
    /// Delta from the previous sample.
    Delta,
    /// Delta from the first sample.
    DeltaSinceStart,
}

/// Counter interpolation behavior.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CounterInterpolation {
    /// No interpolation.
    Point,
    /// Piecewise constant since the previous sample.
    SinceLast,
    /// Piecewise constant until the next sample.
    UntilNext,
    /// Linear interpolation.
    Linear,
}

/// Owned counter semantic metadata.
#[derive(Debug, Clone)]
pub struct CounterSemantic {
    unit: Option<CString>,
    unit_scale: (u64, u64),
    normalize: bool,
    value_type: Option<CounterValueType>,
    interpolation: Option<CounterInterpolation>,
    minimum: Option<CounterLimit>,
    maximum: Option<CounterLimit>,
}

impl Default for CounterSemantic {
    fn default() -> Self {
        Self::new()
    }
}

impl CounterSemantic {
    /// Start an empty counter semantic.
    #[must_use]
    pub const fn new() -> Self {
        Self {
            unit: None,
            unit_scale: (1, 1),
            normalize: false,
            value_type: None,
            interpolation: None,
            minimum: None,
            maximum: None,
        }
    }

    /// Set the counter unit.
    #[must_use]
    pub fn unit(mut self, unit: &CStr) -> Self {
        self.unit = Some(unit.to_owned());
        self
    }

    /// Set the unit scale fraction.
    ///
    /// # Errors
    /// Returns [`SemanticError::ZeroUnitScale`] when either component is zero.
    pub fn unit_scale(mut self, numerator: u64, denominator: u64) -> Result<Self, SemanticError> {
        if numerator == 0 || denominator == 0 {
            return Err(SemanticError::ZeroUnitScale);
        }
        self.unit_scale = (numerator, denominator);
        Ok(self)
    }

    /// Normalize fixed-point values.
    #[must_use]
    pub const fn normalize(mut self) -> Self {
        self.normalize = true;
        self
    }

    /// Set the counter value interpretation.
    #[must_use]
    pub const fn value_type(mut self, value_type: CounterValueType) -> Self {
        self.value_type = Some(value_type);
        self
    }

    /// Set counter interpolation.
    #[must_use]
    pub const fn interpolation(mut self, interpolation: CounterInterpolation) -> Self {
        self.interpolation = Some(interpolation);
        self
    }

    /// Set both counter limits.
    ///
    /// # Errors
    /// Returns an error when the limits use different numeric types or the
    /// minimum exceeds the maximum.
    pub fn limits(
        mut self,
        minimum: impl Into<CounterLimit>,
        maximum: impl Into<CounterLimit>,
    ) -> Result<Self, SemanticError> {
        let minimum = minimum.into();
        let maximum = maximum.into();
        validate_limits(minimum, maximum)?;
        self.minimum = Some(minimum);
        self.maximum = Some(maximum);
        Ok(self)
    }

    /// Set the minimum counter limit.
    ///
    /// # Errors
    /// Returns an error when an existing maximum has a different numeric type
    /// or is less than `minimum`.
    pub fn minimum(mut self, minimum: impl Into<CounterLimit>) -> Result<Self, SemanticError> {
        let minimum = minimum.into();
        if let Some(maximum) = self.maximum {
            validate_limits(minimum, maximum)?;
        }
        self.minimum = Some(minimum);
        Ok(self)
    }

    /// Set the maximum counter limit.
    ///
    /// # Errors
    /// Returns an error when an existing minimum has a different numeric type
    /// or exceeds `maximum`.
    pub fn maximum(mut self, maximum: impl Into<CounterLimit>) -> Result<Self, SemanticError> {
        let maximum = maximum.into();
        if let Some(minimum) = self.minimum {
            validate_limits(minimum, maximum)?;
        }
        self.maximum = Some(maximum);
        Ok(self)
    }
}

/// Semantic metadata assigning a scope.
#[derive(Debug, Clone, Copy)]
pub struct ScopeSemantic {
    scope: ScopeId,
}

impl ScopeSemantic {
    /// Assign the given scope.
    #[must_use]
    pub const fn new(scope: ScopeId) -> Self {
        Self { scope }
    }
}

/// Semantic metadata assigning a time domain.
#[derive(Debug, Clone, Copy)]
pub struct TimeSemantic {
    time_domain: TimeDomainId,
}

impl TimeSemantic {
    /// Assign a registered time domain.
    #[must_use]
    pub const fn new(time_domain: TimeDomainId) -> Self {
        Self { time_domain }
    }

    /// Assign a predefined timestamp source.
    #[must_use]
    pub const fn predefined(timestamp_type: TimestampType) -> Self {
        Self {
            time_domain: TimeDomainId::predefined(timestamp_type),
        }
    }
}

/// Semantic metadata assigning a cross-event correlation.
#[derive(Debug, Clone)]
pub struct CorrelationSemantic {
    domain_uuid: [u8; 16],
    display_name: Option<CString>,
    role: u64,
}

impl CorrelationSemantic {
    /// Start correlation metadata for a globally unique domain UUID.
    #[must_use]
    pub const fn new(domain_uuid: [u8; 16]) -> Self {
        Self {
            domain_uuid,
            display_name: None,
            role: nvtx_sys::ffi::NVTX_CORRELATION_ROLE_NONE as u64,
        }
    }

    /// Set the correlation domain display name.
    #[must_use]
    pub fn display_name(mut self, display_name: &CStr) -> Self {
        self.display_name = Some(display_name.to_owned());
        self
    }

    /// Set the correlation role.
    #[must_use]
    pub const fn role(mut self, role: u64) -> Self {
        self.role = role;
        self
    }
}

/// One semantic extension.
#[derive(Debug, Clone)]
pub enum Semantic {
    /// Counter interpretation.
    Counter(CounterSemantic),
    /// Scope assignment.
    Scope(ScopeSemantic),
    /// Time-domain assignment.
    Time(TimeSemantic),
    /// Correlation assignment.
    Correlation(CorrelationSemantic),
}

impl From<CounterSemantic> for Semantic {
    fn from(value: CounterSemantic) -> Self {
        Self::Counter(value)
    }
}

impl From<ScopeSemantic> for Semantic {
    fn from(value: ScopeSemantic) -> Self {
        Self::Scope(value)
    }
}

impl From<TimeSemantic> for Semantic {
    fn from(value: TimeSemantic) -> Self {
        Self::Time(value)
    }
}

impl From<CorrelationSemantic> for Semantic {
    fn from(value: CorrelationSemantic) -> Self {
        Self::Correlation(value)
    }
}

/// Owned heterogeneous linked list of semantic extensions.
#[derive(Debug, Clone, Default)]
pub struct SemanticChain {
    semantics: Vec<Semantic>,
}

impl SemanticChain {
    /// Start an empty chain.
    #[must_use]
    pub const fn new() -> Self {
        Self {
            semantics: Vec::new(),
        }
    }

    /// Append a semantic extension.
    #[must_use]
    pub fn semantic(mut self, semantic: impl Into<Semantic>) -> Self {
        self.semantics.push(semantic.into());
        self
    }

    pub(crate) fn encode(&self) -> EncodedSemanticChain {
        EncodedSemanticChain::new(&self.semantics)
    }
}

enum RawSemanticNode {
    Counter(nvtx_sys::payload::CounterSemantics),
    Scope(nvtx_sys::payload::ScopeSemantics),
    Time(nvtx_sys::payload::TimeSemantics),
    Correlation(nvtx_sys::payload::CorrelationSemantics),
}

impl RawSemanticNode {
    fn header(&self) -> *const nvtx_sys::payload::SemanticsHeader {
        match self {
            Self::Counter(value) => addr_of!(value.header),
            Self::Scope(value) => addr_of!(value.header),
            Self::Time(value) => addr_of!(value.header),
            Self::Correlation(value) => addr_of!(value.header),
        }
    }

    fn header_mut(&mut self) -> *mut nvtx_sys::payload::SemanticsHeader {
        match self {
            Self::Counter(value) => addr_of_mut!(value.header),
            Self::Scope(value) => addr_of_mut!(value.header),
            Self::Time(value) => addr_of_mut!(value.header),
            Self::Correlation(value) => addr_of_mut!(value.header),
        }
    }
}

pub(crate) struct EncodedSemanticChain {
    nodes: Vec<RawSemanticNode>,
}

impl EncodedSemanticChain {
    fn new(semantics: &[Semantic]) -> Self {
        let mut nodes: Vec<RawSemanticNode> = semantics.iter().map(encode_semantic).collect();
        let pointers: Vec<*const nvtx_sys::payload::SemanticsHeader> =
            nodes.iter().map(RawSemanticNode::header).collect();
        for (index, node) in nodes.iter_mut().enumerate() {
            let next = pointers
                .get(index + 1)
                .copied()
                .unwrap_or(core::ptr::null());
            // SAFETY: `nodes` is fully allocated before pointers are taken and
            // is not structurally modified while the chain is in use.
            unsafe {
                (*node.header_mut()).next = next;
            }
        }
        Self { nodes }
    }

    pub(crate) fn head(&self) -> *const nvtx_sys::payload::SemanticsHeader {
        self.nodes
            .first()
            .map_or(core::ptr::null(), RawSemanticNode::header)
    }
}

impl Domain {
    /// Register a custom scope in this domain.
    ///
    /// # Errors
    /// Returns [`SemanticError::InvalidStaticId`] when `static_id` is outside
    /// the user-defined static-ID range.
    pub fn register_scope(
        &self,
        path: &CStr,
        parent: ScopeId,
        static_id: Option<ScopeId>,
    ) -> Result<RegisteredScope<'_>, SemanticError> {
        if let Some(id) = static_id {
            validate_static_id(id.get())?;
        }
        let attributes = nvtx_sys::payload::ScopeAttributes {
            structSize: size_of::<nvtx_sys::payload::ScopeAttributes>(),
            path: path.as_ptr(),
            parentScope: parent.get(),
            scopeId: static_id.unwrap_or(ScopeId::NONE).get(),
        };
        // SAFETY: `path` is a valid C string for the complete call.
        let id = unsafe { nvtx_sys::payload::scope_register(self.raw_handle(), &attributes) };
        Ok(RegisteredScope {
            id: ScopeId::from_raw(id),
            domain: self,
        })
    }

    /// Register a time domain.
    #[must_use]
    pub fn register_time_domain(&self, builder: TimeDomainBuilder) -> RegisteredTimeDomain<'_> {
        let attributes = nvtx_sys::payload::TimeDomainAttributes {
            scopeId: builder.scope.get(),
            timestampTypeId: builder.timestamp_type.get(),
            timeDomainId: builder.static_id.get(),
            timerFlags: builder.flags.0,
            timerResolution: builder.resolution,
            timerStart: builder.start.raw(),
        };
        let id = nvtx_sys::payload::time_domain_register(self.raw_handle(), &attributes);
        RegisteredTimeDomain {
            id: TimeDomainId::from_raw(id),
            domain: self,
        }
    }

    /// Return whether this domain is enabled by the active tool.
    #[must_use]
    pub fn is_enabled(&self) -> bool {
        nvtx_sys::payload::domain_is_enabled(self.raw_handle())
    }
}

/// Read a timestamp from the active NVTX handler.
#[must_use]
pub fn timestamp() -> i64 {
    nvtx_sys::payload::timestamp_get()
}

fn encode_semantic(semantic: &Semantic) -> RawSemanticNode {
    match semantic {
        Semantic::Counter(value) => {
            let mut flags = 0_u64;
            if value.normalize {
                flags |= nvtx_sys::ffi::NVTX_COUNTER_FLAG_NORMALIZE as u64;
            }
            flags |= match value.value_type {
                None => 0,
                Some(CounterValueType::Absolute) => {
                    nvtx_sys::ffi::NVTX_COUNTER_FLAG_VALUETYPE_ABSOLUTE as u64
                }
                Some(CounterValueType::Delta) => {
                    nvtx_sys::ffi::NVTX_COUNTER_FLAG_VALUETYPE_DELTA as u64
                }
                Some(CounterValueType::DeltaSinceStart) => {
                    nvtx_sys::ffi::NVTX_COUNTER_FLAG_VALUETYPE_DELTA_SINCE_START as u64
                }
            };
            flags |= match value.interpolation {
                None => 0,
                Some(CounterInterpolation::Point) => {
                    nvtx_sys::ffi::NVTX_COUNTER_FLAG_INTERPOLATION_POINT as u64
                }
                Some(CounterInterpolation::SinceLast) => {
                    nvtx_sys::ffi::NVTX_COUNTER_FLAG_INTERPOLATION_SINCE_LAST as u64
                }
                Some(CounterInterpolation::UntilNext) => {
                    nvtx_sys::ffi::NVTX_COUNTER_FLAG_INTERPOLATION_UNTIL_NEXT as u64
                }
                Some(CounterInterpolation::Linear) => {
                    nvtx_sys::ffi::NVTX_COUNTER_FLAG_INTERPOLATION_LINEAR as u64
                }
            };
            if value.minimum.is_some() {
                flags |= nvtx_sys::ffi::NVTX_COUNTER_FLAG_LIMIT_MIN as u64;
            }
            if value.maximum.is_some() {
                flags |= nvtx_sys::ffi::NVTX_COUNTER_FLAG_LIMIT_MAX as u64;
            }
            let limit = value.minimum.or(value.maximum);
            RawSemanticNode::Counter(nvtx_sys::payload::CounterSemantics {
                header: semantic_header(
                    size_of::<nvtx_sys::payload::CounterSemantics>(),
                    nvtx_sys::ffi::NVTX_SEMANTIC_ID_COUNTERS_V1,
                    nvtx_sys::ffi::NVTX_COUNTER_SEMANTIC_VERSION,
                ),
                flags,
                unit: value
                    .unit
                    .as_ref()
                    .map_or(core::ptr::null(), |unit| unit.as_ptr()),
                unitScaleNumerator: value.unit_scale.0,
                unitScaleDenominator: value.unit_scale.1,
                limitType: limit.map_or(
                    i64::from(nvtx_sys::ffi::NVTX_COUNTER_LIMIT_UNDEFINED),
                    counter_limit_type,
                ),
                min: encode_limit(value.minimum),
                max: encode_limit(value.maximum),
            })
        }
        Semantic::Scope(value) => RawSemanticNode::Scope(nvtx_sys::payload::ScopeSemantics {
            header: semantic_header(
                size_of::<nvtx_sys::payload::ScopeSemantics>(),
                nvtx_sys::ffi::NVTX_SEMANTIC_ID_SCOPE_V1,
                nvtx_sys::ffi::NVTX_SCOPE_SEMANTIC_VERSION,
            ),
            scopeId: value.scope.get(),
        }),
        Semantic::Time(value) => RawSemanticNode::Time(nvtx_sys::payload::TimeSemantics {
            header: semantic_header(
                size_of::<nvtx_sys::payload::TimeSemantics>(),
                nvtx_sys::ffi::NVTX_SEMANTIC_ID_TIME_V1,
                nvtx_sys::ffi::NVTX_TIME_SEMANTIC_VERSION,
            ),
            timeDomainId: value.time_domain.get(),
        }),
        Semantic::Correlation(value) => {
            RawSemanticNode::Correlation(nvtx_sys::payload::CorrelationSemantics {
                header: semantic_header(
                    size_of::<nvtx_sys::payload::CorrelationSemantics>(),
                    nvtx_sys::ffi::NVTX_SEMANTIC_ID_CORRELATION_V1,
                    nvtx_sys::ffi::NVTX_CORRELATION_SEMANTIC_VERSION,
                ),
                correlationDomainUuid: value.domain_uuid,
                displayName: value
                    .display_name
                    .as_ref()
                    .map_or(core::ptr::null(), |name| name.as_ptr()),
                role: value.role,
            })
        }
    }
}

fn semantic_header(
    size: usize,
    semantic_id: i32,
    version: i32,
) -> nvtx_sys::payload::SemanticsHeader {
    nvtx_sys::payload::SemanticsHeader {
        structSize: size as u32,
        semanticId: semantic_id as u16,
        version: version as u16,
        next: core::ptr::null(),
    }
}

fn encode_limit(limit: Option<CounterLimit>) -> nvtx_sys::ffi::nvtxCounterLimit_t {
    match limit {
        None => nvtx_sys::ffi::nvtxCounterLimit_t { u64_: 0 },
        Some(CounterLimit::I64(value)) => nvtx_sys::ffi::nvtxCounterLimit_t { i64_: value },
        Some(CounterLimit::U64(value)) => nvtx_sys::ffi::nvtxCounterLimit_t { u64_: value },
        Some(CounterLimit::F64(value)) => nvtx_sys::ffi::nvtxCounterLimit_t { f64_: value },
    }
}

fn counter_limit_type(limit: CounterLimit) -> i64 {
    match limit {
        CounterLimit::I64(_) => i64::from(nvtx_sys::ffi::NVTX_COUNTER_LIMIT_I64),
        CounterLimit::U64(_) => i64::from(nvtx_sys::ffi::NVTX_COUNTER_LIMIT_U64),
        CounterLimit::F64(_) => i64::from(nvtx_sys::ffi::NVTX_COUNTER_LIMIT_F64),
    }
}

fn validate_limits(minimum: CounterLimit, maximum: CounterLimit) -> Result<(), SemanticError> {
    if discriminant(&minimum) != discriminant(&maximum) {
        return Err(SemanticError::LimitTypeMismatch);
    }
    let valid = match (minimum, maximum) {
        (CounterLimit::I64(minimum), CounterLimit::I64(maximum)) => minimum <= maximum,
        (CounterLimit::U64(minimum), CounterLimit::U64(maximum)) => minimum <= maximum,
        (CounterLimit::F64(minimum), CounterLimit::F64(maximum)) => minimum <= maximum,
        _ => false,
    };
    if valid {
        Ok(())
    } else {
        Err(SemanticError::InvalidLimits)
    }
}

fn validate_static_id(id: u64) -> Result<(), SemanticError> {
    if (nvtx_sys::payload::SCOPE_ID_STATIC_START..nvtx_sys::payload::SCOPE_ID_DYNAMIC_START)
        .contains(&id)
    {
        Ok(())
    } else {
        Err(SemanticError::InvalidStaticId)
    }
}

#[cfg(all(test, feature = "std"))]
mod tests {
    use super::*;

    #[test]
    fn semantic_chain_wires_headers_in_order() {
        let chain = SemanticChain::new()
            .semantic(ScopeSemantic::new(ScopeId::CURRENT_SW_PROCESS))
            .semantic(TimeSemantic::predefined(TimestampType::CPU_TSC));
        let encoded = chain.encode();
        let first = encoded.head();
        assert!(!first.is_null());
        // SAFETY: `encoded` owns both nodes and keeps their addresses stable.
        let first = unsafe { *first };
        assert_eq!(
            first.semanticId,
            nvtx_sys::ffi::NVTX_SEMANTIC_ID_SCOPE_V1 as u16
        );
        let second = first.next;
        assert!(!second.is_null());
        // SAFETY: The first header's next pointer addresses the second owned node.
        let second = unsafe { *second };
        assert_eq!(
            second.semanticId,
            nvtx_sys::ffi::NVTX_SEMANTIC_ID_TIME_V1 as u16
        );
        assert!(second.next.is_null());
    }

    #[test]
    fn counter_limits_encode_the_selected_union_member() {
        let semantic = CounterSemantic::new().limits(-5_i64, 10_i64).unwrap();
        let chain = SemanticChain::new().semantic(semantic);
        let encoded = chain.encode();
        let raw = encoded.head().cast::<nvtx_sys::payload::CounterSemantics>();
        // SAFETY: The only node was created from `CounterSemantic`, so this
        // header pointer addresses a `CounterSemantics` value.
        let raw = unsafe { *raw };
        assert_eq!(
            raw.limitType,
            i64::from(nvtx_sys::ffi::NVTX_COUNTER_LIMIT_I64)
        );
        // SAFETY: `limitType` above selects the signed union member.
        let minimum = unsafe { raw.min.i64_ };
        // SAFETY: `limitType` above selects the signed union member.
        let maximum = unsafe { raw.max.i64_ };
        assert_eq!(minimum, -5);
        assert_eq!(maximum, 10);
    }
}
