use crate::{
    common::{
        event_attributes::GenericEventAttributesBuilder, GenericEventArgument,
        GenericEventAttributes,
    },
    Color, Payload, Str, TypeValueEncodable,
};
use std::{
    collections::HashMap,
    marker::PhantomData,
    sync::{
        atomic::{AtomicU32, Ordering},
        Mutex,
    },
};

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
/// let domain = nvtx::Domain::new("Domain");
/// let cat = domain.register_category("Category1");
///
/// let attr = domain.event_attributes_builder()
///                .category(cat)
///                .color([20, 192, 240])
///                .payload(3.141592)
///                .message("Hello")
///                .build();
/// ```
#[derive(Clone)]
pub struct EventAttributesBuilder<'a> {
    pub(super) domain: &'a Domain,
    pub(super) inner: GenericEventAttributesBuilder<Category<'a>, Message<'a>>,
}

impl<'a> EventAttributesBuilder<'a> {
    /// Update the attribute's category. An assertion will be thrown if a Category is
    /// passed in whose domain is not the same as this builder.
    ///
    /// ```
    /// let domain = nvtx::Domain::new("Domain");
    /// let cat = domain.register_category("Category1");
    /// // ...
    /// let builder = domain.event_attributes_builder();
    /// // ...
    /// let builder = builder.category(cat);
    /// ```
    pub fn category(mut self, category: Category<'a>) -> EventAttributesBuilder<'a> {
        assert!(
            std::ptr::eq(category.domain(), self.domain),
            "EventAttributesBuilder's Domain differs from Category's Domain"
        );
        self.inner = self.inner.category(category);
        self
    }

    /// Update the attribute's category. An assertion will be thrown if a Category is
    /// passed in whose domain is not the same as this builder.
    ///
    /// ```
    /// let domain = nvtx::Domain::new("Domain");
    /// // ...
    /// let builder = domain.event_attributes_builder();
    /// // ...
    /// let builder = builder.category_name("Category2");
    /// ```
    pub fn category_name(mut self, name: impl Into<Str>) -> EventAttributesBuilder<'a> {
        let category = self.domain.register_category(name);
        self.inner = self.inner.category(category);
        self
    }

