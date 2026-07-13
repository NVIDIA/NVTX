// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#![deny(missing_docs)]
#![cfg_attr(docsrs, feature(doc_auto_cfg, doc_cfg))]
#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(test, allow(clippy::expect_used, clippy::unwrap_used))]
#![deny(unsafe_op_in_unsafe_fn)]

//! Crate for interfacing with NVIDIA's NVTX API
//!
//! When not running within Nsight tools, the calls will dispatch to
//! empty method stubs, thus enabling low-overhead profiling.
//!
//! * In `alloc`/`std` profiles, all events are fully supported:
//!   * process ranges [`Range`] and [`domain::Range`]
//!   * thread ranges [`LocalRange`] and [`domain::LocalRange`]
//!   * marks [`mark`] and [`Domain::mark`]
//! * In `alloc`/`std` profiles, naming threads is fully supported
//!   (see [`name_thread`] and [`name_current_thread`]).
//! * In `alloc`/`std` profiles, domain, category, and registered strings are fully supported.
//! * The user-defined synchronization API is implemented.
//! * The user-defined resource naming API is implemented for the following platforms:
//!   * Pthreads (on unix-like platforms)
//!   * CUDA
//!   * CUDA Runtime
//!
//! ## Features
//!
//! The crate supports three capability tiers:
//!
//! * **std** *(default)* -
//!   Full ergonomic API surface, including domain caching and integrations like
//!   [`name_current_thread`] and [`tracing::NvtxLayer`].
//!
//! * **alloc** -
//!   `#![no_std]` + heap-backed API (domains, categories, ranges, and owned strings) without
//!   `std` integrations. Domain caches use a spinlock in this profile; interrupt-driven or
//!   RTOS environments should account for priority inversion risk around domain operations.
//!
//! * **core-only** (`--no-default-features`) -
//!   Strict `#![no_std]` profile with low-level borrowed-string APIs, including
//!   [`mark_ascii`], [`mark_unicode`], [`name_thread_ascii`], [`name_thread_unicode`], and
//!   low-level exports under [`sys`].
//!
//! Additional opt-in features:
//!
//! * **color-name** -
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
//! * **`cuda_runtime`** -
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
//! * **`PThread` Resource Naming** -
//!   `PThreadIdentifier` is added to the [`domain`] module on UNIX-like platforms. This
//!   enables the naming of Pthread-specific entities such as mutexes, semaphores,
//!   condition variables, and read-write-locks.

#[cfg(feature = "alloc")]
extern crate alloc;

mod error;
pub use error::NvtxError;

#[cfg(feature = "alloc")]
mod category;
/// Category for use with marks and ranges.
#[cfg(feature = "alloc")]
pub use category::Category;

/// Common utilities and types shared between global and domain contexts
#[cfg(feature = "alloc")]
mod common;
#[cfg(all(test, feature = "std"))]
pub use common::test_utils;

/// Support for colors.
pub mod color;
/// Color type for controlling appearance within Nsight profilers.
pub type Color = color::Color;

#[cfg(all(feature = "alloc", feature = "cuda"))]
/// Support for CUDA-related APIs.
mod cuda;
#[cfg(all(feature = "alloc", feature = "cuda"))]
pub use cuda::*;

#[cfg(all(feature = "alloc", feature = "cuda_runtime"))]
/// Support for CUDA runtime related APIs.
mod cuda_runtime;
#[cfg(all(feature = "alloc", feature = "cuda_runtime"))]
pub use cuda_runtime::*;

/// Specialized types for use within a domain context.
#[cfg(feature = "alloc")]
pub mod domain;
/// Domain for high-level grouping within Nsight profilers.
#[cfg(feature = "alloc")]
pub type Domain = domain::Domain;

/// Convenience wrapper for all valid argument types to ranges and marks.
///
/// * Any string type will be translated to [`EventArgument::Message`].
/// * If [`EventArgument::Attributes`] is the active discriminator:
///   - If its [`EventAttributes`] only specifies a message, then message will be used.
///   - Otherwise, the existing [`EventAttributes`] will be used for the event.
///
#[cfg(feature = "alloc")]
pub type EventArgument = crate::common::GenericEventArgument<Message, EventAttributes>;

