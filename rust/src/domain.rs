// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use crate::{
    common::{
        event_attributes::GenericEventAttributesBuilder, CategoryEncodable, GenericEventArgument,
        GenericEventAttributes,
    },
    Color, NvtxError, Payload, Str, TypeValueEncodable,
};
#[cfg(all(feature = "alloc", not(feature = "std")))]
// `HashMap` depends on `std`'s hasher support. In `no_std + alloc`, `BTreeMap`
// keeps the caches available with O(log n) lookups instead of average O(1).
use alloc::collections::BTreeMap as StringMap;
use alloc::string::{String, ToString};
use core::{
    marker::PhantomData,
    sync::atomic::{AtomicU32, Ordering},
};
#[cfg(all(feature = "alloc", not(feature = "std")))]
// Domain caches use a spinlock in this tier; see the crate feature docs for
// the interrupt/RTOS priority inversion caveat.
use spin::Mutex;
#[cfg(feature = "std")]
use std::collections::HashMap as StringMap;
#[cfg(feature = "std")]
use std::sync::Mutex;

mod category;
pub use category::Category;

/// Convenience wrapper for all valid argument types to ranges and marks.
///
/// * Any string type will be translated to [`EventArgument::Message`].
/// * If [`EventArgument::Attributes`] is the active discriminator:
///   - If its [`EventAttributes`] only specifies a message, then message will be used.
///   - Otherwise, the existing [`EventAttributes`] will be used for the event.
pub type EventArgument<'a> = GenericEventArgument<Message<'a>, EventAttributes<'a>>;

/// All attributes that are associated with marks and ranges.
pub type EventAttributes<'a> = GenericEventAttributes<Category<'a>, Message<'a>>;

/// Builder to facilitate easier construction of [`EventAttributes`].
///
/// ```
/// let domain = nvtx::Domain::new(c"Domain");
/// let cat = domain.register_category(c"Category1");
///
/// let attr = domain.event_attributes_builder()
///                .category(cat)
///                .color([20, 192, 240])
///                .payload(3.141592)
///                .message(c"Hello")
///                .build();
/// ```
#[derive(Clone)]
pub struct EventAttributesBuilder<'a> {
    pub(super) domain: &'a Domain,
    pub(super) inner: GenericEventAttributesBuilder<Category<'a>, Message<'a>>,
}

impl<'a> EventAttributesBuilder<'a> {
    fn category_in_builder_domain(&self, category: Category<'a>) -> Category<'a> {
        if core::ptr::eq(category.domain(), self.domain) {
            category
        } else {
            Category::new(category.encode_id(), self.domain)
        }
    }

    fn resolve_registered_message(
        &self,
        message: impl Into<Message<'a>>,
    ) -> Result<RegisteredString<'a>, NvtxError> {
        // implementation optimization: always prefer registered strings
        let msg = match message.into() {
            Message::Ascii(s) => self.domain.register_string(s),
            Message::Unicode(s) => self.domain.register_string(s),
            Message::Registered(r) => r,
        };
        if !core::ptr::eq(msg.domain(), self.domain) {
            return Err(NvtxError::DomainMismatch);
        }
        Ok(msg)
    }

    /// Update the attribute's category.
    ///
    /// If `category` belongs to a different [`Domain`], this method preserves
    /// the category ID in this builder's domain. Use [`Self::try_category`] to
    /// receive an explicit error.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// let cat = domain.register_category(c"Category1");
    /// // ...
    /// let builder = domain.event_attributes_builder();
    /// // ...
    /// let builder = builder.category(cat);
    /// ```
    #[must_use]
    pub fn category(mut self, category: Category<'a>) -> EventAttributesBuilder<'a> {
        if core::ptr::eq(category.domain(), self.domain) {
            self.inner = self.inner.category(category);
        } else {
            debug_assert!(false, "{:?}", NvtxError::DomainMismatch);
            let category = self.category_in_builder_domain(category);
            self.inner = self.inner.category(category);
        }
        self
    }

    /// Fallible variant of [`Self::category`].
    ///
    /// # Errors
    ///
    /// Returns [`NvtxError::DomainMismatch`] when `category` belongs to a
    /// different domain.
    pub fn try_category(
        mut self,
        category: Category<'a>,
    ) -> Result<EventAttributesBuilder<'a>, NvtxError> {
        if !core::ptr::eq(category.domain(), self.domain) {
            return Err(NvtxError::DomainMismatch);
        }
        self.inner = self.inner.category(category);
        Ok(self)
    }

