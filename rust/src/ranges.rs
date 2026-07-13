// SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use core::marker::PhantomData;

use crate::{strip_registered_message_for_global_context, EventArgument, Message, NvtxError};

fn reject_registered_in_global_context(arg: &EventArgument) -> Result<(), NvtxError> {
    match arg {
        EventArgument::Message(Message::Registered(())) => {
            Err(NvtxError::RegisteredStringInGlobalContext)
        }
        EventArgument::Attributes(a) if matches!(&a.message, Some(Message::Registered(()))) => {
            Err(NvtxError::RegisteredStringInGlobalContext)
        }
        _ => Ok(()),
    }
}

/// A RAII-like object for modeling process-wide Ranges.
#[derive(Debug)]
pub struct Range {
    id: Option<nvtx_sys::RangeId>,
}

impl Range {
    fn start(arg: EventArgument) -> Range {
        let id = match arg {
            EventArgument::Message(Message::Ascii(s)) => nvtx_sys::range_start_ascii(&s),
            EventArgument::Message(Message::Unicode(s)) => nvtx_sys::range_start_unicode(&s),
            EventArgument::Message(Message::Registered(())) => unreachable!("validated above"),
            EventArgument::Attributes(a) => nvtx_sys::range_start_ex(&a.encode()),
        };
        Range { id: Some(id) }
    }

    /// Create an RAII-friendly range type which (1) can be moved across thread
    /// boundaries and (2) automatically ended when dropped.
    ///
    /// ```
    /// // creation from a Rust string, explicitly allowing NUL-stripping
    /// let range = nvtx::Range::new(nvtx::Str::from_str_lossy("simple name"));
    ///
    /// // creation from a c string (from rust 1.77+)
    /// let range = nvtx::Range::new(c"simple name");
    ///
    /// // creation from EventAttributes
    /// let attr = nvtx::EventAttributes::builder()
    ///     .payload(1)
    ///     .message(c"complex range")
    ///     .build();
    /// let range = nvtx::Range::new(attr);
    ///
    /// // explicitly end a range
    /// drop(range)
    /// ```
    pub fn new(arg: impl Into<EventArgument>) -> Range {
        let arg = arg.into();
        match reject_registered_in_global_context(&arg) {
            Ok(()) => Self::start(arg),
            Err(error) => {
                debug_assert!(false, "{error}");
                Self::start(strip_registered_message_for_global_context(arg))
            }
        }
    }

    /// Fallible variant of [`Range::new`] for invalid global-context usage.
    ///
    /// # Errors
    ///
    /// Returns [`NvtxError::RegisteredStringInGlobalContext`] when `arg`
    /// contains a registered-string message variant.
    pub fn try_new(arg: impl Into<EventArgument>) -> Result<Range, NvtxError> {
        let arg = arg.into();
        reject_registered_in_global_context(&arg)?;
        Ok(Self::start(arg))
    }
}

impl Drop for Range {
    fn drop(&mut self) {
        if let Some(id) = self.id {
            nvtx_sys::range_end(id);
        }
    }
}

/// A RAII-like object for modeling callstack (thread-local) Ranges.
#[derive(Debug)]
pub struct LocalRange {
    active: bool,
    // prevent Sync + Send
    _phantom: PhantomData<*mut i32>,
}

impl LocalRange {
    fn push(arg: EventArgument) -> LocalRange {
        match arg {
            EventArgument::Message(Message::Ascii(s)) => nvtx_sys::range_push_ascii(&s),
            EventArgument::Message(Message::Unicode(s)) => nvtx_sys::range_push_unicode(&s),
            EventArgument::Message(Message::Registered(())) => unreachable!("validated above"),
            EventArgument::Attributes(a) => nvtx_sys::range_push_ex(&a.encode()),
        };
        LocalRange {
            active: true,
            _phantom: PhantomData,
        }
    }

    /// Create an RAII-friendly range type which (1) cannot be moved across thread
    /// boundaries and (2) automatically ended when dropped.
    ///
    /// ```
    /// // creation from a Rust string, explicitly allowing NUL-stripping
    /// let range = nvtx::LocalRange::new(nvtx::Str::from_str_lossy("simple name"));
    ///
    /// // creation from C string (since 1.77)
    /// let range = nvtx::LocalRange::new(c"simple name");
    ///
    /// // creation from EventAttributes
    /// let attr = nvtx::EventAttributes::builder()
    ///     .payload(1)
    ///     .message(c"complex range")
    ///     .build();
    /// let range = nvtx::LocalRange::new(attr);
    ///
    /// // explicitly end a range
    /// drop(range)
    /// ```
    pub fn new(arg: impl Into<EventArgument>) -> LocalRange {
        let arg = arg.into();
        match reject_registered_in_global_context(&arg) {
            Ok(()) => Self::push(arg),
            Err(error) => {
                debug_assert!(false, "{error}");
                Self::push(strip_registered_message_for_global_context(arg))
            }
        }
    }

    /// Fallible variant of [`LocalRange::new`] for invalid global-context usage.
    ///
    /// # Errors
    ///
    /// Returns [`NvtxError::RegisteredStringInGlobalContext`] when `arg`
    /// contains a registered-string message variant.
    pub fn try_new(arg: impl Into<EventArgument>) -> Result<LocalRange, NvtxError> {
        let arg = arg.into();
        reject_registered_in_global_context(&arg)?;
        Ok(Self::push(arg))
    }
}

impl Drop for LocalRange {
    fn drop(&mut self) {
        if self.active {
            nvtx_sys::range_pop();
        }
    }
}

#[cfg(all(test, feature = "std"))]
mod tests {
    use super::*;
    use crate::EventAttributes;

    #[test]
    fn range_try_new_rejects_registered_message() {
        let arg = EventArgument::Message(Message::Registered(()));
        assert!(matches!(
            Range::try_new(arg),
            Err(NvtxError::RegisteredStringInGlobalContext)
        ));
    }

    #[test]
    fn local_range_try_new_rejects_registered_message() {
        let arg = EventArgument::Message(Message::Registered(()));
        assert!(matches!(
            LocalRange::try_new(arg),
            Err(NvtxError::RegisteredStringInGlobalContext)
        ));
    }

    #[test]
    fn range_try_new_rejects_registered_message_in_attributes() {
        let attr = EventAttributes::builder()
            .message(Message::Registered(()))
            .build();
        assert!(matches!(
            Range::try_new(attr),
            Err(NvtxError::RegisteredStringInGlobalContext)
        ));
    }

    #[test]
    fn registered_message_attributes_can_be_sanitized_for_global_ranges() {
        let attr = EventAttributes::builder()
            .message(Message::Registered(()))
            .payload(1_i32)
            .build();

        let EventArgument::Attributes(sanitized) =
            strip_registered_message_for_global_context(EventArgument::Attributes(attr))
        else {
            unreachable!("expected sanitized attributes");
        };

        assert!(sanitized.message.is_none());
        assert!(matches!(sanitized.payload, Some(crate::Payload::Int32(1))));
    }

    #[test]
    fn local_range_try_new_rejects_registered_message_in_attributes() {
        let attr = EventAttributes::builder()
            .message(Message::Registered(()))
            .build();
        assert!(matches!(
            LocalRange::try_new(attr),
            Err(NvtxError::RegisteredStringInGlobalContext)
        ));
    }
}