/// All attributes that are associated with marks and ranges.
#[cfg(feature = "alloc")]
pub type EventAttributes = crate::common::GenericEventAttributes<Category, Message>;

#[cfg(feature = "alloc")]
impl EventAttributes {
    pub fn builder() -> crate::common::GenericEventAttributesBuilder<Category, Message> {
        crate::common::GenericEventAttributesBuilder::default()
    }
}

/// Represents a message for use within events and ranges.
///
/// * [`Message::Ascii`] is the discriminator for ASCII C strings
/// * [`Message::Unicode`] is the discriminator for Rust strings and wide C strings
#[cfg(feature = "alloc")]
pub type Message = crate::common::GenericMessage<()>;

#[cfg(feature = "alloc")]
fn strip_registered_message_for_global_context(arg: EventArgument) -> EventArgument {
    match arg {
        EventArgument::Attributes(mut attr) => {
            if matches!(attr.message, Some(Message::Registered(()))) {
                attr.message = None;
            }
            EventArgument::Attributes(attr)
        }
        EventArgument::Message(Message::Registered(())) => {
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

/// Platform-native types.
pub mod native_types;

/// Support for payload information for Ranges and Marks.
mod payload;
/// Payload type for use with event attributes.
pub use payload::Payload;

/// Support for process-wide ranges.
#[cfg(feature = "alloc")]
mod ranges;
/// Process-wide range for use across threads.
#[cfg(feature = "alloc")]
pub use ranges::{LocalRange, Range};

/// Support for transparent string types (ASCII or Unicode).
#[cfg(feature = "alloc")]
mod str;
/// Transparent string type (ASCII or Unicode).
#[cfg(feature = "alloc")]
pub use crate::str::{Str, StrError};

#[cfg(all(feature = "tracing", feature = "std"))]
/// Support for tracing.
pub mod tracing;

/// Low-level NVTX FFI exports.
pub use nvtx_sys as sys;

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

/// Marks an instantaneous event with an ASCII C string.
pub fn mark_ascii(message: &core::ffi::CStr) {
    nvtx_sys::mark_ascii(message);
}

/// Marks an instantaneous event with a Unicode wide C string.
pub fn mark_unicode(message: &widestring::WideCStr) {
    nvtx_sys::mark_unicode(message);
}

/// Marks an instantaneous event in the application.
///
/// See [`EventArgument`] and [`EventAttributesBuilder`] for usage.
///
/// A marker can contain a text message or specify additional information using the event
/// attributes structure. These attributes include a text message, color, category, and a
/// payload. Each of the attributes is optional.
///
/// This helper accepts any value convertible into [`EventArgument`] and returns `()`.
/// Invalid global-context input is not returned or propagated: it triggers a
/// `debug_assert!` in debug builds and is dropped in non-debug builds. Use
/// [`try_mark`] when invalid input must be handled explicitly.
///
/// ```
/// nvtx::mark(c"Sample mark");
///
/// nvtx::mark(c"Another example");
///
/// nvtx::mark(
///   nvtx::EventAttributes::builder()
///     .message(c"Interesting example")
///     .color([255, 0, 0])
///     .build());
/// ```
#[cfg(feature = "alloc")]
pub fn mark(argument: impl Into<EventArgument>) {
    let argument = argument.into();
    match try_mark(argument.clone()) {
        Ok(()) => {}
        Err(error) => {
            debug_assert!(false, "{error}");
            let _ = try_mark(strip_registered_message_for_global_context(argument));
        }
    }
}

/// Fallible variant of [`mark`] that reports invalid global-context usage.
///
/// # Errors
///
/// Returns [`NvtxError::RegisteredStringInGlobalContext`] when `argument`
/// contains a registered-string message variant.
#[cfg(feature = "alloc")]
pub fn try_mark(argument: impl Into<EventArgument>) -> Result<(), NvtxError> {
    match argument.into() {
        EventArgument::Message(Message::Ascii(s)) => mark_ascii(&s),
        EventArgument::Message(Message::Unicode(s)) => mark_unicode(&s),
        EventArgument::Message(Message::Registered(())) => {
            return Err(NvtxError::RegisteredStringInGlobalContext);
        }
        EventArgument::Attributes(a) => {
            if matches!(a.message, Some(Message::Registered(()))) {
                return Err(NvtxError::RegisteredStringInGlobalContext);
            }
            nvtx_sys::mark_ex(&a.encode());
        }
    }
    Ok(())
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
/// nvtx::name_thread(12345, c"My custom name");
/// ```
#[cfg(feature = "alloc")]
pub fn name_thread(native_tid: u32, name: impl Into<Str>) {
    match name.into() {
        Str::Ascii(s) => name_thread_ascii(native_tid, &s),
        Str::Unicode(s) => name_thread_unicode(native_tid, &s),
    }
}

/// Names an active thread with an ASCII C string.
pub fn name_thread_ascii(native_tid: u32, name: &core::ffi::CStr) {
    nvtx_sys::name_os_thread_ascii(native_tid, name);
}

/// Names an active thread with a Unicode wide C string.
pub fn name_thread_unicode(native_tid: u32, name: &widestring::WideCStr) {
    nvtx_sys::name_os_thread_unicode(native_tid, name);
}

#[cfg(all(feature = "name-current-thread", feature = "std"))]
/// Name the current thread of the current process.
///
/// See [`Str`] for valid conversions.
///
/// ```
/// nvtx::name_current_thread(c"Main thread");
/// ```
pub fn name_current_thread(name: impl Into<Str>) {
    let raw_tid = gettid::gettid();
    let Ok(native_tid) = u32::try_from(raw_tid) else {
        debug_assert!(false, "OS thread id {raw_tid} does not fit into u32");
        return;
    };
    name_thread(native_tid, name);
}

/// Register a new category within the default (global) scope.
///
/// See [`Str`] for valid conversions.
/// ```
/// let cat_a = nvtx::register_category(c"Category A");
/// ```
#[cfg(feature = "alloc")]
pub fn register_category(name: impl Into<Str>) -> Category {
    Category::new(name)
}

/// Register many categories within the default (global) scope.
///
/// See [`Str`] for valid conversions
/// ```
/// let [cat_a, cat_b] = nvtx::register_categories([c"Category A", c"Category B"]);
/// ```
#[cfg(feature = "alloc")]
pub fn register_categories<const C: usize>(names: [impl Into<Str>; C]) -> [Category; C] {
    names.map(register_category)
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
        let cat1 = crate::register_category(lossy_str("category 1"));
        let cat2 = crate::register_category(lossy_str("category 2"));
        assert_ne!(cat1, cat2);
    }

    #[test]
    fn register_categories() {
        let [cat1, cat2] =
            crate::register_categories([lossy_str("category 1"), lossy_str("category 2")]);
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
        let cat = register_category(lossy_str("cat"));
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
        let attr = builder.message(lossy_str(string)).build();
        assert!(
            matches!(attr.message, Some(Message::Unicode(s)) if s.to_string().unwrap() == string)
        );

        let builder = EventAttributes::builder();
        let cstring = CString::new("This is a message").unwrap();
        let attr = builder.message(cstring.clone()).build();
        assert!(matches!(attr.message, Some(Message::Ascii(s)) if s == cstring));
    }

    #[test]
    fn test_try_mark_rejects_registered_message() {
        let arg = EventArgument::Message(Message::Registered(()));
        assert!(matches!(
            try_mark(arg),
            Err(NvtxError::RegisteredStringInGlobalContext)
        ));
    }

    #[test]
    fn test_try_mark_rejects_registered_message_in_attributes() {
        let attr = EventAttributes::builder()
            .message(Message::Registered(()))
            .build();
        assert!(matches!(
            try_mark(attr),
            Err(NvtxError::RegisteredStringInGlobalContext)
        ));
    }
}