    /// Update the attribute's category. An assertion will be thrown if a Category is
    /// passed in whose domain is not the same as this builder.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// // ...
    /// let builder = domain.event_attributes_builder();
    /// // ...
    /// let builder = builder.category_name(c"Category2");
    /// ```
    #[must_use]
    pub fn category_name(mut self, name: impl Into<Str>) -> EventAttributesBuilder<'a> {
        let category = self.domain.register_category(name);
        self.inner = self.inner.category(category);
        self
    }

    /// Update the builder's held [`Color`]. See [`Color`] for valid conversions.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// let builder = domain.event_attributes_builder();
    /// // ...
    /// let builder = builder.color([255, 255, 255]);
    /// ```
    #[must_use]
    pub fn color(mut self, color: impl Into<Color>) -> EventAttributesBuilder<'a> {
        self.inner = self.inner.color(color);
        self
    }

    /// Update the builder's held [`Payload`]. See [`Payload`] for valid conversions.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// let builder = domain.event_attributes_builder();
    /// // ...
    /// let builder = builder.payload(3.1415926535);
    /// ```
    #[must_use]
    pub fn payload(mut self, payload: impl Into<Payload>) -> EventAttributesBuilder<'a> {
        self.inner = self.inner.payload(payload);
        self
    }

    /// Update the attribute's message.
    ///
    /// If `message` resolves to a registered string from a different [`Domain`],
    /// this method clears the message field. Use [`Self::try_message`] for
    /// explicit error handling.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// let builder = domain.event_attributes_builder();
    /// // ...
    /// let builder = builder.message(c"test");
    /// ```
    #[must_use]
    pub fn message(mut self, message: impl Into<Message<'a>>) -> EventAttributesBuilder<'a> {
        match self.resolve_registered_message(message) {
            Ok(msg) => {
                self.inner = self.inner.message(Message::Registered(msg));
            }
            Err(error) => {
                debug_assert!(false, "{error}");
                self.inner = self.inner.clear_message();
            }
        }
        self
    }

    /// Fallible variant of [`Self::message`].
    ///
    /// # Errors
    ///
    /// Returns [`NvtxError::DomainMismatch`] when `message` resolves to a
    /// registered string owned by a different domain.
    pub fn try_message(
        mut self,
        message: impl Into<Message<'a>>,
    ) -> Result<EventAttributesBuilder<'a>, NvtxError> {
        let msg = self.resolve_registered_message(message)?;
        self.inner = self.inner.message(Message::Registered(msg));
        Ok(self)
    }

    /// Construct an [`EventAttributes`] from the builder's held state.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// let cat = domain.register_category(c"Category1");
    /// let attr = domain.event_attributes_builder()
    ///                 .message(c"Example Range")
    ///                 .color([224, 192, 128])
    ///                 .category(cat)
    ///                 .payload(1234567)
    ///                 .build();
    /// ```
    #[must_use]
    pub fn build(self) -> EventAttributes<'a> {
        self.inner.build()
    }
}

mod identifier;
#[cfg(feature = "cuda")]
pub use self::identifier::CudaIdentifier;
#[cfg(feature = "cuda_runtime")]
pub use self::identifier::CudaRuntimeIdentifier;
#[cfg(target_family = "unix")]
pub use self::identifier::PThreadIdentifier;
pub use identifier::{GenericIdentifier, Identifier};

mod ranges;
pub use ranges::{LocalRange, Range};

mod registered_string;
pub use registered_string::RegisteredString;

/// Represents a message for use within events and ranges
///
/// * [`Message::Ascii`] is the discriminator for ASCII C strings
/// * [`Message::Unicode`] is the discriminator for Rust strings and wide C strings
/// * [`Message::Registered`] is the discriminator for NVTX domain-registered strings
pub type Message<'a> = crate::common::GenericMessage<RegisteredString<'a>>;

mod resource;
pub use resource::Resource;

/// user-defined synchronization objects
pub mod sync;

/// Represents a domain for high-level grouping within Nsight profilers.
#[derive(Debug)]
pub struct Domain {
    handle: nvtx_sys::DomainHandle,
    registered_strings: AtomicU32,
    registered_categories: AtomicU32,
    strings: Mutex<StringMap<String, (nvtx_sys::StringHandle, u32)>>,
    categories: Mutex<StringMap<String, u32>>,
}

#[cfg(feature = "std")]
fn lock_unpoison<T>(mutex: &Mutex<T>) -> std::sync::MutexGuard<'_, T> {
    mutex
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner)
}

