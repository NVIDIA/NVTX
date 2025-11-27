#![deny(missing_docs)]
#![cfg_attr(docsrs, feature(doc_auto_cfg, doc_cfg))]

//! Crate for interfacing with NVIDIA's NVTX API
//!
//! When not running within NSight tools, the calls will dispatch to
//! empty method stubs, thus enabling low-overhead profiling.
//!
//! * All events are fully supported:
//!   * process ranges [`Range`] and [`domain::Range`]
//!   * thread ranges [`LocalRange`] and [`domain::LocalRange`]
//!   * marks [`mark`] and [`Domain::mark`]
//! * Naming threads is fully supported (See [`name_thread`] and [`name_current_thread`]).
//! * Domain, category, and registered strings are fully supported.
//! * The user-defined synchronization API is implemented.
//! * The user-defined resource naming API is implemented for the following platforms:
//!   * Pthreads (on unix-like platforms)
//!   * CUDA
//!   * CUDA Runtime
//!
//! ## Features
//!
//! This crate defines a few features which provide opt-in behavior. By default, all
//! features are enabled.
//!
//! * **color-names** -
//!   When enabled, [`color`] is populated with many human-readable color names.
//!
//! * **name-current-thread** -
//!   When enabled, [`name_current_thread`] is added to the crate. This may be preferred
//!   to manually determining the current OS-native thread ID.
//!
//! * **cuda** -
//!   When enabled, [`name_cuda_resource`] is added to the crate. This enables the naming
//!   of CUDA resources such as Devices, Contexts, Events, and Streams. The feature also
//!   adds [`domain::CudaIdentifier`] to provide an alternative naming mechanism via
//!   [`Domain::name_resource`].
//!
//! * **cuda_runtime** -
//!   When enabled, [`name_cudart_resource`] is added to the crate. This enables the
//!   naming of CUDA runtime resources such as Devices, Events, and Streams. The feature
//!   also adds [`domain::CudaRuntimeIdentifier`] to provide an alternative naming
//!   mechanism via [`Domain::name_resource`].
//!
//! * **tracing** -
//!   When enabled, a tracing `Layer` is provided which consumes tracing spans and events
//!   which will yield NVTX ranges and marks, respectively. Only a subset of
//!   functionality is supported.
//!
//! ## Platform-specific types
//!
//! * **PThread Resource Naming** -
//!   `PThreadIdentifier` is added to the [`domain`] module on UNIX-like platforms. This
//!   enables the naming of Pthread-specific entities such as mutexes, semaphores,
//!   condition variables, and read-write-locks.

mod category;
/// Category for use with marks and ranges.
pub use category::Category;

/// Common utilities and types shared between global and domain contexts
mod common;
#[cfg(test)]
pub use common::test_utils;

/// Support for colors.
pub mod color;
/// Color type for controlling appearance within NSight profilers.
pub type Color = color::Color;

#[cfg(feature = "cuda")]
/// Support for CUDA-related APIs.
mod cuda;
#[cfg(feature = "cuda")]
pub use cuda::*;

#[cfg(feature = "cuda_runtime")]
/// Support for CUDA runtime related APIs.
mod cuda_runtime;
#[cfg(feature = "cuda_runtime")]
pub use cuda_runtime::*;

/// Specialized types for use within a domain context.
pub mod domain;
/// Domain for high-level grouping within NSight profilers.
pub type Domain = domain::Domain;

/// Convenience wrapper for all valid argument types to ranges and marks.
///
/// * Any string type will be translated to [`EventArgument::Message`].
/// * If [`EventArgument::Attributes`] is the active discriminator:
///   - If its [`EventAttributes`] only specifies a message, then message will be used.
///   - Otherwise, the existing [`EventAttributes`] will be used for the event.
///
pub type EventArgument = crate::common::GenericEventArgument<Message, EventAttributes>;

/// All attributes that are associated with marks and ranges.
pub type EventAttributes = crate::common::GenericEventAttributes<Category, Message>;

impl EventAttributes {
    pub fn builder() -> crate::common::GenericEventAttributesBuilder<Category, Message> {
        crate::common::GenericEventAttributesBuilder::default()
    }
}

/// Represents a message for use within events and ranges.
///
/// * [`Message::Ascii`] is the discriminator for ASCII C strings
/// * [`Message::Unicode`] is the discriminator for Rust strings and wide C strings
pub type Message = crate::common::GenericMessage<()>;

/// Platform-native types.
pub mod native_types;

/// Support for payload information for Ranges and Marks.
mod payload;
/// Payload type for use with event attributes.
pub use payload::Payload;

/// Support for process-wide ranges.
mod ranges;
/// Process-wide range for use across threads.
pub use ranges::{LocalRange, Range};

/// Support for transparent string types (ASCII or Unicode).
mod str;
/// Transparent string type (ASCII or Unicode).
pub use crate::str::Str;

#[cfg(feature = "tracing")]
/// Support for tracing.
pub mod tracing;

/// Trait used to encode a type to a struct type and value.
pub trait TypeValueEncodable {
    /// The type identifier for the encoded value.
    type Type;
    /// The value to be encoded.
    type Value;

    /// Analyze the current state and yield a type-value tuple.
    fn encode(&self) -> (Self::Type, Self::Value);

    /// Yield a default type-value tuple.
    fn default_encoding() -> (Self::Type, Self::Value);
}

