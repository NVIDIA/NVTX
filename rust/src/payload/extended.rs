// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use alloc::{borrow::ToOwned, ffi::CString, string::String, vec::Vec};
use core::{
    ffi::{c_void, CStr},
    marker::PhantomData,
};

use crate::{semantics::EncodedSemanticChain, Domain, SemanticChain};

/// Error returned while constructing an extended-payload description.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub enum PayloadError {
    /// More than one pointer/address interpretation was selected.
    ConflictingAddressModes,
    /// More than one array interpretation was selected.
    ConflictingArrayModes,
    /// More than one event-attribute interpretation was selected.
    ConflictingEventAttributes,
    /// More than one event role was selected.
    ConflictingEventRoles,
    /// A schema was built without any entries.
    EmptySchema,
    /// An enumeration was built without any entries.
    EmptyEnumeration,
    /// A payload data object referenced an empty byte slice.
    EmptyPayload,
    /// A statically assigned ID was outside the NVTX static-ID range.
    InvalidStaticId,
    /// More payload descriptors were supplied than the core event ABI can encode.
    TooManyPayloads,
    /// A flexible-data offset exceeded the supplied flexible-data buffer.
    InvalidFlexDataOffset,
}

impl core::fmt::Display for PayloadError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::ConflictingAddressModes => write!(f, "conflicting payload address modes"),
            Self::ConflictingArrayModes => write!(f, "conflicting payload array modes"),
            Self::ConflictingEventAttributes => {
                write!(f, "conflicting payload event attributes")
            }
            Self::ConflictingEventRoles => write!(f, "conflicting payload event roles"),
            Self::EmptySchema => write!(f, "payload schema must contain at least one entry"),
            Self::EmptyEnumeration => {
                write!(f, "payload enumeration must contain at least one entry")
            }
            Self::EmptyPayload => write!(f, "extended payload data cannot be empty"),
            Self::InvalidStaticId => write!(f, "ID is outside the NVTX static-ID range"),
            Self::TooManyPayloads => write!(f, "too many payload descriptors for one event"),
            Self::InvalidFlexDataOffset => {
                write!(f, "flexible-data offset exceeds the buffer size")
            }
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for PayloadError {}

/// Identifier of a registered payload schema.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct SchemaId(u64);

impl SchemaId {
    /// No schema or a failed registration.
    pub const NONE: Self = Self(0);

    /// Construct an ID from its raw value.
    #[must_use]
    pub const fn from_raw(raw: u64) -> Self {
        Self(raw)
    }

    /// Return the raw NVTX ID.
    #[must_use]
    pub const fn get(self) -> u64 {
        self.0
    }
}

/// Identifier of a registered payload enumeration.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct EnumId(u64);

impl EnumId {
    /// No enumeration or a failed registration.
    pub const NONE: Self = Self(0);

    /// Construct an ID from its raw value.
    #[must_use]
    pub const fn from_raw(raw: u64) -> Self {
        Self(raw)
    }

    /// Return the raw NVTX ID.
    #[must_use]
    pub const fn get(self) -> u64 {
        self.0
    }
}

/// Type identifier used by a payload schema entry.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct PayloadEntryType(u64);