#[cfg(all(feature = "alloc", not(feature = "std")))]
fn lock_unpoison<T>(mutex: &Mutex<T>) -> spin::MutexGuard<'_, T> {
    mutex.lock()
}

impl Domain {
    /// Register a NVTX domain.
    ///
    /// See [`Str`] for valid conversions.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// ```
    pub fn new(name: impl Into<Str>) -> Self {
        Domain {
            handle: match name.into() {
                Str::Ascii(s) => nvtx_sys::domain_create_ascii(&s),
                Str::Unicode(s) => nvtx_sys::domain_create_unicode(&s),
            },
            registered_strings: AtomicU32::new(0),
            registered_categories: AtomicU32::new(0),
            strings: Mutex::new(StringMap::default()),
            categories: Mutex::new(StringMap::default()),
        }
    }

    /// Gets a new builder instance for event attributes in the current domain.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// // ...
    /// let builder = domain.event_attributes_builder();
    /// ```
    pub fn event_attributes_builder(&self) -> EventAttributesBuilder<'_> {
        EventAttributesBuilder {
            domain: self,
            inner: GenericEventAttributesBuilder::default(),
        }
    }

    /// Registers an immutable string within the current domain.
    ///
    /// Returns a handle to the immutable string registered to NVTX.
    ///
    /// See [`Str`] for valid conversions.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// // ...
    /// let my_str = domain.register_string(c"My immutable string");
    /// ```
    pub fn register_string(&self, string: impl Into<Str>) -> RegisteredString<'_> {
        let into_string: Str = string.into();
        let owned_string = match &into_string {
            Str::Ascii(s) => {
                let mut key = String::from("Ascii:");
                key.push_str(&s.to_string_lossy());
                key
            }
            Str::Unicode(s) => {
                let mut key = String::from("Unicode:");
                key.push_str(&s.to_string_lossy());
                key
            }
        };
        let (handle, uid) = *lock_unpoison(&self.strings)
            .entry(owned_string)
            .or_insert_with(|| {
                let id = 1 + self.registered_strings.fetch_add(1, Ordering::SeqCst);
                let handle = match into_string {
                    Str::Ascii(s) => nvtx_sys::domain_register_string_ascii(self.handle, &s),
                    Str::Unicode(s) => nvtx_sys::domain_register_string_unicode(self.handle, &s),
                };
                (handle, id)
            });
        RegisteredString::new(handle, uid, self)
    }

    /// Register many immutable strings within the current domain.
    ///
    /// Returns an array of handles to the immutable strings registered to NVTX.
    ///
    /// See [`Str`] for valid conversions.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// // ...
    /// let [a, b, c] = domain.register_strings([c"A", c"B", c"C"]);
    /// ```
    pub fn register_strings<const N: usize>(
        &self,
        strings: [impl Into<Str>; N],
    ) -> [RegisteredString<'_>; N] {
        strings.map(|string| self.register_string(string))
    }

    /// Register a new category within the domain.
    ///
    /// Returns a handle to the category registered to NVTX.
    ///
    /// See [`Str`] for valid conversions.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// // ...
    /// let cat = domain.register_category(c"Category");
    /// ```
    pub fn register_category(&self, name: impl Into<Str>) -> Category<'_> {
        let into_name: Str = name.into();
        let owned_name = match &into_name {
            Str::Ascii(s) => s.to_string_lossy().to_string(),
            Str::Unicode(s) => s.to_string_lossy().clone(),
        };

        let id = *lock_unpoison(&self.categories)
            .entry(owned_name)
            .or_insert_with(|| {
                let id = 1 + self.registered_categories.fetch_add(1, Ordering::SeqCst);
                match into_name {
                    Str::Ascii(s) => nvtx_sys::domain_name_category_ascii(self.handle, id, &s),
                    Str::Unicode(s) => nvtx_sys::domain_name_category_unicode(self.handle, id, &s),
                }
                id
            });
        Category::new(id, self)
    }

    /// Register new categories within the domain.
    ///
    /// Returns an array of handles to the categories registered to NVTX.
    ///
    /// See [`Str`] for valid conversions.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// // ...
    /// let [cat_a, cat_b] = domain.register_categories([c"CatA", c"CatB"]);
    /// ```
    pub fn register_categories<const N: usize>(
        &self,
        names: [impl Into<Str>; N],
    ) -> [Category<'_>; N] {
        names.map(|name| self.register_category(name))
    }

    fn validate_event_argument<'a>(&'a self, arg: &EventArgument<'a>) -> Result<(), NvtxError> {
        match arg {
            EventArgument::Attributes(attr) => {
                if let Some(category) = &attr.category {
                    if !core::ptr::eq(category.domain(), self) {
                        return Err(NvtxError::DomainMismatch);
                    }
                }
                if let Some(Message::Registered(reg_str)) = &attr.message {
                    if !core::ptr::eq(reg_str.domain(), self) {
                        return Err(NvtxError::DomainMismatch);
                    }
                }
            }
            EventArgument::Message(Message::Registered(reg_str)) => {
                if !core::ptr::eq(reg_str.domain(), self) {
                    return Err(NvtxError::DomainMismatch);
                }
            }
            EventArgument::Message(_) => {}
        }
        Ok(())
    }

    fn strip_foreign_domain_values<'a>(&'a self, arg: EventArgument<'a>) -> EventArgument<'a> {
        match arg {
            EventArgument::Attributes(mut attr) => {
                if attr
                    .category
                    .as_ref()
                    .is_some_and(|category| !core::ptr::eq(category.domain(), self))
                {
                    attr.category = None;
                }
                if attr.message.as_ref().is_some_and(|message| {
                    matches!(message, Message::Registered(reg_str) if !core::ptr::eq(reg_str.domain(), self))
                }) {
                    attr.message = None;
                }
                EventArgument::Attributes(attr)
            }
            EventArgument::Message(Message::Registered(reg_str))
                if !core::ptr::eq(reg_str.domain(), self) =>
            {
                EventArgument::Attributes(EventAttributes {
                    category: None,
                    color: None,
                    message: None,
                    payload: None,
                })
            }
            arg @ EventArgument::Message(_) => arg,
        }
    }

    fn mark_event_argument<'a>(&'a self, event_arg: EventArgument<'a>) {
        match event_arg {
            EventArgument::Attributes(attr) => {
                let encoded = attr.encode();
                nvtx_sys::domain_mark_ex(self.handle, &encoded);
            }
            EventArgument::Message(m) => {
                let attr = EventAttributes {
                    category: None,
                    color: None,
                    message: Some(m),
                    payload: None,
                };
                let encoded = attr.encode();
                nvtx_sys::domain_mark_ex(self.handle, &encoded);
            }
        }
    }

    /// Marks an instantaneous event in the application belonging to a domain.
    ///
    /// A marker can contain a text message or specify information using the event
    /// attributes structure. These attributes include a text message, color, category,
    /// and a payload. Each of the attributes is optional.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// // ...
    /// domain.mark(c"Sample mark");
    ///
    /// domain.mark(c"Another example");
    ///
    /// domain.mark(
    ///   domain.event_attributes_builder()
    ///     .message(c"Interesting example")
    ///     .color([255, 0, 0])
    ///     .build());
    ///
    /// let reg_str = domain.register_string(c"Registered String");
    /// domain.mark(reg_str);
    /// ```
    ///
    pub fn mark<'a>(&'a self, arg: impl Into<EventArgument<'a>>) {
        let event_arg = arg.into();
        match self.validate_event_argument(&event_arg) {
            Ok(()) => self.mark_event_argument(event_arg),
            Err(error) => {
                debug_assert!(false, "{error}");
                self.mark_event_argument(self.strip_foreign_domain_values(event_arg));
            }
        }
    }

    /// Fallible variant of [`Domain::mark`].
    ///
    /// # Errors
    ///
    /// Returns [`NvtxError::DomainMismatch`] when `arg` references domain-owned
    /// values from a different domain.
    pub fn try_mark<'a>(&'a self, arg: impl Into<EventArgument<'a>>) -> Result<(), NvtxError> {
        let event_arg = arg.into();
        self.validate_event_argument(&event_arg)?;
        self.mark_event_argument(event_arg);
        Ok(())
    }

    /// Create an RAII-friendly, domain-owned range type which (1) cannot be moved across
    /// thread boundaries and (2) automatically ended when dropped.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    ///
    /// // creation from Rust string
    /// let range = domain.local_range(nvtx::Str::from_str_lossy("simple name"));
    ///
    /// // creation from C string (since 1.77)
    /// let range = domain.local_range(c"simple name");
    ///
    /// // creation from EventAttributes
    /// let attr = domain
    ///     .event_attributes_builder()
    ///     .payload(1)
    ///     .message(c"complex range")
    ///     .build();
    /// let range = domain.local_range(attr);
    ///
    /// // explicitly end a range
    /// drop(range)
    /// ```
    pub fn local_range<'a>(&'a self, arg: impl Into<EventArgument<'a>>) -> LocalRange<'a> {
        let event_arg = arg.into();
        match self.validate_event_argument(&event_arg) {
            Ok(()) => LocalRange::new(event_arg, self),
            Err(error) => {
                debug_assert!(false, "{error}");
                LocalRange::new(self.strip_foreign_domain_values(event_arg), self)
            }
        }
    }

    /// Fallible variant of [`Domain::local_range`].
    ///
    /// # Errors
    ///
    /// Returns [`NvtxError::DomainMismatch`] when `arg` references domain-owned
    /// values from a different domain.
    pub fn try_local_range<'a>(
        &'a self,
        arg: impl Into<EventArgument<'a>>,
    ) -> Result<LocalRange<'a>, NvtxError> {
        let event_arg = arg.into();
        self.validate_event_argument(&event_arg)?;
        Ok(LocalRange::new(event_arg, self))
    }

    /// Create an RAII-friendly, domain-owned range type which (1) can be moved across
    /// thread boundaries and (2) automatically ended when dropped.
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    ///
    /// // creation from a unicode string
    /// let range = domain.range(nvtx::Str::from_str_lossy("simple name"));
    ///
    /// // creation from a c string (from rust 1.77+)
    /// let range = domain.range(c"simple name");
    ///
    /// // creation from EventAttributes
    /// let attr = domain
    ///     .event_attributes_builder()
    ///     .payload(1)
    ///     .message(c"complex range")
    ///     .build();
    /// let range = domain.range(attr);
    ///
    /// // explicitly end a range
    /// drop(range)
    /// ```
    ///
    pub fn range<'a>(&'a self, arg: impl Into<EventArgument<'a>>) -> Range<'a> {
        let event_arg = arg.into();
        match self.validate_event_argument(&event_arg) {
            Ok(()) => Range::new(event_arg, self),
            Err(error) => {
                debug_assert!(false, "{error}");
                Range::new(self.strip_foreign_domain_values(event_arg), self)
            }
        }
    }

    /// Fallible variant of [`Domain::range`].
    ///
    /// # Errors
    ///
    /// Returns [`NvtxError::DomainMismatch`] when `arg` references domain-owned
    /// values from a different domain.
    pub fn try_range<'a>(
        &'a self,
        arg: impl Into<EventArgument<'a>>,
    ) -> Result<Range<'a>, NvtxError> {
        let event_arg = arg.into();
        self.validate_event_argument(&event_arg)?;
        Ok(Range::new(event_arg, self))
    }

    /// Internal function for starting a range and returning a raw Range Id
    pub(super) fn range_start<'a>(&self, arg: impl Into<EventArgument<'a>>) -> u64 {
        let arg = match arg.into() {
            EventArgument::Attributes(attr) => attr,
            EventArgument::Message(m) => EventAttributes {
                category: None,
                color: None,
                message: Some(m),
                payload: None,
            },
        };
        nvtx_sys::domain_range_start_ex(self.handle, &arg.encode())
    }

    /// Internal function for ending a range given a raw Range Id
    pub(super) fn range_end(&self, range_id: u64) {
        nvtx_sys::domain_range_end(self.handle, range_id);
    }

    /// Name a resource
    ///
    /// ```
    /// let domain = nvtx::Domain::new(c"Domain");
    /// let pthread_id = 13854;
    /// domain.name_resource(
    ///     nvtx::domain::GenericIdentifier::PosixThread(pthread_id),
    ///     c"My custom name");
    /// #[cfg(feature = "cuda")]
    /// domain.name_resource(nvtx::domain::CudaIdentifier::Device(0), c"My device");
    /// #[cfg(feature = "cuda_runtime")]
    /// domain.name_resource(nvtx::domain::CudaRuntimeIdentifier::Device(1), c"My device");
    /// ```
    pub fn name_resource<'a>(
        &'a self,
        identifier: impl Into<Identifier>,
        name: impl Into<Message<'a>>,
    ) -> Resource<'a> {
        let materialized_identifier: Identifier = identifier.into();
        let materialized_name: Message = name.into();
        let (msg_type, msg_value) = materialized_name.encode();
        let (id_type, id_value) = materialized_identifier.encode();
        let attrs = nvtx_sys::ResourceAttributes {
            // CAST: NVTX_VERSION is forwarded as the raw 16-bit API version field.
            version: nvtx_sys::NVTX_VERSION as u16,
            // CAST: Size is a fixed ABI field encoded as a 16-bit value.
            size: nvtx_sys::NVTX_RESOURCE_ATTRIBUTES_SIZE as u16,
            // CAST: NVTX resource type IDs are transported as raw 32-bit patterns into an i32 field.
            identifierType: id_type as i32,
            identifier: id_value,
            messageType: i32::from(msg_type),
            message: msg_value,
        };
        Resource {
            handle: nvtx_sys::domain_resource_create(self.handle, attrs),
            _lifetime: PhantomData,
        }
    }

    /// Create a user defined synchronization object.
    ///
    /// This is used to track non-OS synchronization working with spinlocks and atomics.
    pub fn user_sync<'a>(&'a self, name: impl Into<Message<'a>>) -> sync::UserSync<'a> {
        let message = name.into();
        let (msg_type, msg_value) = message.encode();
        let attrs = nvtx_sys::SyncUserAttributes {
            // CAST: NVTX_VERSION is forwarded as the raw 16-bit API version field.
            version: nvtx_sys::NVTX_VERSION as u16,
            // CAST: Size is a fixed ABI field encoded as a 16-bit value.
            size: nvtx_sys::NVTX_SYNC_USER_ATTRIBUTES_SIZE as u16,
            messageType: i32::from(msg_type),
            message: msg_value,
        };
        let handle = nvtx_sys::domain_syncuser_create(self.handle, attrs);
        sync::UserSync {
            handle,
            _lifetime: PhantomData,
        }
    }
}

