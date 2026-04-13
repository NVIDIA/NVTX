// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use alloc::{ffi::CString, string::String};
use core::ffi::CStr;
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

/// Convert an owned Rust string into [`Str`].
///
/// This first attempts [`WideCString::from_str`], then removes interior NUL
/// characters and retries. If conversion still fails, debug builds trigger a
/// `debug_assert!`; non-debug builds fall back to an empty [`Str::Ascii`].
/// Interior NULs are removed and malformed input may be lost. Callers that need
/// lossless conversion should use an API that reports conversion errors.
impl From<String> for Str {
    fn from(v: String) -> Self {
        if let Ok(wide) = WideCString::from_str(v.as_str()) {
            Self::Unicode(wide)
        } else {
            // Strip interior NULs so string conversion never panics.
            let sanitized: String = v.chars().filter(|c| *c != '\0').collect();
            if let Ok(wide) = WideCString::from_str(sanitized.as_str()) {
                Self::Unicode(wide)
            } else {
                debug_assert!(
                    false,
                    "WideCString conversion failed even after NUL-stripping: {v:?}"
                );
                // Fallback to an empty ASCII string if wide conversion unexpectedly fails.
                Self::Ascii(CString::default())
            }
        }
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