macro_rules! payload_entry_types {
    ($($(#[$meta:meta])* $name:ident = $raw:ident;)+) => {
        impl PayloadEntryType {
            $(
                $(#[$meta])*
                pub const $name: Self = Self(nvtx_sys::ffi::$raw as u64);
            )+
        }
    };
}

payload_entry_types! {
    /// Invalid or unspecified type.
    INVALID = NVTX_PAYLOAD_ENTRY_TYPE_INVALID;
    /// C `char`.
    CHAR = NVTX_PAYLOAD_ENTRY_TYPE_CHAR;
    /// C `unsigned char`.
    UCHAR = NVTX_PAYLOAD_ENTRY_TYPE_UCHAR;
    /// C `short`.
    SHORT = NVTX_PAYLOAD_ENTRY_TYPE_SHORT;
    /// C `unsigned short`.
    USHORT = NVTX_PAYLOAD_ENTRY_TYPE_USHORT;
    /// C `int`.
    INT = NVTX_PAYLOAD_ENTRY_TYPE_INT;
    /// C `unsigned int`.
    UINT = NVTX_PAYLOAD_ENTRY_TYPE_UINT;
    /// C `long`.
    LONG = NVTX_PAYLOAD_ENTRY_TYPE_LONG;
    /// C `unsigned long`.
    ULONG = NVTX_PAYLOAD_ENTRY_TYPE_ULONG;
    /// C `long long`.
    LONGLONG = NVTX_PAYLOAD_ENTRY_TYPE_LONGLONG;
    /// C `unsigned long long`.
    ULONGLONG = NVTX_PAYLOAD_ENTRY_TYPE_ULONGLONG;
    /// Signed 8-bit integer.
    INT8 = NVTX_PAYLOAD_ENTRY_TYPE_INT8;
    /// Unsigned 8-bit integer.
    UINT8 = NVTX_PAYLOAD_ENTRY_TYPE_UINT8;
    /// Signed 16-bit integer.
    INT16 = NVTX_PAYLOAD_ENTRY_TYPE_INT16;
    /// Unsigned 16-bit integer.
    UINT16 = NVTX_PAYLOAD_ENTRY_TYPE_UINT16;
    /// Signed 32-bit integer.
    INT32 = NVTX_PAYLOAD_ENTRY_TYPE_INT32;
    /// Unsigned 32-bit integer.
    UINT32 = NVTX_PAYLOAD_ENTRY_TYPE_UINT32;
    /// Signed 64-bit integer.
    INT64 = NVTX_PAYLOAD_ENTRY_TYPE_INT64;
    /// Unsigned 64-bit integer.
    UINT64 = NVTX_PAYLOAD_ENTRY_TYPE_UINT64;
    /// C `float`.
    FLOAT = NVTX_PAYLOAD_ENTRY_TYPE_FLOAT;
    /// C `double`.
    DOUBLE = NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE;
    /// C `long double`.
    LONG_DOUBLE = NVTX_PAYLOAD_ENTRY_TYPE_LONGDOUBLE;
    /// C `size_t`.
    SIZE = NVTX_PAYLOAD_ENTRY_TYPE_SIZE;
    /// Address-sized pointer value.
    ADDRESS = NVTX_PAYLOAD_ENTRY_TYPE_ADDRESS;
    /// Wide character.
    WCHAR = NVTX_PAYLOAD_ENTRY_TYPE_WCHAR;
    /// UTF-8 code unit.
    CHAR8 = NVTX_PAYLOAD_ENTRY_TYPE_CHAR8;
    /// UTF-16 code unit.
    CHAR16 = NVTX_PAYLOAD_ENTRY_TYPE_CHAR16;
    /// UTF-32 code unit.
    CHAR32 = NVTX_PAYLOAD_ENTRY_TYPE_CHAR32;
    /// Raw byte.
    BYTE = NVTX_PAYLOAD_ENTRY_TYPE_BYTE;
    /// Signed 128-bit integer.
    INT128 = NVTX_PAYLOAD_ENTRY_TYPE_INT128;
    /// Unsigned 128-bit integer.
    UINT128 = NVTX_PAYLOAD_ENTRY_TYPE_UINT128;
    /// IEEE-754 binary16.
    FLOAT16 = NVTX_PAYLOAD_ENTRY_TYPE_FLOAT16;
    /// IEEE-754 binary32.
    FLOAT32 = NVTX_PAYLOAD_ENTRY_TYPE_FLOAT32;
    /// IEEE-754 binary64.
    FLOAT64 = NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64;
    /// IEEE-754 binary128.
    FLOAT128 = NVTX_PAYLOAD_ENTRY_TYPE_FLOAT128;
    /// bfloat16.
    BF16 = NVTX_PAYLOAD_ENTRY_TYPE_BF16;
    /// TensorFloat-32.
    TF32 = NVTX_PAYLOAD_ENTRY_TYPE_TF32;
    /// NVTX category ID.
    CATEGORY = NVTX_PAYLOAD_ENTRY_TYPE_CATEGORY;
    /// NVTX ARGB color.
    COLOR_ARGB = NVTX_PAYLOAD_ENTRY_TYPE_COLOR_ARGB;
    /// NVTX scope ID.
    SCOPE_ID = NVTX_PAYLOAD_ENTRY_TYPE_SCOPE_ID;
    /// 32-bit process ID.
    PID_UINT32 = NVTX_PAYLOAD_ENTRY_TYPE_PID_UINT32;
    /// 64-bit process ID.
    PID_UINT64 = NVTX_PAYLOAD_ENTRY_TYPE_PID_UINT64;
    /// 32-bit thread ID.
    TID_UINT32 = NVTX_PAYLOAD_ENTRY_TYPE_TID_UINT32;
    /// 64-bit thread ID.
    TID_UINT64 = NVTX_PAYLOAD_ENTRY_TYPE_TID_UINT64;
    /// Locale-dependent C string.
    CSTRING = NVTX_PAYLOAD_ENTRY_TYPE_CSTRING;
    /// UTF-8 C string.
    CSTRING_UTF8 = NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF8;
    /// UTF-16 C string.
    CSTRING_UTF16 = NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF16;
    /// UTF-32 C string.
    CSTRING_UTF32 = NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32;
    /// Registered NVTX string handle.
    REGISTERED_STRING = NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE;
    /// Union selector.
    UNION_SELECTOR = NVTX_PAYLOAD_ENTRY_TYPE_UNION_SELECTOR;
}

impl PayloadEntryType {
    /// Use a registered nested schema as an entry type.
    #[must_use]
    pub const fn schema(schema: SchemaId) -> Self {
        Self(schema.get())
    }

    /// Use a registered enumeration as an entry type.
    #[must_use]
    pub const fn enumeration(enumeration: EnumId) -> Self {
        Self(enumeration.get())
    }

    /// Construct a type from a raw NVTX value.
    #[must_use]
    pub const fn from_raw(raw: u64) -> Self {
        Self(raw)
    }

    /// Return the raw NVTX value.
    #[must_use]
    pub const fn get(self) -> u64 {
        self.0
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum AddressMode {
    Pointer,
    OffsetFromBase,
    OffsetFromHere,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ArrayMode {
    FixedSize,
    ZeroTerminated,
    LengthIndex,
    LengthPayloadIndex,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum EventAttribute {
    Message,
    Timestamp,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum EventRole {
    RangeBegin,
    RangeEnd,
    Mark,
    Counter,
}

/// Validated builder for payload schema entry flags.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PayloadEntryFlag {
    address: Option<AddressMode>,
    array: Option<ArrayMode>,
    event_attribute: Option<EventAttribute>,
    event_role: Option<EventRole>,
    deep_copy: bool,
    hidden: bool,
    conflict: Option<PayloadError>,
}

impl Default for PayloadEntryFlag {
    fn default() -> Self {
        Self::new()
    }
}

impl PayloadEntryFlag {
    /// Start with no entry flags.
    #[must_use]
    pub const fn new() -> Self {
        Self {
            address: None,
            array: None,
            event_attribute: None,
            event_role: None,
            deep_copy: false,
            hidden: false,
            conflict: None,
        }
    }

    /// Start an event-message entry.
    #[must_use]
    pub const fn event() -> Self {
        Self {
            event_attribute: Some(EventAttribute::Message),
            ..Self::new()
        }
    }

    fn record_conflict(&mut self, conflicting: bool, error: PayloadError) {
        if conflicting && self.conflict.is_none() {
            self.conflict = Some(error);
        }
    }

    /// Interpret the value as an absolute pointer.
    #[must_use]
    pub fn pointer(mut self) -> Self {
        let conflicting = self
            .address
            .is_some_and(|mode| mode != AddressMode::Pointer);
        self.record_conflict(conflicting, PayloadError::ConflictingAddressModes);
        self.address = Some(AddressMode::Pointer);
        self
    }

    /// Interpret the value as an offset from the payload base.
    #[must_use]
    pub fn offset_from_base(mut self) -> Self {
        let conflicting = self
            .address
            .is_some_and(|mode| mode != AddressMode::OffsetFromBase);
        self.record_conflict(conflicting, PayloadError::ConflictingAddressModes);
        self.address = Some(AddressMode::OffsetFromBase);
        self
    }

    /// Interpret the value as an offset from this entry.
    #[must_use]
    pub fn offset_from_here(mut self) -> Self {
        let conflicting = self
            .address
            .is_some_and(|mode| mode != AddressMode::OffsetFromHere);
        self.record_conflict(conflicting, PayloadError::ConflictingAddressModes);
        self.address = Some(AddressMode::OffsetFromHere);
        self
    }

    /// Interpret the entry as a fixed-size array.
    #[must_use]
    pub fn array_fixed_size(mut self) -> Self {
        let conflicting = self.array.is_some_and(|mode| mode != ArrayMode::FixedSize);
        self.record_conflict(conflicting, PayloadError::ConflictingArrayModes);
        self.array = Some(ArrayMode::FixedSize);
        self
    }

    /// Interpret the entry as a zero-terminated array.
    #[must_use]
    pub fn array_zero_terminated(mut self) -> Self {
        let conflicting = self
            .array
            .is_some_and(|mode| mode != ArrayMode::ZeroTerminated);
        self.record_conflict(conflicting, PayloadError::ConflictingArrayModes);
        self.array = Some(ArrayMode::ZeroTerminated);
        self
    }

    /// Take array dimensions from another entry in the same payload.
    #[must_use]
    pub fn array_length_index(mut self) -> Self {
        let conflicting = self
            .array
            .is_some_and(|mode| mode != ArrayMode::LengthIndex);
        self.record_conflict(conflicting, PayloadError::ConflictingArrayModes);
        self.array = Some(ArrayMode::LengthIndex);
        self
    }

    /// Take array dimensions from another payload on the same event.
    #[must_use]
    pub fn array_length_payload_index(mut self) -> Self {
        let conflicting = self
            .array
            .is_some_and(|mode| mode != ArrayMode::LengthPayloadIndex);
        self.record_conflict(conflicting, PayloadError::ConflictingArrayModes);
        self.array = Some(ArrayMode::LengthPayloadIndex);
        self
    }

    /// Request deep copying of pointed-to data.
    #[must_use]
    pub const fn deep_copy(mut self) -> Self {
        self.deep_copy = true;
        self
    }

    /// Hide the entry in visualizations.
    #[must_use]
    pub const fn hide(mut self) -> Self {
        self.hidden = true;
        self
    }

    /// Mark the entry as a timestamp event attribute.
    #[must_use]
    pub fn timestamp(mut self) -> Self {
        let conflicting = self
            .event_attribute
            .is_some_and(|value| value != EventAttribute::Timestamp);
        self.record_conflict(conflicting, PayloadError::ConflictingEventAttributes);
        self.event_attribute = Some(EventAttribute::Timestamp);
        self
    }

    /// Mark the entry as the beginning of a range.
    #[must_use]
    pub fn range_begin(mut self) -> Self {
        let conflicting = self
            .event_role
            .is_some_and(|value| value != EventRole::RangeBegin);
        self.record_conflict(conflicting, PayloadError::ConflictingEventRoles);
        self.event_role = Some(EventRole::RangeBegin);
        self
    }

    /// Mark the entry as the end of a range.
    #[must_use]
    pub fn range_end(mut self) -> Self {
        let conflicting = self
            .event_role
            .is_some_and(|value| value != EventRole::RangeEnd);
        self.record_conflict(conflicting, PayloadError::ConflictingEventRoles);
        self.event_role = Some(EventRole::RangeEnd);
        self
    }

    /// Mark the entry as an instantaneous event.
    #[must_use]
    pub fn mark(mut self) -> Self {
        let conflicting = self
            .event_role
            .is_some_and(|value| value != EventRole::Mark);
        self.record_conflict(conflicting, PayloadError::ConflictingEventRoles);
        self.event_role = Some(EventRole::Mark);
        self
    }

    /// Mark the entry as a counter value.
    #[must_use]
    pub fn counter(mut self) -> Self {
        let conflicting = self
            .event_role
            .is_some_and(|value| value != EventRole::Counter);
        self.record_conflict(conflicting, PayloadError::ConflictingEventRoles);
        self.event_role = Some(EventRole::Counter);
        self
    }

    fn bits(self) -> Result<u64, PayloadError> {
        if let Some(error) = self.conflict {
            return Err(error);
        }

        let mut bits = 0_u64;
        bits |= match self.address {
            None => 0,
            Some(AddressMode::Pointer) => nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_POINTER as u64,
            Some(AddressMode::OffsetFromBase) => {
                nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_OFFSET_FROM_BASE as u64
            }
            Some(AddressMode::OffsetFromHere) => {
                nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_OFFSET_FROM_HERE as u64
            }
        };
        bits |= match self.array {
            None => 0,
            Some(ArrayMode::FixedSize) => {
                nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE as u64
            }
            Some(ArrayMode::ZeroTerminated) => {
                nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED as u64
            }
            Some(ArrayMode::LengthIndex) => {
                nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX as u64
            }
            Some(ArrayMode::LengthPayloadIndex) => {
                nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_PAYLOAD_INDEX as u64
            }
        };
        bits |= match self.event_attribute {
            None => 0,
            Some(EventAttribute::Message) => {
                nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE as u64
            }
            Some(EventAttribute::Timestamp) => {
                nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP as u64
            }
        };
        bits |= match self.event_role {
            None => 0,
            Some(EventRole::RangeBegin) => {
                nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN as u64
            }
            Some(EventRole::RangeEnd) => nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_RANGE_END as u64,
            Some(EventRole::Mark) => nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_MARK as u64,
            Some(EventRole::Counter) => nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_COUNTER as u64,
        };
        if self.deep_copy {
            bits |= nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_DEEP_COPY as u64;
        }
        if self.hidden {
            bits |= nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_HIDE as u64;
        }
        Ok(bits)
    }
}

/// One entry in an owned payload schema.
#[derive(Debug, Clone)]
pub struct PayloadSchemaEntry {
    flags: u64,
    entry_type: PayloadEntryType,
    name: Option<CString>,
    description: Option<CString>,
    array_or_union_detail: u64,
    offset: u64,
    semantics: Option<SemanticChain>,
}

impl PayloadSchemaEntry {
    /// Start building an entry with the given type.
    #[must_use]
    pub fn builder(entry_type: PayloadEntryType) -> PayloadSchemaEntryBuilder {
        PayloadSchemaEntryBuilder {
            flags: PayloadEntryFlag::new(),
            entry_type,
            name: None,
            description: None,
            array_or_union_detail: 0,
            offset: 0,
            semantics: None,
        }
    }
}

/// Builder for [`PayloadSchemaEntry`].
#[derive(Debug, Clone)]
pub struct PayloadSchemaEntryBuilder {
    flags: PayloadEntryFlag,
    entry_type: PayloadEntryType,
    name: Option<CString>,
    description: Option<CString>,
    array_or_union_detail: u64,
    offset: u64,
    semantics: Option<SemanticChain>,
}

impl PayloadSchemaEntryBuilder {
    /// Set validated entry flags.
    #[must_use]
    pub const fn flags(mut self, flags: PayloadEntryFlag) -> Self {
        self.flags = flags;
        self
    }

    /// Set the entry name.
    #[must_use]
    pub fn name(mut self, name: &CStr) -> Self {
        self.name = Some(name.to_owned());
        self
    }

    /// Set the entry description.
    #[must_use]
    pub fn description(mut self, description: &CStr) -> Self {
        self.description = Some(description.to_owned());
        self
    }

    /// Set the fixed array length, length source, or union selector detail.
    #[must_use]
    pub const fn array_or_union_detail(mut self, detail: u64) -> Self {
        self.array_or_union_detail = detail;
        self
    }

    /// Set the byte offset from the payload base.
    #[must_use]
    pub const fn offset(mut self, offset: usize) -> Self {
        self.offset = offset as u64;
        self
    }

    /// Attach semantic metadata to this entry.
    #[must_use]
    pub fn semantics(mut self, semantics: SemanticChain) -> Self {
        self.semantics = Some(semantics);
        self
    }

    /// Validate and build the entry.
    ///
    /// # Errors
    /// Returns a conflict error when mutually exclusive flag modes were selected.
    pub fn build(self) -> Result<PayloadSchemaEntry, PayloadError> {
        Ok(PayloadSchemaEntry {
            flags: self.flags.bits()?,
            entry_type: self.entry_type,
            name: self.name,
            description: self.description,
            array_or_union_detail: self.array_or_union_detail,
            offset: self.offset,
            semantics: self.semantics,
        })
    }
}

/// Payload schema layout kind.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PayloadSchemaKind {
    /// Fixed-size C-compatible structure.
    Static,
    /// Sequentially decoded variable-size structure.
    Dynamic,
    /// Union selected by an external entry.
    Union,
    /// Union carrying its own selector.
    UnionWithInternalSelector,
}

impl PayloadSchemaKind {
    const fn raw(self) -> u64 {
        match self {
            Self::Static => nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_TYPE_STATIC as u64,
            Self::Dynamic => nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC as u64,
            Self::Union => nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_TYPE_UNION as u64,
            Self::UnionWithInternalSelector => {
                nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_TYPE_UNION_WITH_INTERNAL_SELECTOR as u64
            }
        }
    }
}

/// Additional payload schema properties.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct PayloadSchemaFlags(u64);

impl PayloadSchemaFlags {
    /// Start with no schema flags.
    #[must_use]
    pub const fn new() -> Self {
        Self(0)
    }

    /// Allow entries requiring deep copies.
    #[must_use]
    pub const fn deep_copy(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_FLAG_DEEP_COPY as u64;
        self
    }

    /// Allow other payloads to reference this payload.
    #[must_use]
    pub const fn referenced(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_FLAG_REFERENCED as u64;
        self
    }

    /// Describe a counter group.
    #[must_use]
    pub const fn counter_group(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_FLAG_COUNTER_GROUP as u64;
        self
    }

    /// Describe a push/pop range.
    #[must_use]
    pub const fn range_push_pop(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSHPOP as u64;
        self
    }

    /// Describe a start/end range.
    #[must_use]
    pub const fn range_start_end(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_STARTEND as u64;
        self
    }

    /// Describe a mark.
    #[must_use]
    pub const fn mark(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_FLAG_MARK as u64;
        self
    }
}

/// Owned payload schema ready for domain registration.
#[derive(Debug, Clone)]
pub struct PayloadSchema {
    name: Option<CString>,
    kind: PayloadSchemaKind,
    flags: PayloadSchemaFlags,
    entries: Vec<PayloadSchemaEntry>,
    static_size: Option<usize>,
    pack_align: Option<usize>,
    schema_id: Option<SchemaId>,
}

impl PayloadSchema {
    /// Start a schema builder.
    #[must_use]
    pub fn builder(kind: PayloadSchemaKind) -> PayloadSchemaBuilder {
        PayloadSchemaBuilder {
            name: None,
            kind,
            flags: PayloadSchemaFlags::new(),
            entries: Vec::new(),
            static_size: None,
            pack_align: None,
            schema_id: None,
        }
    }

    pub(crate) fn into_counter_group(mut self) -> Self {
        self.flags = self.flags.counter_group();
        self
    }

    fn register(&self, domain: nvtx_sys::DomainHandle) -> SchemaId {
        let semantic_chains: Vec<_> = self
            .entries
            .iter()
            .map(|entry| entry.semantics.as_ref().map(SemanticChain::encode))
            .collect();
        let entries: Vec<nvtx_sys::payload::PayloadSchemaEntry> = self
            .entries
            .iter()
            .zip(&semantic_chains)
            .map(
                |(entry, semantic_chain)| nvtx_sys::payload::PayloadSchemaEntry {
                    flags: entry.flags,
                    type_: entry.entry_type.get(),
                    name: entry
                        .name
                        .as_ref()
                        .map_or(core::ptr::null(), |value| value.as_ptr()),
                    description: entry
                        .description
                        .as_ref()
                        .map_or(core::ptr::null(), |value| value.as_ptr()),
                    arrayOrUnionDetail: entry.array_or_union_detail,
                    offset: entry.offset,
                    semantics: semantic_chain
                        .as_ref()
                        .map_or(core::ptr::null(), EncodedSemanticChain::head),
                    reserved: core::ptr::null(),
                },
            )
            .collect();

        let mut field_mask = (nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE
            | nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES
            | nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES)
            as u64;
        if self.name.is_some() {
            field_mask |= nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NAME as u64;
        }
        if self.flags.0 != 0 {
            field_mask |= nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_FLAGS as u64;
        }
        if self.static_size.is_some() {
            field_mask |= nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE as u64;
        }
        if self.pack_align.is_some() {
            field_mask |= nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ALIGNMENT as u64;
        }
        if self.schema_id.is_some() {
            field_mask |= nvtx_sys::ffi::NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_SCHEMA_ID as u64;
        }

        let attributes = nvtx_sys::payload::PayloadSchemaAttributes {
            fieldMask: field_mask,
            name: self
                .name
                .as_ref()
                .map_or(core::ptr::null(), |value| value.as_ptr()),
            type_: self.kind.raw(),
            flags: self.flags.0,
            entries: entries.as_ptr(),
            numEntries: entries.len(),
            payloadStaticSize: self.static_size.unwrap_or(0),
            packAlign: self.pack_align.unwrap_or(0),
            schemaId: self.schema_id.unwrap_or(SchemaId::NONE).get(),
            extension: core::ptr::null_mut(),
        };
        // SAFETY: `self` owns all strings and the local `entries` vector remains
        // alive for the complete registration call.
        SchemaId::from_raw(unsafe { nvtx_sys::payload::schema_register(domain, &attributes) })
    }
}

/// Builder for [`PayloadSchema`].
#[derive(Debug, Clone)]
pub struct PayloadSchemaBuilder {
    name: Option<CString>,
    kind: PayloadSchemaKind,
    flags: PayloadSchemaFlags,
    entries: Vec<PayloadSchemaEntry>,
    static_size: Option<usize>,
    pack_align: Option<usize>,
    schema_id: Option<SchemaId>,
}

impl PayloadSchemaBuilder {
    /// Start a static schema for `T`.
    #[must_use]
    pub fn static_layout<T>(name: &CStr) -> Self {
        Self {
            name: Some(name.to_owned()),
            kind: PayloadSchemaKind::Static,
            flags: PayloadSchemaFlags::new(),
            entries: Vec::new(),
            static_size: Some(core::mem::size_of::<T>()),
            pack_align: None,
            schema_id: None,
        }
    }

    /// Set the schema name.
    #[must_use]
    pub fn name(mut self, name: &CStr) -> Self {
        self.name = Some(name.to_owned());
        self
    }

    /// Set schema flags.
    #[must_use]
    pub const fn flags(mut self, flags: PayloadSchemaFlags) -> Self {
        self.flags = flags;
        self
    }

    /// Append one schema entry.
    #[must_use]
    pub fn entry(mut self, entry: PayloadSchemaEntry) -> Self {
        self.entries.push(entry);
        self
    }

    /// Set the static payload size.
    #[must_use]
    pub const fn static_size(mut self, size: usize) -> Self {
        self.static_size = Some(size);
        self
    }

    /// Set the structure packing alignment.
    #[must_use]
    pub const fn pack_align(mut self, alignment: usize) -> Self {
        self.pack_align = Some(alignment);
        self
    }

    /// Request a static schema ID.
    ///
    /// # Errors
    /// Returns [`PayloadError::InvalidStaticId`] when `id` is outside the
    /// user-defined static-ID range.
    pub fn schema_id(mut self, id: SchemaId) -> Result<Self, PayloadError> {
        validate_static_id(id.get())?;
        self.schema_id = Some(id);
        Ok(self)
    }

    /// Validate and build the schema.
    ///
    /// # Errors
    /// Returns [`PayloadError::EmptySchema`] when no entries were added.
    pub fn build(self) -> Result<PayloadSchema, PayloadError> {
        if self.entries.is_empty() {
            return Err(PayloadError::EmptySchema);
        }
        Ok(PayloadSchema {
            name: self.name,
            kind: self.kind,
            flags: self.flags,
            entries: self.entries,
            static_size: self.static_size,
            pack_align: self.pack_align,
            schema_id: self.schema_id,
        })
    }
}

/// One value in a registered payload enumeration.
#[derive(Debug, Clone)]
pub struct PayloadEnumEntry {
    name: CString,
    value: u64,
    is_flag: bool,
}

impl PayloadEnumEntry {
    /// Create one named enumeration value.
    #[must_use]
    pub fn new(name: &CStr, value: u64) -> Self {
        Self {
            name: name.to_owned(),
            value,
            is_flag: false,
        }
    }

    /// Mark this value as a bit flag.
    #[must_use]
    pub const fn flag(mut self) -> Self {
        self.is_flag = true;
        self
    }
}

/// Owned payload enumeration ready for registration.
#[derive(Debug, Clone)]
pub struct PayloadEnum {
    name: Option<CString>,
    entries: Vec<PayloadEnumEntry>,
    size: usize,
    enum_id: Option<EnumId>,
}

impl PayloadEnum {
    /// Start building an enumeration with the given storage size.
    #[must_use]
    pub fn builder(size: usize) -> PayloadEnumBuilder {
        PayloadEnumBuilder {
            name: None,
            entries: Vec::new(),
            size,
            enum_id: None,
        }
    }

    fn register(&self, domain: nvtx_sys::DomainHandle) -> EnumId {
        let entries: Vec<nvtx_sys::payload::PayloadEnumEntry> = self
            .entries
            .iter()
            .map(|entry| nvtx_sys::payload::PayloadEnumEntry {
                name: entry.name.as_ptr(),
                value: entry.value,
                isFlag: i8::from(entry.is_flag),
            })
            .collect();
        let mut field_mask = (nvtx_sys::ffi::NVTX_PAYLOAD_ENUM_ATTR_FIELD_ENTRIES
            | nvtx_sys::ffi::NVTX_PAYLOAD_ENUM_ATTR_FIELD_NUM_ENTRIES
            | nvtx_sys::ffi::NVTX_PAYLOAD_ENUM_ATTR_FIELD_SIZE) as u64;
        if self.name.is_some() {
            field_mask |= nvtx_sys::ffi::NVTX_PAYLOAD_ENUM_ATTR_FIELD_NAME as u64;
        }
        if self.enum_id.is_some() {
            field_mask |= nvtx_sys::ffi::NVTX_PAYLOAD_ENUM_ATTR_FIELD_SCHEMA_ID as u64;
        }
        let attributes = nvtx_sys::payload::PayloadEnumAttributes {
            fieldMask: field_mask,
            name: self
                .name
                .as_ref()
                .map_or(core::ptr::null(), |value| value.as_ptr()),
            entries: entries.as_ptr(),
            numEntries: entries.len(),
            sizeOfEnum: self.size,
            schemaId: self.enum_id.unwrap_or(EnumId::NONE).get(),
            extension: core::ptr::null_mut(),
        };
        // SAFETY: All pointers refer to owned data alive for the call duration.
        EnumId::from_raw(unsafe { nvtx_sys::payload::enum_register(domain, &attributes) })
    }
}

/// Builder for [`PayloadEnum`].
#[derive(Debug, Clone)]
pub struct PayloadEnumBuilder {
    name: Option<CString>,
    entries: Vec<PayloadEnumEntry>,
    size: usize,
    enum_id: Option<EnumId>,
}

impl PayloadEnumBuilder {
    /// Set the enumeration name.
    #[must_use]
    pub fn name(mut self, name: &CStr) -> Self {
        self.name = Some(name.to_owned());
        self
    }

    /// Append an enumeration entry.
    #[must_use]
    pub fn entry(mut self, entry: PayloadEnumEntry) -> Self {
        self.entries.push(entry);
        self
    }

    /// Request a static enumeration ID.
    ///
    /// # Errors
    /// Returns [`PayloadError::InvalidStaticId`] when `id` is outside the
    /// user-defined static-ID range.
    pub fn enum_id(mut self, id: EnumId) -> Result<Self, PayloadError> {
        validate_static_id(id.get())?;
        self.enum_id = Some(id);
        Ok(self)
    }

    /// Validate and build the enumeration.
    ///
    /// # Errors
    /// Returns [`PayloadError::EmptyEnumeration`] when no values were added.
    pub fn build(self) -> Result<PayloadEnum, PayloadError> {
        if self.entries.is_empty() {
            return Err(PayloadError::EmptyEnumeration);
        }
        Ok(PayloadEnum {
            name: self.name,
            entries: self.entries,
            size: self.size,
            enum_id: self.enum_id,
        })
    }
}

/// Borrowed binary payload with schema information.
#[derive(Debug, Clone, Copy)]
pub struct PayloadData<'a> {
    raw: nvtx_sys::payload::PayloadData,
    lifetime: PhantomData<&'a [u8]>,
}

impl<'a> PayloadData<'a> {
    /// Describe a non-empty byte slice with a schema.
    ///
    /// # Errors
    /// Returns [`PayloadError::EmptyPayload`] when `bytes` is empty.
    pub fn from_bytes(schema: SchemaId, bytes: &'a [u8]) -> Result<Self, PayloadError> {
        if bytes.is_empty() {
            return Err(PayloadError::EmptyPayload);
        }
        Ok(Self {
            raw: nvtx_sys::payload::PayloadData {
                schemaId: schema.get(),
                size: bytes.len(),
                payload: bytes.as_ptr().cast(),
            },
            lifetime: PhantomData,
        })
    }

    /// Describe a typed payload value.
    #[must_use]
    pub fn from_value<T: PayloadSchemaType>(schema: SchemaId, value: &'a T) -> Self {
        Self {
            raw: nvtx_sys::payload::PayloadData {
                schemaId: schema.get(),
                size: core::mem::size_of::<T>(),
                payload: core::ptr::from_ref(value).cast::<c_void>(),
            },
            lifetime: PhantomData,
        }
    }

    /// Describe bytes using a predefined payload entry type.
    ///
    /// # Errors
    /// Returns [`PayloadError::EmptyPayload`] when `bytes` is empty.
    pub fn predefined(entry_type: PayloadEntryType, bytes: &'a [u8]) -> Result<Self, PayloadError> {
        Self::from_bytes(SchemaId::from_raw(entry_type.get()), bytes)
    }

    pub(crate) const fn raw(self) -> nvtx_sys::payload::PayloadData {
        self.raw
    }
}

/// Defines how a Rust type is registered as an NVTX payload schema.
///
/// # Safety
/// Implementors must guarantee that the returned schema exactly describes the
/// in-memory representation of `Self`, including field offsets, size,
/// alignment, and initialized padding requirements.
pub unsafe trait PayloadSchemaType: Sized + 'static {
    /// Construct the schema describing this type.
    ///
    /// # Errors
    /// Returns a payload validation error when the generated or manual schema
    /// is not valid.
    fn payload_schema() -> Result<PayloadSchema, PayloadError>;
}

impl Domain {
    /// Register an owned payload schema in this domain.
    #[must_use]
    pub fn register_payload_schema(&self, schema: &PayloadSchema) -> SchemaId {
        schema.register(self.raw_handle())
    }

    /// Register an owned payload enumeration in this domain.
    #[must_use]
    pub fn register_payload_enum(&self, enumeration: &PayloadEnum) -> EnumId {
        enumeration.register(self.raw_handle())
    }

    /// Get or register the schema associated with `T` in this domain.
    ///
    /// # Errors
    /// Returns the error produced while constructing `T`'s schema.
    pub fn payload_schema<T: PayloadSchemaType>(&self) -> Result<SchemaId, PayloadError> {
        let key = String::from(core::any::type_name::<T>());
        self.cached_schema_id(key, || {
            let schema = T::payload_schema()?;
            Ok(schema.register(self.raw_handle()).get())
        })
        .map(SchemaId::from_raw)
    }

    pub(crate) fn counter_payload_schema<T: PayloadSchemaType>(
        &self,
    ) -> Result<SchemaId, PayloadError> {
        let mut key = String::from("counter:");
        key.push_str(core::any::type_name::<T>());
        self.cached_schema_id(key, || {
            let schema = T::payload_schema()?.into_counter_group();
            Ok(schema.register(self.raw_handle()).get())
        })
        .map(SchemaId::from_raw)
    }
}

pub(crate) fn raw_payloads(payloads: &[PayloadData<'_>]) -> Vec<nvtx_sys::payload::PayloadData> {
    payloads.iter().copied().map(PayloadData::raw).collect()
}

fn validate_static_id(id: u64) -> Result<(), PayloadError> {
    if (nvtx_sys::payload::PAYLOAD_SCHEMA_ID_STATIC_START
        ..nvtx_sys::payload::PAYLOAD_SCHEMA_ID_DYNAMIC_START)
        .contains(&id)
    {
        Ok(())
    } else {
        Err(PayloadError::InvalidStaticId)
    }
}

/// Implement [`PayloadSchemaType`] for a `#[repr(C)]` structure.
///
/// Each field is named in the schema with its Rust identifier. Complex arrays,
/// unions, semantics, and custom labels should use the manual builders.
///
/// # Safety
/// The target structure must use a stable C-compatible representation and all
/// fields and padding passed to NVTX must be initialized.
#[macro_export]
macro_rules! payload_schema {
    (
        $type:ty,
        $schema_name:expr,
        { $($field:ident : $entry_type:expr),+ $(,)? }
    ) => {
        // SAFETY: The macro caller accepts the documented `#[repr(C)]` and
        // initialized-representation requirements.
        unsafe impl $crate::PayloadSchemaType for $type {
            fn payload_schema() -> ::core::result::Result<
                $crate::PayloadSchema,
                $crate::PayloadError,
            > {
                let mut builder =
                    $crate::PayloadSchemaBuilder::static_layout::<Self>($schema_name);
                $(
                    let field_name = {
                        // SAFETY: `concat!` appends exactly one trailing NUL to
                        // an identifier, which cannot itself contain NUL bytes.
                        unsafe {
                            ::core::ffi::CStr::from_bytes_with_nul_unchecked(
                                ::core::concat!(::core::stringify!($field), "\0").as_bytes(),
                            )
                        }
                    };
                    let entry = $crate::PayloadSchemaEntry::builder($entry_type)
                        .name(field_name)
                        .offset(::core::mem::offset_of!(Self, $field))
                        .build()?;
                    builder = builder.entry(entry);
                )+
                builder.build()
            }
        }
    };
}

#[cfg(all(test, feature = "std"))]
mod tests {
    use super::*;

    #[test]
    fn entry_flags_encode_independent_groups() {
        let flags = PayloadEntryFlag::event()
            .pointer()
            .array_zero_terminated()
            .deep_copy()
            .hide()
            .bits()
            .unwrap();
        assert_eq!(
            flags,
            (nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE
                | nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_POINTER
                | nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED
                | nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_DEEP_COPY
                | nvtx_sys::ffi::NVTX_PAYLOAD_ENTRY_FLAG_HIDE) as u64
        );
    }

    #[test]
    fn static_ids_are_range_checked() {
        assert!(PayloadSchema::builder(PayloadSchemaKind::Static)
            .schema_id(SchemaId::from_raw(7))
            .is_err());
        assert!(PayloadSchema::builder(PayloadSchemaKind::Static)
            .schema_id(SchemaId::from_raw(
                nvtx_sys::payload::PAYLOAD_SCHEMA_ID_STATIC_START,
            ))
            .is_ok());
    }
}