impl Drop for Domain {
    fn drop(&mut self) {
        nvtx_sys::domain_destroy(self.handle);
    }
}

#[cfg(all(test, feature = "std"))]
mod tests {
    use super::*;
    use crate::common::TestUtils;
    use std::ffi::CString;
    use widestring::WideCString;

    fn lossy_str(value: &str) -> Str {
        Str::from_str_lossy(value)
    }

    #[test]
    fn test_register_string() {
        let d = Domain::new(lossy_str("d"));
        TestUtils::assert_domain_string_registration(&d, "hello");

        // Test different strings are different
        let s1 = d.register_string(lossy_str("hello"));
        let s2 = d.register_string(lossy_str("hello2"));
        assert_ne!(s1, s2);
    }

    #[test]
    fn test_register_strings() {
        let d = Domain::new(lossy_str("d"));
        let [s1, s2, s3] =
            d.register_strings([lossy_str("hello"), lossy_str("hello2"), lossy_str("hello")]);
        assert_eq!(s1, s3);
        assert_ne!(s1, s2);
    }

    #[test]
    fn test_register_category() {
        let d = Domain::new(lossy_str("d"));
        TestUtils::assert_domain_category_registration(&d, "hello");

        // Test different categories are different
        let c1 = d.register_category(lossy_str("hello"));
        let c2 = d.register_category(lossy_str("hello2"));
        assert_ne!(c1, c2);
    }