/// Marks an instantaneous event in the application.
///
/// See [`EventArgument`] and [`EventAttributesBuilder`] for usage.
///
/// A marker can contain a text message or specify additional information using the event
/// attributes structure. These attributes include a text message, color, category, and a
/// payload. Each of the attributes is optional.
///
/// ```
/// nvtx::mark("Sample mark");
///
/// nvtx::mark(c"Another example");
///
/// nvtx::mark(
///   nvtx::EventAttributes::builder()
///     .message("Interesting example")
///     .color([255, 0, 0])
///     .build());
/// ```
pub fn mark(argument: impl Into<EventArgument>) {
    match argument.into() {
        EventArgument::Message(Message::Ascii(s)) => nvtx_sys::mark_ascii(&s),
        EventArgument::Message(Message::Unicode(s)) => nvtx_sys::mark_unicode(&s),
        EventArgument::Message(Message::Registered(_)) => {
            unreachable!("Registered strings are not valid in the global context")
        }
        EventArgument::Attributes(a) => nvtx_sys::mark_ex(&a.encode()),
    }
}

/// Name an active thread of the current process.
///
/// See [`Str`] for valid conversions.
///
/// If an invalid thread ID is provided or a thread ID from a different process is used
/// the behavior of the tool is implementation dependent. This requires the OS-specific
/// thread id to be passed in.
///
/// Note: getting the native TID is not necessarily easy. Prefer [`name_current_thread`]
/// if you are trying to name the current thread.
///
/// ```
/// nvtx::name_thread(12345, "My custom name");
/// ```
pub fn name_thread(native_tid: u32, name: impl Into<Str>) {
    match name.into() {
        Str::Ascii(s) => nvtx_sys::name_os_thread_ascii(native_tid, &s),
        Str::Unicode(s) => nvtx_sys::name_os_thread_unicode(native_tid, &s),
    }
}

#[cfg(feature = "name-current-thread")]
/// Name the current thread of the current process.
///
/// See [`Str`] for valid conversions.
///
/// ```
/// nvtx::name_current_thread("Main thread");
/// ```
pub fn name_current_thread(name: impl Into<Str>) {
    name_thread(gettid::gettid() as u32, name);
}

/// Register a new category within the default (global) scope.
///
/// See [`Str`] for valid conversions.
/// ```
/// let cat_a = nvtx::register_category("Category A");
/// ```
pub fn register_category(name: impl Into<Str>) -> Category {
    Category::new(name)
}

/// Register many categories within the default (global) scope.
///
/// See [`Str`] for valid conversions
/// ```
/// let [cat_a, cat_b] = nvtx::register_categories(["Category A", "Category B"]);
/// ```
pub fn register_categories<const C: usize>(names: [impl Into<Str>; C]) -> [Category; C] {
    names.map(register_category)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::common::TestUtils;
    use std::ffi::CString;
    use widestring::WideCString;

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
    fn register_category_test() {
        let cat1 = crate::register_category("category 1");
        let cat2 = crate::register_category("category 2");
        assert_ne!(cat1, cat2);
    }

    #[test]
    fn register_categories() {
        let [cat1, cat2] = crate::register_categories(["category 1", "category 2"]);
        assert_ne!(cat1, cat2);
    }
    #[test]
    fn test_builder_color() {
        let builder = EventAttributes::builder();
        let color = Color::new(0x11, 0x22, 0x44, 0x88);
        let attr = builder.color(color).build();
        assert!(matches!(attr.color, Some(c) if c == color));
    }

    #[test]
    fn test_builder_category() {
        let builder = EventAttributes::builder();
        let cat = register_category("cat");
        let attr = builder.category(cat).build();
        assert!(matches!(attr.category, Some(c) if c == cat));
    }

    #[test]
    fn test_builder_payload() {
        let attr = EventAttributes::builder().payload(1_i32).build();
        assert!(matches!(attr.payload, Some(Payload::Int32(i)) if i == 1_i32));
        let attr = EventAttributes::builder().payload(2_u32).build();
        assert!(matches!(attr.payload, Some(Payload::Uint32(i)) if i == 2_u32));
        let attr = EventAttributes::builder().payload(1_i64).build();
        assert!(matches!(attr.payload, Some(Payload::Int64(i)) if i == 1_i64));
        let attr = EventAttributes::builder().payload(2_u64).build();
        assert!(matches!(attr.payload, Some(Payload::Uint64(i)) if i == 2_u64));
        let attr = EventAttributes::builder().payload(1.0_f32).build();
        assert!(matches!(attr.payload, Some(Payload::Float(i)) if i == 1.0_f32));
        let attr = EventAttributes::builder().payload(2.0_f64).build();
        assert!(matches!(attr.payload, Some(Payload::Double(i)) if i == 2.0_f64));
    }

    #[test]
    fn test_builder_message() {
        let builder = EventAttributes::builder();
        let string = "This is a message";
        let attr = builder.message(string).build();
        assert!(
            matches!(attr.message, Some(Message::Unicode(s)) if s.to_string().unwrap() == string)
        );

        let builder = EventAttributes::builder();
        let cstring = CString::new("This is a message").unwrap();
        let attr = builder.message(cstring.clone()).build();
        assert!(matches!(attr.message, Some(Message::Ascii(s)) if s == cstring));
    }
}