    /// Update the builder's held [`Color`]. See [`Color`] for valid conversions.
    ///
    /// ```
    /// let domain = nvtx::Domain::new("Domain");
    /// let builder = domain.event_attributes_builder();
    /// // ...
    /// let builder = builder.color([255, 255, 255]);
    /// ```
    pub fn color(mut self, color: impl Into<Color>) -> EventAttributesBuilder<'a> {
        self.inner = self.inner.color(color);
        self
    }

    /// Update the builder's held [`Payload`]. See [`Payload`] for valid conversions.
    ///
    /// ```
    /// let domain = nvtx::Domain::new("Domain");
    /// let builder = domain.event_attributes_builder();
    /// // ...
    /// let builder = builder.payload(3.1415926535);
    /// ```
    pub fn payload(mut self, payload: impl Into<Payload>) -> EventAttributesBuilder<'a> {
        self.inner = self.inner.payload(payload);
        self
    }

    /// Update the attribute's message. An assertion will be thrown if a
    /// [`super::RegisteredString`] is passed in whose domain is not the same as this
    /// builder.
    ///
    /// ```
    /// let domain = nvtx::Domain::new("Domain");
    /// let builder = domain.event_attributes_builder();
    /// // ...
    /// let builder = builder.message("test");
    /// ```
    pub fn message(mut self, message: impl Into<Message<'a>>) -> EventAttributesBuilder<'a> {
        // implementation optimization: always prefer registered strings
        let msg = match message.into() {
            Message::Ascii(s) => self.domain.register_string(s.to_str().unwrap().to_string()),
            Message::Unicode(s) => self.domain.register_string(s.to_string().unwrap()),
            Message::Registered(r) => r,
        };
        assert!(
            std::ptr::eq(msg.domain(), self.domain),
            "EventAttributesBuilder's Domain differs from RegisteredString's Domain"
        );
        self.inner = self.inner.message(Message::Registered(msg));
        self
    }

    /// Construct an [`EventAttributes`] from the builder's held state.
    ///
    /// ```
    /// let domain = nvtx::Domain::new("Domain");
    /// let cat = domain.register_category("Category1");
    /// let attr = domain.event_attributes_builder()
    ///                 .message("Example Range")
    ///                 .color([224, 192, 128])
    ///                 .category(cat)
    ///                 .payload(1234567)
    ///                 .build();
    /// ```
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

/// Represents a domain for high-level grouping within NSight profilers.
#[derive(Debug)]
pub struct Domain {
    handle: nvtx_sys::DomainHandle,
    registered_strings: AtomicU32,
    registered_categories: AtomicU32,
    strings: Mutex<HashMap<String, (nvtx_sys::StringHandle, u32)>>,
    categories: Mutex<HashMap<String, u32>>,
}

impl Domain {
    /// Register a NVTX domain.
    ///
    /// See [`Str`] for valid conversions.
    ///
    /// ```
    /// let domain = nvtx::Domain::new("Domain");
    /// ```
    pub fn new(name: impl Into<Str>) -> Self {
        Domain {
            handle: match name.into() {
                Str::Ascii(s) => nvtx_sys::domain_create_ascii(&s),
                Str::Unicode(s) => nvtx_sys::domain_create_unicode(&s),
            },
            registered_strings: AtomicU32::new(0),
            registered_categories: AtomicU32::new(0),
            strings: Mutex::new(HashMap::default()),
            categories: Mutex::new(HashMap::default()),
        }
    }

    /// Gets a new builder instance for event attributes in the current domain.
    ///
    /// ```
    /// let domain = nvtx::Domain::new("Domain");
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
    /// let domain = nvtx::Domain::new("Domain");
    /// // ...
    /// let my_str = domain.register_string("My immutable string");
    /// ```
    pub fn register_string(&self, string: impl Into<Str>) -> RegisteredString<'_> {
        let into_string: Str = string.into();
        let owned_string = match &into_string {
            Str::Ascii(s) => s.to_str().unwrap().to_string(),
            Str::Unicode(s) => s.to_string().unwrap(),
        };
        let (handle, uid) = *self
            .strings
            .lock()
            .unwrap()
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
    /// let domain = nvtx::Domain::new("Domain");
    /// // ...
    /// let [a, b, c] = domain.register_strings(["A", "B", "C"]);
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
    /// let domain = nvtx::Domain::new("Domain");
    /// // ...
    /// let cat = domain.register_category("Category");
    /// ```
    pub fn register_category(&self, name: impl Into<Str>) -> Category<'_> {
        let into_name: Str = name.into();
        let owned_name = match &into_name {
            Str::Ascii(s) => s.to_str().unwrap().to_string(),
            Str::Unicode(s) => s.to_string().unwrap(),
        };

        let id = *self
            .categories
            .lock()
            .unwrap()
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
    /// let domain = nvtx::Domain::new("Domain");
    /// // ...
    /// let [cat_a, cat_b] = domain.register_categories(["CatA", "CatB"]);
    /// ```
    pub fn register_categories<const N: usize>(
        &self,
        names: [impl Into<Str>; N],
    ) -> [Category<'_>; N] {
        names.map(|name| self.register_category(name))
    }

    /// Marks an instantaneous event in the application belonging to a domain.
    ///
    /// A marker can contain a text message or specify information using the event
    /// attributes structure. These attributes include a text message, color, category,
    /// and a payload. Each of the attributes is optional.
    ///
    /// ```
    /// let domain = nvtx::Domain::new("Domain");
    /// // ...
    /// domain.mark("Sample mark");
    ///
    /// domain.mark(c"Another example");
    ///
    /// domain.mark(
    ///   domain.event_attributes_builder()
    ///     .message("Interesting example")
    ///     .color([255, 0, 0])
    ///     .build());
    ///
    /// let reg_str = domain.register_string("Registered String");
    /// domain.mark(reg_str);
    /// ```
    pub fn mark<'a>(&'a self, arg: impl Into<EventArgument<'a>>) {
        match arg.into() {
            EventArgument::Attributes(attr) => {
                if let Some(category) = &attr.category {
                    assert!(
                        std::ptr::eq(category.domain(), self),
                        "EventAttributes' Domain does not match current Domain"
                    );
                }
                if let Some(Message::Registered(reg_str)) = &attr.message {
                    assert!(
                        std::ptr::eq(reg_str.domain(), self),
                        "EventAttributes' Domain does not match current Domain"
                    );
                }
                let encoded = attr.encode();
                nvtx_sys::domain_mark_ex(self.handle, &encoded)
            }
            EventArgument::Message(m) => {
                if let Message::Registered(reg_str) = m {
                    assert!(
                        std::ptr::eq(reg_str.domain(), self),
                        "EventAttributes' Domain does not match current Domain"
                    );
                }
                let attr: EventAttributes = m.into();
                let encoded = attr.encode();
                nvtx_sys::domain_mark_ex(self.handle, &encoded)
            }
        }
    }

    /// Create an RAII-friendly, domain-owned range type which (1) cannot be moved across
    /// thread boundaries and (2) automatically ended when dropped.
    ///
    /// ```
    /// let domain = nvtx::Domain::new("Domain");
    ///
    /// // creation from Rust string
    /// let range = domain.local_range("simple name");
    ///
    /// // creation from C string (since 1.77)
    /// let range = domain.local_range(c"simple name");
    ///
    /// // creation from EventAttributes
    /// let attr = domain
    ///     .event_attributes_builder()
    ///     .payload(1)
    ///     .message("complex range")
    ///     .build();
    /// let range = domain.local_range(attr);
    ///
    /// // explicitly end a range
    /// drop(range)
    /// ```
    pub fn local_range<'a>(&'a self, arg: impl Into<EventArgument<'a>>) -> LocalRange<'a> {
        LocalRange::new(arg, self)
    }

    /// Create an RAII-friendly, domain-owned range type which (1) can be moved across
    /// thread boundaries and (2) automatically ended when dropped.
    ///
    /// ```
    /// let domain = nvtx::Domain::new("Domain");
    ///
    /// // creation from a unicode string
    /// let range = domain.range("simple name");
    ///
    /// // creation from a c string (from rust 1.77+)
    /// let range = domain.range(c"simple name");
    ///
    /// // creation from EventAttributes
    /// let attr = domain
    ///     .event_attributes_builder()
    ///     .payload(1)
    ///     .message("complex range")
    ///     .build();
    /// let range = domain.range(attr);
    ///
    /// // explicitly end a range
    /// drop(range)
    /// ```
    pub fn range<'a>(&'a self, arg: impl Into<EventArgument<'a>>) -> Range<'a> {
        let event_arg = arg.into();
        match &event_arg {
            EventArgument::Attributes(attr) => {
                // Validate that all categories and messages belong to this domain
                if let Some(category) = &attr.category {
                    assert!(
                        std::ptr::eq(category.domain(), self),
                        "EventAttributes' Domain does not match current Domain"
                    );
                }
                if let Some(Message::Registered(reg_str)) = &attr.message {
                    assert!(
                        std::ptr::eq(reg_str.domain(), self),
                        "EventAttributes' Domain does not match current Domain"
                    );
                }
            }
            EventArgument::Message(m) => {
                // NEW: Add validation here
                if let Message::Registered(reg_str) = m {
                    assert!(
                        std::ptr::eq(reg_str.domain(), self),
                        "EventAttributes' Domain does not match current Domain"
                    );
                }
            }
        }
        Range::new(event_arg, self)
    }

    /// Internal function for starting a range and returning a raw Range Id
    pub(super) fn range_start<'a>(&self, arg: impl Into<EventArgument<'a>>) -> u64 {
        let arg = match arg.into() {
            EventArgument::Attributes(attr) => attr,
            EventArgument::Message(m) => m.into(),
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
    /// let domain = nvtx::Domain::new("Domain");
    /// let pthread_id = 13854;
    /// domain.name_resource(
    ///     nvtx::domain::GenericIdentifier::PosixThread(pthread_id),
    ///     "My custom name");
    /// #[cfg(feature = "cuda")]
    /// domain.name_resource(nvtx::domain::CudaIdentifier::Device(0), "My device");
    /// #[cfg(feature = "cuda_runtime")]
    /// domain.name_resource(nvtx::domain::CudaRuntimeIdentifier::Device(1), "My device");
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
            version: nvtx_sys::NVTX_VERSION as u16,
            size: 32,
            identifierType: id_type as i32,
            identifier: id_value,
            messageType: msg_type as i32,
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
            version: nvtx_sys::NVTX_VERSION as u16,
            size: 16,
            messageType: msg_type as i32,
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
        nvtx_sys::domain_destroy(self.handle)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::common::TestUtils;
    use std::ffi::CString;
    use widestring::WideCString;

    #[test]
    fn test_register_string() {
        let d = Domain::new("d");
        TestUtils::assert_domain_string_registration(&d, "hello");

        // Test different strings are different
        let s1 = d.register_string("hello");
        let s2 = d.register_string("hello2");
        assert_ne!(s1, s2);
    }

    #[test]
    fn test_register_strings() {
        let d = Domain::new("d");
        let [s1, s2, s3] = d.register_strings(["hello", "hello2", "hello"]);
        assert_eq!(s1, s3);
        assert_ne!(s1, s2);
    }

    #[test]
    fn test_register_category() {
        let d = Domain::new("d");
        TestUtils::assert_domain_category_registration(&d, "hello");

        // Test different categories are different
        let c1 = d.register_category("hello");
        let c2 = d.register_category("hello2");
        assert_ne!(c1, c2);
    }

    #[test]
    fn test_register_categories() {
        let d = Domain::new("d");
        let [c1, c2, c3] = d.register_categories(["hello", "hello2", "hello"]);
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
        let d = Domain::new("d");
        let reg = d.register_string("test");
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
        let d = Domain::new("d");
        let reg = d.register_string("test");
        let m = Message::Registered(reg);
        let (t, v) = m.encode();
        assert_eq!(t, nvtx_sys::MessageType::NVTX_MESSAGE_TYPE_REGISTERED);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::MessageValue{ registered: r } if r == nvtx_sys::ffi::nvtxStringHandle_t::from(reg.handle()))
            );
        }
    }

    #[test]
    fn test_builder_color() {
        let d = Domain::new("d");
        let builder = d.event_attributes_builder();
        let color = Color::new(0x11, 0x22, 0x44, 0x88);
        let attr = builder.color(color).build();
        assert!(matches!(attr.color, Some(c) if c == color));
    }

    #[test]
    fn test_builder_category() {
        let d = Domain::new("d");
        let cat = d.register_category("cat");
        let builder = d.event_attributes_builder();
        let attr = builder.category(cat).build();
        assert!(matches!(attr.category, Some(c) if c == cat));
    }

    #[test]
    fn test_builder_category_name() {
        let d = Domain::new("d");
        let builder = d.event_attributes_builder();
        let attr = builder.category_name("cat").build();
        let cat = d.register_category("cat");
        assert!(matches!(attr.category, Some(c) if c == cat));
    }

    #[test]
    fn test_builder_payload() {
        let d = Domain::new("d");
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
        let d = Domain::new("d");
        let builder = d.event_attributes_builder();
        let attr = builder.message("This is a message").build();
        let registered = d.register_string("This is a message");
        assert!(matches!(attr.message, Some(Message::Registered(r)) if r == registered));
    }

    #[test]
    #[should_panic(expected = "EventAttributes' Domain does not match current Domain")]
    fn test_unowned_category_panic_mark() {
        let d1 = Domain::new("Domain1");
        let c1 = d1.register_category("category");
        let d2 = Domain::new("Domain2");
        d2.mark(d1.event_attributes_builder().category(c1).build());
    }

    #[test]
    #[should_panic(expected = "EventAttributesBuilder's Domain differs from Category's Domain")]
    fn test_unowned_category_panic_in_builder_mark() {
        let d1 = Domain::new("Domain1");
        let c1 = d1.register_category("category");
        let d2 = Domain::new("Domain2");
        d2.mark(d2.event_attributes_builder().category(c1).build());
    }

    #[test]
    #[should_panic(expected = "EventAttributes' Domain does not match current Domain")]
    fn test_unowned_category_panic_range() {
        let d1 = Domain::new("Domain1");
        let c1 = d1.register_category("category");
        let d2 = Domain::new("Domain2");
        d2.range(d1.event_attributes_builder().category(c1).build());
    }

    #[test]
    #[should_panic(expected = "EventAttributesBuilder's Domain differs from Category's Domain")]
    fn test_unowned_category_panic_in_builder_range() {
        let d1 = Domain::new("Domain1");
        let c1 = d1.register_category("category");
        let d2 = Domain::new("Domain2");
        d2.range(d2.event_attributes_builder().category(c1).build());
    }

    #[test]
    #[should_panic(expected = "EventAttributes' Domain does not match current Domain")]
    fn test_unowned_string_panic_mark() {
        let d1 = Domain::new("Domain1");
        let s1 = d1.register_string("test string");
        let d2 = Domain::new("Domain2");
        d2.mark(d1.event_attributes_builder().message(s1).build());
    }

    #[test]
    #[should_panic(
        expected = "EventAttributesBuilder's Domain differs from RegisteredString's Domain"
    )]
    fn test_unowned_string_panic_in_builder_mark() {
        let d1 = Domain::new("Domain1");
        let s1 = d1.register_string("test string");
        let d2 = Domain::new("Domain2");
        d2.mark(d2.event_attributes_builder().message(s1).build());
    }

    #[test]
    #[should_panic(expected = "EventAttributes' Domain does not match current Domain")]
    fn test_unowned_string_panic_range() {
        let d1 = Domain::new("Domain1");
        let s1 = d1.register_string("test string");
        let d2 = Domain::new("Domain2");
        d2.range(d1.event_attributes_builder().message(s1).build());
    }

    #[test]
    #[should_panic(
        expected = "EventAttributesBuilder's Domain differs from RegisteredString's Domain"
    )]
    fn test_unowned_string_panic_in_builder_range() {
        let d1 = Domain::new("Domain1");
        let s1 = d1.register_string("test string");
        let d2 = Domain::new("Domain2");
        d2.range(d2.event_attributes_builder().message(s1).build());
    }

    #[test]
    #[should_panic(expected = "EventAttributes' Domain does not match current Domain")]
    fn test_simple_domain_validation() {
        let d1 = Domain::new("Domain1");
        let s1 = d1.register_string("test string");
        let d2 = Domain::new("Domain2");
        // Create attributes with a string from d1, then use them with d2
        let attr = d1.event_attributes_builder().message(s1).build();
        d2.mark(attr);
    }
}