    #[test]
    fn test_register_categories() {
        let d = Domain::new(lossy_str("d"));
        let [c1, c2, c3] =
            d.register_categories([lossy_str("hello"), lossy_str("hello2"), lossy_str("hello")]);
        assert_eq!(c1, c3);
        assert_ne!(c1, c2);
    }

    #[test]
    fn test_message_ascii() {
        let cstr = CString::new("hello").unwrap();
        let m = Message::Ascii(cstr.clone());
        assert!(matches!(m, Message::Ascii(s) if s == cstr));
    }

    #[test]
    fn test_message_unicode() {
        let s = "hello";
        let wstr = WideCString::from_str(s).unwrap();
        let m = Message::Unicode(wstr.clone());
        assert!(matches!(m, Message::Unicode(s) if s == wstr));
    }

    #[test]
    fn test_message_registered() {
        let d = Domain::new(lossy_str("d"));
        let reg = d.register_string(lossy_str("test"));
        let m = Message::Registered(reg);
        assert!(matches!(m, Message::Registered(s) if s == reg));
    }

    #[test]
    fn test_encode_ascii() {
        let cstr = CString::new("hello").unwrap();
        let m = Message::Ascii(cstr.clone());
        TestUtils::assert_message_ascii_encoding(&m, "hello");
    }

