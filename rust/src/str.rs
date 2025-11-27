use std::ffi::{CStr, CString};
use widestring::{WideCStr, WideCString};

/// A convenience wrapper for various string types.
///
/// * [`Str::Ascii`] is the discriminator for C string types
/// * [`Str::Unicode`] is the discriminator for Rust string types and C wide string types
#[derive(Debug, Clone)]
pub enum Str {
    /// Represents an ASCII friendly string
    Ascii(CString),
    /// Represents a Unicode string
    Unicode(WideCString),
}

impl From<String> for Str {
    fn from(v: String) -> Self {
        Self::Unicode(WideCString::from_str(v.as_str()).expect("Could not convert to wide string"))
    }
}

impl From<&str> for Str {
    fn from(v: &str) -> Self {
        String::from(v).into()
    }
}

impl From<CString> for Str {
    fn from(v: CString) -> Self {
        Self::Ascii(v)
    }
}

impl From<&CStr> for Str {
    fn from(v: &CStr) -> Self {
        CString::from(v).into()
    }
}

impl From<WideCString> for Str {
    fn from(v: WideCString) -> Self {
        Self::Unicode(v)
    }
}

impl From<&WideCStr> for Str {
    fn from(v: &WideCStr) -> Self {
        WideCString::from(v).into()
    }
}
