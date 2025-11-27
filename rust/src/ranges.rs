use std::marker::PhantomData;

use crate::{EventArgument, Message};

/// A RAII-like object for modeling process-wide Ranges.
#[derive(Debug)]
pub struct Range {
    id: nvtx_sys::RangeId,
}

impl Range {
    /// Create an RAII-friendly range type which (1) can be moved across thread
    /// boundaries and (2) automatically ended when dropped.
    ///
    /// ```
    /// // creation from a unicode string
    /// let range = nvtx::Range::new("simple name");
    ///
    /// // creation from a c string (from rust 1.77+)
    /// let range = nvtx::Range::new(c"simple name");
    ///
    /// // creation from EventAttributes
    /// let attr = nvtx::EventAttributes::builder()
    ///     .payload(1)
    ///     .message("complex range")
    ///     .build();
    /// let range = nvtx::Range::new(attr);
    ///
    /// // explicitly end a range
    /// drop(range)
    /// ```
    pub fn new(arg: impl Into<EventArgument>) -> Range {
        let id = match arg.into() {
            EventArgument::Message(Message::Ascii(s)) => nvtx_sys::range_start_ascii(&s),
            EventArgument::Message(Message::Unicode(s)) => nvtx_sys::range_start_unicode(&s),
            EventArgument::Message(Message::Registered(_)) => {
                unreachable!("Registered strings are not valid in the global context")
            }
            EventArgument::Attributes(a) => nvtx_sys::range_start_ex(&a.encode()),
        };
        Range { id }
    }
}

impl Drop for Range {
    fn drop(&mut self) {
        nvtx_sys::range_end(self.id)
    }
}

/// A RAII-like object for modeling callstack (thread-local) Ranges.
#[derive(Debug)]
pub struct LocalRange {
    // prevent Sync + Send
    _phantom: PhantomData<*mut i32>,
}

impl LocalRange {
    /// Create an RAII-friendly range type which (1) cannot be moved across thread
    /// boundaries and (2) automatically ended when dropped.
    ///
    /// ```
    /// // creation from Rust string
    /// let range = nvtx::LocalRange::new("simple name");
    ///
    /// // creation from C string (since 1.77)
    /// let range = nvtx::LocalRange::new(c"simple name");
    ///
    /// // creation from EventAttributes
    /// let attr = nvtx::EventAttributes::builder()
    ///     .payload(1)
    ///     .message("complex range")
    ///     .build();
    /// let range = nvtx::LocalRange::new(attr);
    ///
    /// // explicitly end a range
    /// drop(range)
    /// ```
    pub fn new(arg: impl Into<EventArgument>) -> LocalRange {
        match arg.into() {
            EventArgument::Message(Message::Ascii(s)) => nvtx_sys::range_push_ascii(&s),
            EventArgument::Message(Message::Unicode(s)) => nvtx_sys::range_push_unicode(&s),
            EventArgument::Message(Message::Registered(_)) => {
                unreachable!("Registered strings are not valid in the global context")
            }
            EventArgument::Attributes(a) => nvtx_sys::range_push_ex(&a.encode()),
        };
        LocalRange {
            _phantom: PhantomData,
        }
    }
}

impl Drop for LocalRange {
    fn drop(&mut self) {
        nvtx_sys::range_pop();
    }
}