    #[test]
    fn test_encode_unicode() {
        let s = "hello";
        let wstr = WideCString::from_str(s).unwrap();
        let m = Message::Unicode(wstr.clone());
        TestUtils::assert_message_unicode_encoding(&m, "hello");
    }

    #[test]
    fn test_encode_registered() {
        let domain = Domain::new(lossy_str("d"));
        let registered_message = domain.register_string(lossy_str("test"));
        let message = Message::Registered(registered_message);
        let (message_type, message_value) = message.encode();
        assert_eq!(
            message_type,
            nvtx_sys::MessageType::NVTX_MESSAGE_TYPE_REGISTERED
        );
        // SAFETY: The registered message type is checked above, so reading `registered` is valid.
        unsafe {
            assert!(
                matches!(message_value, nvtx_sys::MessageValue{ registered: registered_handle } if registered_handle == nvtx_sys::ffi::nvtxStringHandle_t::from(registered_message.handle()))
            );
        }
    }

    #[test]
    fn test_builder_color() {
        let d = Domain::new(lossy_str("d"));
        let builder = d.event_attributes_builder();
        let color = Color::new(0x11, 0x22, 0x44, 0x88);
        let attr = builder.color(color).build();
        assert!(matches!(attr.color, Some(c) if c == color));
    }

    #[test]
    fn test_builder_category() {
        let d = Domain::new(lossy_str("d"));
        let cat = d.register_category(lossy_str("cat"));
        let builder = d.event_attributes_builder();
        let attr = builder.category(cat).build();
        assert!(matches!(attr.category, Some(c) if c == cat));
    }

    #[test]
    fn test_builder_category_rebinds_foreign_category_id_for_best_effort_use() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let c1 = d1.register_category(lossy_str("category"));
        let d2 = Domain::new(lossy_str("Domain2"));

        let rebound = d2.event_attributes_builder().category_in_builder_domain(c1);

        assert_eq!(rebound.encode_id(), c1.encode_id());
        assert!(core::ptr::eq(rebound.domain(), &d2));
    }

    #[test]
    fn test_builder_category_name() {
        let d = Domain::new(lossy_str("d"));
        let builder = d.event_attributes_builder();
        let attr = builder.category_name(lossy_str("cat")).build();
        let cat = d.register_category(lossy_str("cat"));
        assert!(matches!(attr.category, Some(c) if c == cat));
    }

    #[test]
    fn test_builder_payload() {
        let d = Domain::new(lossy_str("d"));
        let builder = d.event_attributes_builder();
        let attr = builder.clone().payload(1_i32).build();
        assert!(matches!(attr.payload, Some(Payload::Int32(i)) if i == 1_i32));
        let attr = builder.clone().payload(2_u32).build();
        assert!(matches!(attr.payload, Some(Payload::Uint32(i)) if i == 2_u32));
        let attr = builder.clone().payload(1_i64).build();
        assert!(matches!(attr.payload, Some(Payload::Int64(i)) if i == 1_i64));
        let attr = builder.clone().payload(2_u64).build();
        assert!(matches!(attr.payload, Some(Payload::Uint64(i)) if i == 2_u64));
        let attr = builder.clone().payload(1.0_f32).build();
        assert!(matches!(attr.payload, Some(Payload::Float(i)) if i == 1.0_f32));
        let attr = builder.clone().payload(2.0_f64).build();
        assert!(matches!(attr.payload, Some(Payload::Double(i)) if i == 2.0_f64));
    }

    #[test]
    fn test_builder_message() {
        let d = Domain::new(lossy_str("d"));
        let builder = d.event_attributes_builder();
        let attr = builder.message(lossy_str("This is a message")).build();
        let registered = d.register_string(lossy_str("This is a message"));
        assert!(matches!(attr.message, Some(Message::Registered(r)) if r == registered));
    }

    #[test]
    fn test_unowned_category_try_mark_error() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let c1 = d1.register_category(lossy_str("category"));
        let d2 = Domain::new(lossy_str("Domain2"));
        let attr = d1.event_attributes_builder().category(c1).build();
        assert!(matches!(d2.try_mark(attr), Err(NvtxError::DomainMismatch)));
    }

    #[test]
    fn test_unowned_category_mark_strips_foreign_category() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let c1 = d1.register_category(lossy_str("category"));
        let d2 = Domain::new(lossy_str("Domain2"));
        let attr = EventAttributes {
            category: Some(c1),
            color: None,
            message: Some(Message::from(lossy_str("message"))),
            payload: Some(Payload::Int32(1)),
        };

        let EventArgument::Attributes(sanitized) =
            d2.strip_foreign_domain_values(EventArgument::Attributes(attr))
        else {
            unreachable!("expected sanitized attributes");
        };

        assert!(sanitized.category.is_none());
        assert!(matches!(sanitized.message, Some(Message::Unicode(_))));
        assert!(matches!(sanitized.payload, Some(Payload::Int32(1))));
    }

    #[test]
    fn test_unowned_category_try_builder_error() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let c1 = d1.register_category(lossy_str("category"));
        let d2 = Domain::new(lossy_str("Domain2"));
        assert!(matches!(
            d2.event_attributes_builder().try_category(c1),
            Err(NvtxError::DomainMismatch)
        ));
    }

    #[test]
    fn test_unowned_category_try_range_error() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let c1 = d1.register_category(lossy_str("category"));
        let d2 = Domain::new(lossy_str("Domain2"));
        let attr = d1.event_attributes_builder().category(c1).build();
        assert!(matches!(d2.try_range(attr), Err(NvtxError::DomainMismatch)));
    }

    #[test]
    fn test_unowned_category_range_can_start_from_sanitized_attributes() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let c1 = d1.register_category(lossy_str("category"));
        let d2 = Domain::new(lossy_str("Domain2"));
        let attr = EventAttributes {
            category: Some(c1),
            color: None,
            message: Some(Message::from(lossy_str("message"))),
            payload: Some(Payload::Int32(1)),
        };

        let event_arg = d2.strip_foreign_domain_values(EventArgument::Attributes(attr));
        let range = Range::new(event_arg, &d2);

        assert!(range.id.is_some());
    }

    #[test]
    fn test_unowned_category_try_local_range_error() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let c1 = d1.register_category(lossy_str("category"));
        let d2 = Domain::new(lossy_str("Domain2"));
        let attr = d1.event_attributes_builder().category(c1).build();
        assert!(matches!(
            d2.try_local_range(attr),
            Err(NvtxError::DomainMismatch)
        ));
    }

    #[test]
    fn test_unowned_category_local_range_can_start_from_sanitized_attributes() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let c1 = d1.register_category(lossy_str("category"));
        let d2 = Domain::new(lossy_str("Domain2"));
        let attr = EventAttributes {
            category: Some(c1),
            color: None,
            message: Some(Message::from(lossy_str("message"))),
            payload: Some(Payload::Int32(1)),
        };

        let event_arg = d2.strip_foreign_domain_values(EventArgument::Attributes(attr));
        let range = LocalRange::new(event_arg, &d2);

        assert!(range.active);
    }

    #[test]
    fn test_unowned_string_try_mark_error() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let s1 = d1.register_string(lossy_str("test string"));
        let d2 = Domain::new(lossy_str("Domain2"));
        let attr = d1.event_attributes_builder().message(s1).build();
        assert!(matches!(d2.try_mark(attr), Err(NvtxError::DomainMismatch)));
    }

    #[test]
    fn test_unowned_string_mark_strips_foreign_registered_message() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let s1 = d1.register_string(lossy_str("test string"));
        let d2 = Domain::new(lossy_str("Domain2"));

        let EventArgument::Attributes(sanitized) =
            d2.strip_foreign_domain_values(EventArgument::Message(Message::Registered(s1)))
        else {
            unreachable!("expected sanitized attributes");
        };

        assert!(sanitized.category.is_none());
        assert!(sanitized.message.is_none());
        assert!(sanitized.payload.is_none());
    }

    #[test]
    fn test_unowned_string_try_builder_error() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let s1 = d1.register_string(lossy_str("test string"));
        let d2 = Domain::new(lossy_str("Domain2"));
        assert!(matches!(
            d2.event_attributes_builder().try_message(s1),
            Err(NvtxError::DomainMismatch)
        ));
    }

    #[test]
    fn test_unowned_string_try_range_error() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let s1 = d1.register_string(lossy_str("test string"));
        let d2 = Domain::new(lossy_str("Domain2"));
        let attr = d1.event_attributes_builder().message(s1).build();
        assert!(matches!(d2.try_range(attr), Err(NvtxError::DomainMismatch)));
    }

    #[test]
    fn test_unowned_string_try_local_range_error() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let s1 = d1.register_string(lossy_str("test string"));
        let d2 = Domain::new(lossy_str("Domain2"));
        let attr = d1.event_attributes_builder().message(s1).build();
        assert!(matches!(
            d2.try_local_range(attr),
            Err(NvtxError::DomainMismatch)
        ));
    }

    #[test]
    fn test_simple_domain_validation() {
        let d1 = Domain::new(lossy_str("Domain1"));
        let s1 = d1.register_string(lossy_str("test string"));
        let d2 = Domain::new(lossy_str("Domain2"));
        // Create attributes with a string from d1, then use them with d2
        let attr = d1.event_attributes_builder().message(s1).build();
        assert!(matches!(d2.try_mark(attr), Err(NvtxError::DomainMismatch)));
    }
}
